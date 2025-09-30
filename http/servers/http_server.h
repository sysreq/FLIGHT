#pragma once

#include "..\config\http_config.h"
#include "http_events.h"
#include "http_router.h"

namespace http::servers {

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
    
    // Callback handlers (must be public for C callbacks)
    err_t handle_accept(struct tcp_pcb* client_pcb, err_t err);
    err_t handle_recv(struct tcp_pcb* pcb, struct pbuf* p, err_t err, void* connection);
    err_t handle_sent(struct tcp_pcb* pcb, u16_t len, void* connection);
    void handle_error(err_t err, void* connection);

    // Access to handlers
    HttpEventHandler& event_handler() { return event_handler_; }
    const HttpEventHandler& event_handler() const { return event_handler_; }
    
    // Access to status (if needed externally)
    HttpStatusHandler& status_handler() { return router_.status_handler(); }
    
    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;
    HttpServer(HttpServer&&) = delete;
    HttpServer& operator=(HttpServer&&) = delete;

private:
    struct tcp_pcb* server_pcb_ = nullptr;
    ip_addr_t ip_{};
    
    // Core components - statically allocated
    HttpEventHandler event_handler_;
    HttpRequestRouter router_;
    
    // Connection pool
    std::array<Connection, config::http::MAX_CONNECTIONS> connections_{};
    
    // Connection management
    Connection* allocate_connection();
    void free_connection(Connection* conn);
    err_t close_connection(Connection* conn, struct tcp_pcb* pcb, err_t close_err);
    
    // Request processing (delegates to router)
    void process_request(Connection* conn, const char* request, size_t len);
};

} // namespace http::servers