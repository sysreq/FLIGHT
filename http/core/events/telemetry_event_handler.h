#pragma once

#include <cstdint>
#include <cstdio>
#include "event_types.h"

namespace http::core {

class TelemetryEventHandler {
public:
    float speed = 0.0f;
    float altitude = 0.0f;
    float force = 0.0f;

    TelemetryEventHandler() = default;

    static void handle_event(const Event& event, void* context) {
        auto* handler = static_cast<TelemetryEventHandler*>(context);
        if (!handler) return;

        if (event.type == EventType::TelemetryUpdate) {
            handler->update_telemetry(event);
        }
    }

    void reset() {
        speed = 0.0f;
        altitude = 0.0f;
        force = 0.0f;
    }

private:
    void update_telemetry(const Event& event) {
        printf("Telemetry update: v1=%d, v2=%d, f=%.3f\n",
               event.value1, event.value2, event.fvalue);
    }
};

} // namespace http::core
