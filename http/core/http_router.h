#pragma once

#include <array>
#include <cstddef>
#include "http_types.h"
#include "http_utils.h"
#include "http_events.h"
#include "events/event_dispatcher.h"
#include "events/timer_event_handler.h"
#include "events/telemetry_event_handler.h"

// Need full type for member variable
#include "pico/time.h"

namespace http::core {

class HttpServer;

class HttpRequestRouter {
public:
    static constexpr size_t MAX_ROUTES = constants::MAX_ROUTES;

    HttpRequestRouter() = default;

    void initialize(HttpEventHandler& event_handler) {
        event_handler_ = &event_handler;
    }

    explicit HttpRequestRouter(HttpEventHandler& event_handler)
        : event_handler_(&event_handler) {
        initialize_event_system();
        register_default_routes();
    }

    void set_start_time(absolute_time_t start_time) {
        start_time_ = start_time;
    }

    size_t route_request(const char* request, size_t len,
                        char* response_buffer, size_t buffer_size);

    bool register_route(const char* path, const char* method,
                       RouteHandler handler, void* context = nullptr);

    EventDispatcher& event_dispatcher() { return event_dispatcher_; }
    TimerEventHandler& timer_handler() { return timer_handler_; }
    TelemetryEventHandler& telemetry_handler() { return telemetry_handler_; }
    TimerEventHandler& status_handler() { return timer_handler_; }

private:
    HttpEventHandler* event_handler_ = nullptr;
    absolute_time_t start_time_{};

    EventDispatcher event_dispatcher_;
    TimerEventHandler timer_handler_;
    TelemetryEventHandler telemetry_handler_;

    std::array<Route, MAX_ROUTES> routes_;
    size_t route_count_ = 0;

    HttpRequest parse_request(const char* request, size_t len);
    static const char* extract_path(const char* request, char* path_buffer, size_t buffer_size);

    size_t handle_not_found(const HttpRequest& req, HttpResponse& resp);

    void initialize_event_system();
    void register_default_routes();
};

} // namespace http::core