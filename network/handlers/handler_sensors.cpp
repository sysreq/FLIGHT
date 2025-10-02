#include "handler_sensors.h"
#include "response_helpers.h"
#include "shared_state.h"
#include <cstdio>

namespace network::handlers {
    void HandleSensors(platform::Connection& conn) {
        char body[1024];
        int len;

        if (g_shared_state.session_active.load()) {
            len = snprintf(body, sizeof(body),
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
                g_shared_state.gyro_z.load());
        } else {
            len = snprintf(body, sizeof(body),
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
                g_shared_state.gyro_ready.load() ? "READY" : "FAILED");
        }

        if (len > 0 && static_cast<size_t>(len) < sizeof(body)) {
            SendJsonResponse(conn, body, len);
        }
    }
}
