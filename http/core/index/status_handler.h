#pragma once

#include "..\http_types.h"
#include "..\http_utils.h"
#include "..\events\timer_event_handler.h"
#include "..\events\telemetry_event_handler.h"

namespace http::core {

struct StatusHandlerContext {
    TimerEventHandler* timer_handler;
    TelemetryEventHandler* telemetry_handler;
};

class StatusHandler {
public:
    static size_t handle(const HttpRequest& req, HttpResponse& resp, void* context) {
        auto* ctx = static_cast<StatusHandlerContext*>(context);
        if (!ctx || !ctx->timer_handler || !ctx->telemetry_handler) {
            return generate_error(resp);
        }

        char json[constants::JSON_BUFFER_SIZE];
        size_t json_len = generate_json(
            json, sizeof(json),
            ctx->timer_handler,
            ctx->telemetry_handler
        );

        char header[constants::HEADER_BUFFER_SIZE];
        int header_len = snprintf(header, sizeof(header),
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: %zu\r\n"
            "Content-Type: application/json\r\n"
            "Cache-Control: no-cache, no-store, must-revalidate\r\n"
            "Connection: close\r\n"
            "\r\n",
            json_len
        );

        resp.write(header, header_len);
        resp.write(json, json_len);

        return resp.content_length;
    }

private:
    static size_t generate_json(char* buffer, size_t buffer_size,
                                TimerEventHandler* timer,
                                TelemetryEventHandler* telemetry) {
        return snprintf(buffer, buffer_size,
            "{\"running\":%s,\"elapsed\":%u,\"start_count\":%u,\"stop_count\":%u,"
            "\"speed\":%.2f,\"altitude\":%.1f,\"force\":%.3f}",
            timer->is_timer_running() ? "true" : "false",
            timer->get_elapsed_seconds(),
            timer->get_start_count(),
            timer->get_stop_count(),
            telemetry->speed,
            telemetry->altitude,
            telemetry->force
        );
    }

    static size_t generate_error(HttpResponse& resp) {
        return generate_error_500(resp);
    }
};

} // namespace http::core
