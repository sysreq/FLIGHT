#pragma once

#include <cstdint>
#include <cstring>

// Need full platform type for inline constructors
#include "pico/time.h"

namespace http::core {

enum class EventType : uint8_t {
    None = 0,
    TimerStart,
    TimerStop,
    TelemetryUpdate,
};

struct Event {
    EventType type = EventType::None;
    int32_t value1 = 0;
    int32_t value2 = 0;
    float fvalue = 0.0f;
    uint32_t timestamp;

    Event() : timestamp(to_ms_since_boot(get_absolute_time())) {}

    explicit Event(EventType t)
        : type(t), timestamp(to_ms_since_boot(get_absolute_time())) {}

    Event(EventType t, int32_t v1, int32_t v2 = 0, float fv = 0.0f)
        : type(t), value1(v1), value2(v2), fvalue(fv),
          timestamp(to_ms_since_boot(get_absolute_time())) {}
};

inline EventType event_type_from_string(const char* name) {
    if (std::strcmp(name, "start") == 0) return EventType::TimerStart;
    if (std::strcmp(name, "stop") == 0) return EventType::TimerStop;
    if (std::strcmp(name, "telemetry") == 0) return EventType::TelemetryUpdate;
    return EventType::None;
}

inline const char* event_type_to_string(EventType type) {
    switch (type) {
        case EventType::None: return "None";
        case EventType::TimerStart: return "TimerStart";
        case EventType::TimerStop: return "TimerStop";
        case EventType::TelemetryUpdate: return "TelemetryUpdate";
        default: return "Unknown";
    }
}

} // namespace http::core
