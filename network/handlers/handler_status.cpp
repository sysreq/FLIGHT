#include "dispatcher.h"
#include "shared_state.h"
#include <cstdio>
#include <cstring>

namespace network::handlers {
    void HandleStatus(platform::Connection& conn) {
        char body[256];
        int body_written = snprintf(body, sizeof(body),
            "{"
            "\"force\":%.2f,"
            "\"speed\":%.2f,"
            "\"uptime\":%lu"
            "}",
            g_shared_state.force.load(),
            g_shared_state.speed.load(),
            (unsigned long)g_shared_state.uptime_seconds.load());

        // Check for truncation in body
        if (body_written < 0 || static_cast<size_t>(body_written) >= sizeof(body)) {
            // JSON body too large or error
            return;
        }

        char response[512];
        int written = snprintf(response, sizeof(response),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %d\r\n"
            "\r\n"
            "%s",
            body_written, body);

        // Check for truncation in response
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
