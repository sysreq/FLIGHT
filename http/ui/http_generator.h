#pragma once

#include "..\config\http_config.h"

namespace http::servers {

class HttpGenerator {
public:
    // Public static telemetry variables for external setting
    static inline float speed = 0.0f;      // Speed value
    static inline float altitude = 0.0f;   // Altitude value  
    static inline float force = 0.0f;      // Force value
    
    static size_t generate_page(char* buffer, size_t buffer_size, uint64_t uptime_seconds, size_t event_queue_size);
    static size_t generate_response(char* buffer, size_t buffer_size, uint64_t uptime_seconds, size_t event_queue_size);
};

} // namespace http::servers