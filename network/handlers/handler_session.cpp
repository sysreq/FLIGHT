#include "handler_session.h"
#include "shared_state.h"
#include "pico/time.h"
#include <cstdio>
#include <cstring>

namespace network::handlers {
    void HandleSessionStart(platform::Connection& conn) {
        // Only start if not already active
        bool expected = false;
        if (g_shared_state.session_active.compare_exchange_strong(expected, true)) {
            // Session started - record start time
            g_shared_state.session_start_time.store(time_us_64() / 1000);  // Convert to ms

            const char* response =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: application/json\r\n"
                "Content-Length: 21\r\n"
                "\r\n"
                "{\"status\":\"started\"}";

            size_t len = strlen(response);
            std::memcpy(conn.response_buffer.data(), response, len);
            conn.response_length = len;
        } else {
            // Already active - ignore
            const char* response =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: application/json\r\n"
                "Content-Length: 29\r\n"
                "\r\n"
                "{\"status\":\"already_active\"}";

            size_t len = strlen(response);
            std::memcpy(conn.response_buffer.data(), response, len);
            conn.response_length = len;
        }
    }

    void HandleSessionStop(platform::Connection& conn) {
        // Only stop if active
        bool expected = true;
        if (g_shared_state.session_active.compare_exchange_strong(expected, false)) {
            // Session stopped - clear sensor values
            g_shared_state.airspeed.store(0.0f);
            g_shared_state.force_value.store(0.0f);
            g_shared_state.power.store(0.0f);
            g_shared_state.accel_x.store(0.0f);
            g_shared_state.accel_y.store(0.0f);
            g_shared_state.accel_z.store(0.0f);
            g_shared_state.gyro_x.store(0.0f);
            g_shared_state.gyro_y.store(0.0f);
            g_shared_state.gyro_z.store(0.0f);
            g_shared_state.session_start_time.store(0);

            const char* response =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: application/json\r\n"
                "Content-Length: 21\r\n"
                "\r\n"
                "{\"status\":\"stopped\"}";

            size_t len = strlen(response);
            std::memcpy(conn.response_buffer.data(), response, len);
            conn.response_length = len;
        } else {
            // Already stopped - ignore
            const char* response =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: application/json\r\n"
                "Content-Length: 30\r\n"
                "\r\n"
                "{\"status\":\"already_stopped\"}";

            size_t len = strlen(response);
            std::memcpy(conn.response_buffer.data(), response, len);
            conn.response_length = len;
        }
    }

    void HandleSessionStatus(platform::Connection& conn) {
        char body[256];
        int body_written;

        if (g_shared_state.session_active.load()) {
            // Calculate elapsed time in seconds
            uint64_t current_time = time_us_64() / 1000;  // Current time in ms
            uint64_t start_time = g_shared_state.session_start_time.load();
            uint64_t elapsed_ms = current_time - start_time;
            uint32_t elapsed_sec = static_cast<uint32_t>(elapsed_ms / 1000);

            body_written = snprintf(body, sizeof(body),
                "{\"active\":true,\"elapsed\":%lu}",
                (unsigned long)elapsed_sec);
        } else {
            body_written = snprintf(body, sizeof(body),
                "{\"active\":false}");
        }

        if (body_written < 0 || static_cast<size_t>(body_written) >= sizeof(body)) {
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

        if (written < 0 || static_cast<size_t>(written) >= sizeof(response)) {
            return;
        }

        if (static_cast<size_t>(written) > conn.response_buffer.size()) {
            return;
        }

        std::memcpy(conn.response_buffer.data(), response, written);
        conn.response_length = written;
    }
}
