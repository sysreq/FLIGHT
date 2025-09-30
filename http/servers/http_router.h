#pragma once

#include "..\config\http_config.h"
#include "http_events.h"
#include "http_status.h"
#include "..\ui\http_generator.h"

namespace http::servers {

class HttpServer;

struct HttpRequest {
    const char* method;
    const char* path;
    const char* query_string;
    const char* full_request;
    size_t request_len;
};

struct HttpResponse {
    char* buffer;
    size_t buffer_size;
    size_t content_length;
    
    size_t write(const char* data, size_t len) {
        size_t to_write = std::min(len, buffer_size - content_length);
        std::memcpy(buffer + content_length, data, to_write);
        content_length += to_write;
        return to_write;
    }
};

class HttpRequestRouter {
public:
    HttpRequestRouter() = default;
    
    void initialize(HttpEventHandler& event_handler) {
        event_handler_ = &event_handler;
    }
    
    explicit HttpRequestRouter(HttpEventHandler& event_handler)
        : event_handler_(&event_handler) {}
    
    void set_start_time(absolute_time_t start_time) {
        start_time_ = start_time;
    }
    
    size_t route_request(const char* request, size_t len, 
                        char* response_buffer, size_t buffer_size);
    
    HttpStatusHandler& status_handler() { return status_handler_; }

private:
    HttpEventHandler* event_handler_ = nullptr;
    HttpStatusHandler status_handler_;
    absolute_time_t start_time_{};

    HttpRequest parse_request(const char* request, size_t len);

    size_t handle_index(const HttpRequest& req, HttpResponse& resp);
    size_t handle_status(const HttpRequest& req, HttpResponse& resp);
    size_t handle_not_found(const HttpRequest& req, HttpResponse& resp);
    
    static const char* extract_path(const char* request, char* path_buffer, size_t buffer_size);
};

} // namespace http::servers