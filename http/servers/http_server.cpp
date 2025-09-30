#include "http_server.h"

extern "C" {
static err_t tcp_server_accept_callback(void* arg, struct tcp_pcb* client_pcb, err_t err) {
    auto* server = static_cast<http::servers::HttpServer*>(arg);
    return server->handle_accept(client_pcb, err);
}

static err_t tcp_server_recv_callback(void* arg, struct tcp_pcb* pcb, struct pbuf* p, err_t err) {
    auto* conn = static_cast<http::servers::HttpServer::Connection*>(arg);
    return conn->server->handle_recv(pcb, p, err, conn);
}

static err_t tcp_server_sent_callback(void* arg, struct tcp_pcb* pcb, u16_t len) {
    auto* conn = static_cast<http::servers::HttpServer::Connection*>(arg);
    return conn->server->handle_sent(pcb, len, conn);
}

static void tcp_server_err_callback(void* arg, err_t err) {
    auto* conn = static_cast<http::servers::HttpServer::Connection*>(arg);
    conn->server->handle_error(err, conn);
}
} // extern "C"

namespace http::servers {

HttpServer::~HttpServer() {
    stop();
}

bool HttpServer::start(const ip_addr_t* ip, absolute_time_t start_time) {
    if (server_pcb_ != nullptr) {
        return false;
    }
    
    ip_addr_copy(ip_, *ip);
    
    // Initialize components
    event_handler_.initialize();
    router_.set_start_time(start_time);
    
    // Clear connection pool
    for (auto& conn : connections_) {
        conn.in_use = false;
        conn.server = this;
    }
    
    // Create TCP PCB
    struct tcp_pcb* pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
    if (pcb == nullptr) {
        printf("HTTP: Failed to create PCB\n");
        return false;
    }
    
    // Bind to port
    err_t err = tcp_bind(pcb, IP_ANY_TYPE, config::http::SERVER_PORT);
    if (err != ERR_OK) {
        tcp_close(pcb);
        printf("HTTP: Failed to bind to port %d\n", config::http::SERVER_PORT);
        return false;
    }
    
    // Start listening
    server_pcb_ = tcp_listen(pcb);
    if (server_pcb_ == nullptr) {
        tcp_close(pcb);
        printf("HTTP: Failed to listen\n");
        return false;
    }
    
    tcp_arg(server_pcb_, this);
    tcp_accept(server_pcb_, tcp_server_accept_callback);
    
    printf("HTTP server listening on port %d\n", config::http::SERVER_PORT);
    return true;
}

void HttpServer::stop() {
    if (server_pcb_ != nullptr) {
        tcp_close(server_pcb_);
        server_pcb_ = nullptr;
    }
    
    // Close all active connections
    for (auto& conn : connections_) {
        if (conn.in_use && conn.pcb != nullptr) {
            tcp_abort(conn.pcb);
            conn.in_use = false;
        }
    }
}

err_t HttpServer::handle_accept(struct tcp_pcb* client_pcb, err_t err) {
    if (err != ERR_OK || client_pcb == nullptr) {
        return ERR_VAL;
    }
    
    Connection* conn = allocate_connection();
    if (conn == nullptr) {
        tcp_abort(client_pcb);
        return ERR_MEM;
    }
    
    conn->pcb = client_pcb;
    
    tcp_arg(client_pcb, conn);
    tcp_sent(client_pcb, tcp_server_sent_callback);
    tcp_recv(client_pcb, tcp_server_recv_callback);
    tcp_err(client_pcb, tcp_server_err_callback);
    
    return ERR_OK;
}

err_t HttpServer::handle_recv(struct tcp_pcb* pcb, struct pbuf* p, err_t err, void* connection) {
    auto* conn = static_cast<Connection*>(connection);
    
    if (p == nullptr) {
        return close_connection(conn, pcb, ERR_OK);
    }
    
    struct PbufGuard {
        struct ::pbuf* p;
        ~PbufGuard() { if (p) pbuf_free(p); }
    } pbuf_guard{p};
    
    if (p->tot_len > 0) {
        // Extract request
        char request[256];
        size_t req_len = std::min(static_cast<size_t>(p->tot_len), sizeof(request)-1);
        pbuf_copy_partial(p, request, req_len, 0);
        request[req_len] = 0;
        
        // Process request using router
        if (std::strncmp(request, "GET", 3) == 0) {
            process_request(conn, request, req_len);
            
            // Send response
            conn->sent_len = 0;
            err_t write_err = tcp_write(pcb, conn->response, conn->response_len, 0);
            if (write_err != ERR_OK) {
                tcp_recved(pcb, p->tot_len);
                return close_connection(conn, pcb, write_err);
            }
        }
        tcp_recved(pcb, p->tot_len);
    }
    
    return ERR_OK;
}

err_t HttpServer::handle_sent(struct tcp_pcb* pcb, u16_t len, void* connection) {
    auto* conn = static_cast<Connection*>(connection);
    conn->sent_len += len;
    
    if (conn->sent_len >= conn->response_len) {
        return close_connection(conn, pcb, ERR_OK);
    }
    
    return ERR_OK;
}

void HttpServer::handle_error(err_t err, void* connection) {
    auto* conn = static_cast<Connection*>(connection);
    if (err != ERR_ABRT) {
        close_connection(conn, conn->pcb, err);
    } else {
        free_connection(conn);
    }
}

void HttpServer::process_request(HttpServer::Connection* conn, const char* request, size_t len) {
    // Delegate all routing logic to the router
    conn->response_len = router_.route_request(
        request, len,
        conn->response, sizeof(conn->response)
    );
}

HttpServer::Connection* HttpServer::allocate_connection() {
    for (auto& conn : connections_) {
        if (!conn.in_use) {
            conn.in_use = true;
            conn.sent_len = 0;
            conn.response_len = 0;
            return &conn;
        }
    }
    return nullptr;
}

void HttpServer::free_connection(HttpServer::Connection* conn) {
    if (conn != nullptr) {
        conn->in_use = false;
        conn->pcb = nullptr;
    }
}

err_t HttpServer::close_connection(HttpServer::Connection* conn, struct tcp_pcb* pcb, err_t close_err) {
    if (pcb != nullptr) {
        tcp_arg(pcb, nullptr);
        tcp_poll(pcb, nullptr, 0);
        tcp_sent(pcb, nullptr);
        tcp_recv(pcb, nullptr);
        tcp_err(pcb, nullptr);
        
        err_t err = tcp_close(pcb);
        if (err != ERR_OK) {
            tcp_abort(pcb);
            close_err = ERR_ABRT;
        }
    }
    
    free_connection(conn);
    return close_err;
}

} // namespace http::servers