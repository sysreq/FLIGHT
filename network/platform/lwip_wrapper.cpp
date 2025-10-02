#include "lwip_wrapper.h"
#include "lwip/tcp.h"
#include "lwip/pbuf.h"
#include "pico/cyw43_arch.h"
#include <cstring>
#include <cstdio>
#include <algorithm>

// Forward declaration - implemented in http_service.cpp
namespace network::services {
    class HttpService;
}

namespace network::platform::lwip {
    // Internal LWIP callbacks for HTTP handling
    namespace {
        err_t OnHttpAccept(void* /*arg*/, tcp_pcb* client_pcb, err_t err);
        err_t OnHttpReceive(void* arg, tcp_pcb* pcb, pbuf* p, err_t err);
        void OnHttpError(void* arg, err_t err);
    }
    std::expected<TcpListenerHandle, ErrorCode> CreateListener(uint16_t port) {
        tcp_pcb* pcb = tcp_new();
        if (!pcb) {
            return std::unexpected(ErrorCode::OutOfMemory);
        }

        err_t err = tcp_bind(pcb, IP_ADDR_ANY, port);
        if (err != ERR_OK) {
            tcp_close(pcb);
            return std::unexpected(ErrorCode::ConnectionFailed);
        }

        tcp_pcb* listener = tcp_listen(pcb);
        if (!listener) {
            tcp_close(pcb);
            return std::unexpected(ErrorCode::OutOfMemory);
        }

        return static_cast<TcpListenerHandle>(listener);
    }

    void DestroyListener(TcpListenerHandle listener) {
        if (listener) {
            tcp_close(static_cast<tcp_pcb*>(listener));
        }
    }

    std::expected<TcpHandle, ErrorCode> AcceptConnection(TcpListenerHandle listener) {
        // Note: LWIP uses callback-based accept, not direct polling
        // This function is designed to be called from accept callback
        // For now, return not available - will be handled via callbacks
        (void)listener;
        return std::unexpected(ErrorCode::NoConnectionAvailable);
    }

    void CloseConnection(TcpHandle handle) {
        if (handle) {
            tcp_pcb* pcb = static_cast<tcp_pcb*>(handle);
            tcp_close(pcb);
        }
    }

    std::expected<size_t, ErrorCode> Send(TcpHandle handle, std::span<const char> data) {
        if (!handle) {
            printf("Send: Invalid handle\n");
            return std::unexpected(ErrorCode::InvalidParameter);
        }

        tcp_pcb* pcb = static_cast<tcp_pcb*>(handle);

        // Check how much space is available in the send buffer
        size_t available = tcp_sndbuf(pcb);
        printf("Send: Requested=%zu, Available=%zu\n", data.size(), available);

        if (available == 0) {
            printf("Send: No buffer space (will rely on callback)\n");
            return std::unexpected(ErrorCode::SendFailed);
        }

        // Send only what fits in the buffer
        size_t to_send = (data.size() < available) ? data.size() : available;

        err_t err = tcp_write(pcb, data.data(), to_send, TCP_WRITE_FLAG_COPY);
        if (err != ERR_OK) {
            printf("Send: tcp_write failed with error %d\n", err);
            return std::unexpected(ErrorCode::SendFailed);
        }

        // Trigger output
        tcp_output(pcb);

        printf("Send: Enqueued %zu bytes (callback will handle remainder)\n", to_send);
        return to_send;
    }

    std::expected<size_t, ErrorCode> Receive(TcpHandle handle, std::span<char> buffer) {
        // Note: LWIP uses callback-based receive, not direct polling
        // This function would be called from receive callback with pbuf data
        // For now, return not available - will be handled via callbacks
        (void)handle;
        (void)buffer;
        return std::unexpected(ErrorCode::ReceiveFailed);
    }

    void Poll() {
        cyw43_arch_poll();
    }

    void SetupHttpListener(TcpListenerHandle listener) {
        if (!listener) return;

        tcp_pcb* listener_pcb = static_cast<tcp_pcb*>(listener);
        tcp_arg(listener_pcb, nullptr);
        tcp_accept(listener_pcb, OnHttpAccept);
    }

    // Implementation of internal callbacks
    namespace {
        // Forward declaration
        err_t OnHttpSent(void* arg, tcp_pcb* pcb, u16_t len);

        err_t OnHttpAccept(void* /*arg*/, tcp_pcb* client_pcb, err_t err) {
            if (err != ERR_OK || !client_pcb) {
                return ERR_VAL;
            }

            // Acquire a connection from the pool
            Connection* conn = AcquireConnection();
            if (!conn) {
                printf("HTTP: No connection slots available\n");
                tcp_close(client_pcb);
                return ERR_MEM;
            }

            // Store the TCP handle
            conn->tcp_handle = static_cast<TcpHandle>(client_pcb);
            conn->request_length = 0;
            conn->response_length = 0;
            conn->response_sent = 0;

            // Set up LWIP callbacks with connection as argument
            tcp_arg(client_pcb, conn);
            tcp_recv(client_pcb, OnHttpReceive);
            tcp_err(client_pcb, OnHttpError);
            tcp_sent(client_pcb, OnHttpSent);

            return ERR_OK;
        }

