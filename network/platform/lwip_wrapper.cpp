#include "lwip_wrapper.h"
#include "connection.h"
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
        size_t available = tcp_sndbuf(pcb);

        if (available == 0) {
            return std::unexpected(ErrorCode::SendFailed);
        }

        size_t to_send = (data.size() < available) ? data.size() : available;
        err_t err = tcp_write(pcb, data.data(), to_send, TCP_WRITE_FLAG_COPY);

        if (err != ERR_OK) {
            printf("HTTP: Send failed with error %d\n", err);
            return std::unexpected(ErrorCode::SendFailed);
        }

        tcp_output(pcb);
        return to_send;
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

            Connection* conn = Connection::Acquire();
            if (!conn) {
                printf("HTTP: No connection slots available\n");
                tcp_close(client_pcb);
                return ERR_MEM;
            }

            conn->SetTcpHandle(static_cast<TcpHandle>(client_pcb));

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
                Connection::Release(c);
                tcp_close(pcb);
                return ERR_OK;
            }

            if (err != ERR_OK) {
                pbuf_free(p);
                Connection::Release(c);
                tcp_close(pcb);
                return err;
            }

            // Check if request buffer would overflow
            size_t available = c->GetRequestCapacity() - c->GetRequestLength();
            size_t to_copy = std::min(static_cast<size_t>(p->tot_len), available);

            if (to_copy == 0) {
                // Request too large, reject it
                pbuf_free(p);
                Connection::Release(c);
                tcp_close(pcb);
                return ERR_MEM;
            }

            if (to_copy > 0) {
                pbuf_copy_partial(p, c->GetRequestBuffer() + c->GetRequestLength(), to_copy, 0);
                c->SetRequestLength(c->GetRequestLength() + to_copy);
                tcp_recved(pcb, to_copy);
            }

            pbuf_free(p);

            // Check if we have a complete HTTP request (ends with \r\n\r\n)
            if (c->GetRequestLength() >= 4) {
                const char* buf = c->GetRequestBuffer();
                for (size_t i = 0; i <= c->GetRequestLength() - 4; ++i) {
                    if (buf[i] == '\r' && buf[i+1] == '\n' &&
                        buf[i+2] == '\r' && buf[i+3] == '\n') {
                        // Complete request received - process it
                        ProcessHttpRequest(*c);

                        // Send the response if any was generated
                        if (c->GetResponseLength() > 0) {
                            size_t available = tcp_sndbuf(pcb);

                            if (available == 0) {
                                c->SetResponseSent(0);
                                return ERR_OK;
                            }

                            size_t to_send = std::min(c->GetResponseLength(), available);
                            err_t send_err = tcp_write(pcb, c->GetResponseBuffer(), to_send, TCP_WRITE_FLAG_COPY);

                            if (send_err == ERR_OK) {
                                c->SetResponseSent(to_send);
                                tcp_output(pcb);

                                if (c->IsResponseComplete()) {
                                    Connection::Release(c);
                                    tcp_close(pcb);
                                }
                            } else {
                                printf("HTTP: Send failed with error %d\n", send_err);
                                Connection::Release(c);
                                tcp_close(pcb);
                            }
                        } else {
                            Connection::Release(c);
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
                Connection::Release(c);
            }
        }

        err_t OnHttpSent(void* arg, tcp_pcb* pcb, u16_t /*len*/) {
            Connection* c = static_cast<Connection*>(arg);
            if (!c) {
                return ERR_OK;
            }

            // Check if there's more data to send
            if (!c->IsResponseComplete()) {
                size_t remaining = c->GetResponseLength() - c->GetResponseSent();
                size_t available = tcp_sndbuf(pcb);
                size_t to_send = std::min(remaining, available);

                if (to_send > 0) {
                    err_t err = tcp_write(pcb, c->GetResponseBuffer() + c->GetResponseSent(),
                                         to_send, TCP_WRITE_FLAG_COPY);
                    if (err == ERR_OK) {
                        c->IncrementResponseSent(to_send);
                        tcp_output(pcb);

                        if (c->IsResponseComplete()) {
                            Connection::Release(c);
                            tcp_close(pcb);
                        }
                    } else {
                        printf("HTTP: Send continuation failed with error %d\n", err);
                        Connection::Release(c);
                        tcp_close(pcb);
                    }
                }
            } else {
                Connection::Release(c);
                tcp_close(pcb);
            }

            return ERR_OK;
        }
    }

    // Weak symbol - will be overridden by http_service.cpp
    __attribute__((weak)) void ProcessHttpRequest(Connection& conn) {
        const char* response =
            "HTTP/1.1 501 Not Implemented\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 15\r\n"
            "\r\n"
            "Not Implemented";
        conn.SafeWriteResponse(response, strlen(response));
    }
}
