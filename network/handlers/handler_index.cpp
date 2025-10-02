#include "dispatcher.h"
#include <cstring>
#include <cstdio>

namespace network::handlers {
    void HandleIndex(platform::Connection& conn) {
        // Calculate content length first for proper HTTP header
        const char* body =
            "<!DOCTYPE html>"
            "<html><head><title>Network Test</title></head>"
            "<body>"
            "<h1>Hello from RP2350!</h1>"
            "<p><a href='/status'>View Status</a></p>"
            "</body></html>";

        size_t body_len = strlen(body);

        // Format HTTP response with Content-Length header
        char response[512];  // Local buffer for formatting
        int written = snprintf(response, sizeof(response),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: %zu\r\n"
            "\r\n"
            "%s",
            body_len, body);

        // Check for buffer overflow protection
        if (written < 0 || static_cast<size_t>(written) >= sizeof(response)) {
            // Response too large for local buffer
            return;
        }

        // Check if response fits in connection buffer
        if (static_cast<size_t>(written) > conn.response_buffer.size()) {
            // Response too large for connection buffer
            return;
        }

        // Safe to copy now
        std::memcpy(conn.response_buffer.data(), response, written);
        conn.response_length = written;
    }
}