        err_t OnHttpReceive(void* arg, tcp_pcb* pcb, pbuf* p, err_t err) {
            Connection* c = static_cast<Connection*>(arg);

            if (!p) {
                // Connection closed by client
                ReleaseConnection(c);
                tcp_close(pcb);
                return ERR_OK;
            }

            if (err != ERR_OK) {
                pbuf_free(p);
                ReleaseConnection(c);
                tcp_close(pcb);
                return err;
            }

            // Copy data to request buffer
            size_t available = c->request_buffer.size() - c->request_length;
            size_t to_copy = std::min(static_cast<size_t>(p->tot_len), available);

            if (to_copy > 0) {
                pbuf_copy_partial(p, c->request_buffer.data() + c->request_length, to_copy, 0);
                c->request_length += to_copy;
                tcp_recved(pcb, to_copy);
            }

            pbuf_free(p);

            // Check if we have a complete HTTP request (ends with \r\n\r\n)
            if (c->request_length >= 4) {
                const char* buf = c->request_buffer.data();
                for (size_t i = 0; i <= c->request_length - 4; ++i) {
                    if (buf[i] == '\r' && buf[i+1] == '\n' &&
                        buf[i+2] == '\r' && buf[i+3] == '\n') {
                        // Complete request received - process it
                        ProcessHttpRequest(*c);

                        // Send the response if any was generated
                        if (c->response_length > 0) {
                            size_t available = tcp_sndbuf(pcb);
                            printf("HTTP: Response ready: %zu bytes, buffer available: %zu\n",
                                   c->response_length, available);

                            if (available == 0) {
                                printf("HTTP: No buffer space, will wait for callback\n");
                                c->response_sent = 0;
                                // tcp_sent callback will handle sending when space is available
                                return ERR_OK;
                            }

                            size_t to_send = (c->response_length < available) ? c->response_length : available;

                            err_t send_err = tcp_write(pcb, c->response_buffer.data(), to_send, TCP_WRITE_FLAG_COPY);
                            if (send_err == ERR_OK) {
                                c->response_sent = to_send;
                                tcp_output(pcb);
                                printf("HTTP: Sent %zu/%zu bytes (callback will handle rest)\n",
                                       to_send, c->response_length);

                                // If all data sent, close immediately
                                if (c->response_sent >= c->response_length) {
                                    ReleaseConnection(c);
                                    tcp_close(pcb);
                                }
                                // Otherwise, tcp_sent callback will handle the rest
                            } else {
                                printf("HTTP: Initial send failed with error %d (available was %zu, tried %zu)\n",
                                       send_err, available, to_send);
                                ReleaseConnection(c);
                                tcp_close(pcb);
                            }
                        } else {
                            // No response, just close
                            ReleaseConnection(c);
                            tcp_close(pcb);
                        }
                        return ERR_OK;
                    }
                }
            }

            return ERR_OK;
        }

        void OnHttpError(void* arg, err_t /*err*/) {
            Connection* c = static_cast<Connection*>(arg);
            if (c) {
                ReleaseConnection(c);
            }
        }

        err_t OnHttpSent(void* arg, tcp_pcb* pcb, u16_t len) {
            Connection* c = static_cast<Connection*>(arg);
            if (!c) {
                return ERR_OK;
            }

            printf("HTTP: Sent callback - %u bytes acknowledged\n", len);

            // Check if there's more data to send
            if (c->response_sent < c->response_length) {
                size_t remaining = c->response_length - c->response_sent;
                size_t available = tcp_sndbuf(pcb);
                size_t to_send = (remaining < available) ? remaining : available;

                printf("HTTP: Sending next chunk: %zu bytes (sent so far: %zu/%zu)\n",
                       to_send, c->response_sent, c->response_length);

                if (to_send > 0) {
                    err_t err = tcp_write(pcb, c->response_buffer.data() + c->response_sent,
                                         to_send, TCP_WRITE_FLAG_COPY);
                    if (err == ERR_OK) {
                        c->response_sent += to_send;
                        tcp_output(pcb);

                        // If all done, close connection
                        if (c->response_sent >= c->response_length) {
                            printf("HTTP: All data sent, closing connection\n");
                            ReleaseConnection(c);
                            tcp_close(pcb);
                        }
                    } else {
                        printf("HTTP: Follow-up send failed with error %d\n", err);
                        ReleaseConnection(c);
                        tcp_close(pcb);
                    }
                }
            } else {
                // All data sent, close connection
                printf("HTTP: All data confirmed sent, closing\n");
                ReleaseConnection(c);
                tcp_close(pcb);
            }

            return ERR_OK;
        }
    }

    // Weak symbol - will be overridden by http_service.cpp
    __attribute__((weak)) void ProcessHttpRequest(Connection& conn) {
        // Default implementation - send 501 Not Implemented
        const char* response =
            "HTTP/1.1 501 Not Implemented\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 15\r\n"
            "\r\n"
            "Not Implemented";

        size_t len = strlen(response);
        if (len <= conn.response_buffer.size()) {
            std::memcpy(conn.response_buffer.data(), response, len);
            conn.response_length = len;
        }
    }
}
