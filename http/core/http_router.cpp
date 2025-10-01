#include "http_router.h"
#include "../http_platform.h"
#include "http_utils.h"
#include "index\index_handler.h"
#include "index\status_handler.h"
#include <cstring>
#include <cstdio>

namespace http::core {

size_t HttpRequestRouter::route_request(const char* request, size_t len,
                                        char* response_buffer, size_t buffer_size) {
    HttpRequest req = parse_request(request, len);
    HttpResponse resp = { response_buffer, buffer_size, 0 };

    for (size_t i = 0; i < route_count_; ++i) {
        if (routes_[i].matches(req.path, req.method)) {
            return routes_[i].handler(req, resp, routes_[i].context);
        }
    }

    return handle_not_found(req, resp);
}

bool HttpRequestRouter::register_route(const char* path, const char* method,
                                      RouteHandler handler, void* context) {
    if (route_count_ >= MAX_ROUTES) {
        printf("Router: Cannot register route %s %s - registry full\n", method, path);
        return false;
    }

    routes_[route_count_++] = Route(path, method, handler, context);
    printf("Router: Registered route %s %s\n", method, path);
    return true;
}

HttpRequest HttpRequestRouter::parse_request(const char* request, size_t len) {
    HttpRequest req = {};
    req.full_request = request;
    req.request_len = len;
    
    if (std::strncmp(request, "GET", 3) == 0) {
        req.method = "GET";
        
        static char path_buffer[constants::PATH_BUFFER_SIZE];
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

void HttpRequestRouter::initialize_event_system() {
    event_dispatcher_.register_handler(EventType::TimerStart,
                                      TimerEventHandler::handle_event,
                                      &timer_handler_);
    event_dispatcher_.register_handler(EventType::TimerStop,
                                      TimerEventHandler::handle_event,
                                      &timer_handler_);
    event_dispatcher_.register_handler(EventType::TelemetryUpdate,
                                      TelemetryEventHandler::handle_event,
                                      &telemetry_handler_);
}

void HttpRequestRouter::register_default_routes() {
    static IndexHandlerContext index_ctx;
    index_ctx.event_queue = event_handler_;
    index_ctx.event_dispatcher = &event_dispatcher_;
    index_ctx.start_time = &start_time_;
    index_ctx.telemetry_handler = &telemetry_handler_;

    static StatusHandlerContext status_ctx;
    status_ctx.timer_handler = &timer_handler_;
    status_ctx.telemetry_handler = &telemetry_handler_;

    register_route("/", "GET", IndexHandler::handle, &index_ctx);
    register_route("/index", "GET", IndexHandler::handle, &index_ctx);
    register_route("/status", "GET", StatusHandler::handle, &status_ctx);
}

size_t HttpRequestRouter::handle_not_found(const HttpRequest& req, HttpResponse& resp) {
    return generate_error_404(resp);
}

} // namespace http::core