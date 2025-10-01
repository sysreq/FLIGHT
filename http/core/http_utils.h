#pragma once

// ============================================
// HTTP UTILITIES - Common Helpers & Constants
// ============================================
// Consolidated utility functions and constants for HTTP subsystem.
// Minimal dependencies - only includes http_types.h

#include <cstdint>
#include <cstring>
#include "http_types.h"

namespace http::core::constants {
    // Buffer sizes
    constexpr size_t JSON_BUFFER_SIZE = 256;
    constexpr size_t HEADER_BUFFER_SIZE = 128;
    constexpr size_t PATH_BUFFER_SIZE = 128;
    constexpr size_t NAME_BUFFER_SIZE = 64;
    constexpr size_t CONTENT_BUFFER_SIZE = 8192;

    // Time conversions
    constexpr uint32_t MS_PER_SECOND = 1000;
    constexpr uint32_t US_PER_SECOND = 1000000;

    // HTTP error response lengths
    constexpr size_t ERROR_500_CONTENT_LENGTH = 21;
    constexpr size_t ERROR_404_CONTENT_LENGTH = 9;

    // HTTP limits
    constexpr size_t MAX_ROUTES = 16;
    constexpr size_t MAX_EVENT_TYPES = 16;
    constexpr size_t MAX_EVENT_QUEUE_SIZE = 32;
}

namespace http::core {

// ============================================
// HTTP ERROR RESPONSES
// ============================================

inline size_t generate_error_500(HttpResponse& resp) {
    const char* error = "HTTP/1.1 500 Internal Server Error\r\n"
                      "Content-Type: text/plain\r\n"
                      "Content-Length: 21\r\n"
                      "Connection: close\r\n"
                      "\r\n"
                      "Internal Server Error";
    return resp.write(error, std::strlen(error));
}

inline size_t generate_error_404(HttpResponse& resp) {
    const char* error = "HTTP/1.1 404 Not Found\r\n"
                      "Content-Type: text/plain\r\n"
                      "Content-Length: 9\r\n"
                      "Connection: close\r\n"
                      "\r\n"
                      "Not Found";
    return resp.write(error, std::strlen(error));
}

// ============================================
// TIME CONVERSION UTILITIES
// ============================================

namespace utils {
    inline uint32_t ms_to_seconds(uint32_t ms) {
        return ms / constants::MS_PER_SECOND;
    }

    inline uint64_t us_to_seconds(uint64_t us) {
        return us / constants::US_PER_SECOND;
    }

    inline uint32_t seconds_to_ms(uint32_t seconds) {
        return seconds * constants::MS_PER_SECOND;
    }

    inline uint64_t seconds_to_us(uint64_t seconds) {
        return seconds * constants::US_PER_SECOND;
    }
} // namespace utils

} // namespace http::core
