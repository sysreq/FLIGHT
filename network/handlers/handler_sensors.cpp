#include "handler_sensors.h"
#include "shared_state.h"
#include <cstdio>
#include <cstring>

namespace network::handlers {
    void HandleSensors(platform::Connection& conn) {
        char body[1024];
        int body_written;

        bool session_active = g_shared_state.session_active.load();

        if (session_active) {
            // Return sensor values
            body_written = snprintf(body, sizeof(body),
                "{"
                "\"airspeed\":{\"value\":%.2f,\"unit\":\"m/s\"},"
                "\"force\":{\"value\":%.2f,\"unit\":\"N\"},"
                "\"power\":{\"value\":%.2f,\"unit\":\"W\"},"
                "\"accel\":{\"x\":%.2f,\"y\":%.2f,\"z\":%.2f,\"unit\":\"m/s²\"},"
                "\"gyro\":{\"x\":%.3f,\"y\":%.3f,\"z\":%.3f,\"unit\":\"rad/s\"}"
                "}",
                g_shared_state.airspeed.load(),
                g_shared_state.force_value.load(),
                g_shared_state.power.load(),
                g_shared_state.accel_x.load(),
                g_shared_state.accel_y.load(),
                g_shared_state.accel_z.load(),
                g_shared_state.gyro_x.load(),
                g_shared_state.gyro_y.load(),
                g_shared_state.gyro_z.load()
            );
        } else {
            // Return sensor initialization status
            body_written = snprintf(body, sizeof(body),
                "{"
                "\"airspeed\":\"%s\","
                "\"force\":\"%s\","
                "\"power\":\"%s\","
                "\"accel\":\"%s\","
                "\"gyro\":\"%s\""
                "}",
                g_shared_state.airspeed_ready.load() ? "READY" : "FAILED",
                g_shared_state.force_sensor_ready.load() ? "READY" : "FAILED",
                g_shared_state.power_ready.load() ? "READY" : "FAILED",
                g_shared_state.accel_ready.load() ? "READY" : "FAILED",
                g_shared_state.gyro_ready.load() ? "READY" : "FAILED"
            );
        }

        if (body_written < 0 || static_cast<size_t>(body_written) >= sizeof(body)) {
            return;
        }

        char response[2048];
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
