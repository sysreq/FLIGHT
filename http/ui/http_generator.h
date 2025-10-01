#pragma once

#include <cstddef>
#include <cstdint>

namespace http::ui {

class HttpGenerator {
public:
    // Public static telemetry variables for external setting
    // These are kept for backward compatibility during migration
    static inline float speed = 0.0f;      // Speed value
    static inline float altitude = 0.0f;   // Altitude value
    static inline float force = 0.0f;      // Force value

    static size_t generate_page(char* buffer, size_t buffer_size, uint64_t uptime_seconds, size_t event_queue_size);
    static size_t generate_page(char* buffer, size_t buffer_size, uint64_t uptime_seconds, size_t event_queue_size,
                               float speed_val, float altitude_val, float force_val);
    static size_t generate_response(char* buffer, size_t buffer_size, uint64_t uptime_seconds, size_t event_queue_size);
    static size_t generate_response(char* buffer, size_t buffer_size, uint64_t uptime_seconds, size_t event_queue_size,
                                   float speed_val, float altitude_val, float force_val);
};

} // namespace http::ui