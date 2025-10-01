#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include "../config/http_config.h"
#include "http_events.h"
#include "http_router.h"

// Forward declare LWIP types for pointers/parameters
struct tcp_pcb;
struct pbuf;
typedef signed char err_t;
typedef unsigned short u16_t;

// Need full type for member variable
#include "lwip/ip_addr.h"
#include "pico/time.h"

namespace http::core {

class HttpServer {
public:    
    struct Connection {
        struct tcp_pcb* pcb = nullptr;
        char response[config::http::RESPONSE_BUFFER_SIZE]{};
        int response_len = 0;
        int sent_len = 0;
        bool in_use = false;
        HttpServer* server = nullptr;
    };
    
    HttpServer() : router_(event_handler_) {}
    ~HttpServer();
    
    bool start(const ip_addr_t* ip, absolute_time_t start_time);
    void stop();
    bool is_running() const { return server_pcb_ != nullptr; }

    err_t handle_accept(struct tcp_pcb* client_pcb, err_t err);
    err_t handle_recv(struct tcp_pcb* pcb, struct pbuf* p, err_t err, void* connection);
    err_t handle_sent(struct tcp_pcb* pcb, u16_t len, void* connection);
    void handle_error(err_t err, void* connection);

    HttpEventHandler& event_handler() { return event_handler_; }
    const HttpEventHandler& event_handler() const { return event_handler_; }

    TimerEventHandler& status_handler() { return router_.timer_handler(); }
    TelemetryEventHandler& telemetry_handler() { return router_.telemetry_handler(); }
    TimerEventHandler& timer_handler() { return router_.timer_handler(); }
    EventDispatcher& event_dispatcher() { return router_.event_dispatcher(); }

    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;
    HttpServer(HttpServer&&) = delete;
    HttpServer& operator=(HttpServer&&) = delete;

private:
    struct tcp_pcb* server_pcb_ = nullptr;
    ip_addr_t ip_{};

    HttpEventHandler event_handler_;
    HttpRequestRouter router_;

    std::array<Connection, config::http::MAX_CONNECTIONS> connections_{};

    Connection* allocate_connection();
    void free_connection(Connection* conn);
    err_t close_connection(Connection* conn, struct tcp_pcb* pcb, err_t close_err);

    void process_request(Connection* conn, const char* request, size_t len);
};

} // namespace http::core