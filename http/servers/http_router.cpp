#include "http_router.h"

namespace http::servers {

size_t HttpRequestRouter::route_request(const char* request, size_t len,
                                        char* response_buffer, size_t buffer_size) {
    HttpRequest req = parse_request(request, len);
    HttpResponse resp = { response_buffer, buffer_size, 0 };
    
    // Route based on path
    if (std::strcmp(req.path, "/") == 0 || std::strcmp(req.path, "/index") == 0) {
        return handle_index(req, resp);
    } else if (std::strcmp(req.path, "/status") == 0) {
        return handle_status(req, resp);
    } else {
        return handle_not_found(req, resp);
    }
}

HttpRequest HttpRequestRouter::parse_request(const char* request, size_t len) {
    HttpRequest req = {};
    req.full_request = request;
    req.request_len = len;
    
    if (std::strncmp(request, "GET", 3) == 0) {
        req.method = "GET";
        
        static char path_buffer[128];
        req.path = extract_path(request + 4, path_buffer, sizeof(path_buffer));
        
        const char* query = std::strchr(request, '?');
        if (query && query < request + len) {
            req.query_string = query + 1;
        }
    }
    
    return req;
}

const char* HttpRequestRouter::extract_path(const char* request, char* path_buffer, size_t buffer_size) {
    while (*request == ' ') request++;
    
    const char* end = request;
    while (*end && *end != ' ' && *end != '?' && *end != '\r' && *end != '\n') {
        end++;
    }
    
    size_t path_len = std::min(static_cast<size_t>(end - request), buffer_size - 1);
    std::memcpy(path_buffer, request, path_len);
    path_buffer[path_len] = '\0';
    
    if (path_len == 0) {
        std::strcpy(path_buffer, "/");
    }
    
    return path_buffer;
}

size_t HttpRequestRouter::handle_index(const HttpRequest& req, HttpResponse& resp) {
    if (req.query_string) {
        Event event = event_handler_->parse_event(req.full_request);
        if (!event.name.empty()) {
            status_handler_.handle_event(event);
            event_handler_->process_request(req.full_request, req.request_len);
        }
    }
    
    uint64_t uptime_s = absolute_time_diff_us(start_time_, get_absolute_time()) / 1000000;
    return HttpGenerator::generate_response(
        resp.buffer,
        resp.buffer_size,
        uptime_s,
        event_handler_->queue_size()
    );
}

size_t HttpRequestRouter::handle_status(const HttpRequest& req, HttpResponse& resp) {
    return status_handler_.generate_http_status_response(resp.buffer, resp.buffer_size);
}

size_t HttpRequestRouter::handle_not_found(const HttpRequest& req, HttpResponse& resp) {
    const char* error_404 = 
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 9\r\n"
        "Connection: close\r\n"
        "\r\n"
        "Not Found";
    
    size_t len = std::strlen(error_404);
    return resp.write(error_404, len);
}

} // namespace http::servers