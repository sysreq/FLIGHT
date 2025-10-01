#pragma once

#include "..\http_types.h"
#include "..\http_events.h"
#include "..\http_utils.h"
#include "..\events\event_dispatcher.h"
#include "..\events\telemetry_event_handler.h"
#include "..\..\ui\http_generator.h"

namespace http::core {

// Context structure for index handler
struct IndexHandlerContext {
    HttpEventHandler* event_queue;
    EventDispatcher* event_dispatcher;
    absolute_time_t* start_time;
    TelemetryEventHandler* telemetry_handler;
};

class IndexHandler {
public:
    static size_t handle(const HttpRequest& req, HttpResponse& resp, void* context) {
        auto* ctx = static_cast<IndexHandlerContext*>(context);
        if (!ctx) {
            return generate_error(resp);
        }

        if (req.query_string && ctx->event_queue && ctx->event_dispatcher) {
            process_query_events(req, ctx);
        }

        uint64_t uptime_s = 0;
        if (ctx->start_time) {
            uptime_s = utils::us_to_seconds(absolute_time_diff_us(*ctx->start_time, get_absolute_time()));
        }

        size_t queue_size = ctx->event_queue ? ctx->event_queue->queue_size() : 0;

        float speed = ctx->telemetry_handler ? ctx->telemetry_handler->speed : 0.0f;
        float altitude = ctx->telemetry_handler ? ctx->telemetry_handler->altitude : 0.0f;
        float force = ctx->telemetry_handler ? ctx->telemetry_handler->force : 0.0f;

        return ui::HttpGenerator::generate_response(
            resp.buffer,
            resp.buffer_size,
            uptime_s,
            queue_size,
            speed,
            altitude,
            force
        );
    }

private:
    static void process_query_events(const HttpRequest& req, IndexHandlerContext* ctx) {
        Event event = ctx->event_queue->parse_event(req.full_request);

        if (event.type != EventType::None) {
            ctx->event_dispatcher->dispatch(event);
            ctx->event_queue->process_request(req.full_request, req.request_len);
        }
    }

    static size_t generate_error(HttpResponse& resp) {
        return generate_error_500(resp);
    }
};

} // namespace http::core
