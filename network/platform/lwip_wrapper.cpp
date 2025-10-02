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
            return std::unexpected(ErrorCode::InvalidParameter);
        }

        tcp_pcb* pcb = static_cast<tcp_pcb*>(handle);

        // Check available space in send buffer
        size_t available = tcp_sndbuf(pcb);
        if (available == 0) {
            return std::unexpected(ErrorCode::SendFailed);
        }

        // Send only what fits in buffer
        size_t to_send = (data.size() < available) ? data.size() : available;

        err_t err = tcp_write(pcb, data.data(), to_send, TCP_WRITE_FLAG_COPY);
        if (err != ERR_OK) {
            return std::unexpected(ErrorCode::SendFailed);
        }

        // Flush the data
        err = tcp_output(pcb);
        if (err != ERR_OK) {
            return std::unexpected(ErrorCode::SendFailed);
        }

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

            // Set up LWIP callbacks with connection as argument
            tcp_arg(client_pcb, conn);
            tcp_recv(client_pcb, OnHttpReceive);
            tcp_err(client_pcb, OnHttpError);

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
                            std::span<const char> data(c->response_buffer.data(), c->response_length);
                            auto result = Send(c->tcp_handle, data);
                            if (!result) {
                                printf("HTTP: Send failed\n");
                            }
                        }

                        // Close connection after response
                        ReleaseConnection(c);
                        tcp_close(pcb);
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
