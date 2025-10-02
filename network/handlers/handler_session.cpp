#include "handler_session.h"
#include "response_helpers.h"
#include "shared_state.h"
#include "pico/time.h"
#include <cstdio>

namespace network::handlers {
    void HandleSessionStart(platform::Connection& conn) {
        bool expected = false;
        if (g_shared_state.session_active.compare_exchange_strong(expected, true)) {
            g_shared_state.session_start_time.store(time_us_64() / 1000);
            SendJsonResponse(conn, "{\"status\":\"started\"}");
        } else {
            SendJsonResponse(conn, "{\"status\":\"already_active\"}");
        }
    }

    void HandleSessionStop(platform::Connection& conn) {
        bool expected = true;
        if (g_shared_state.session_active.compare_exchange_strong(expected, false)) {
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
            SendJsonResponse(conn, "{\"status\":\"stopped\"}");
        } else {
            SendJsonResponse(conn, "{\"status\":\"already_stopped\"}");
        }
    }

    void HandleSessionStatus(platform::Connection& conn) {
        char body[256];
        int len;

        if (g_shared_state.session_active.load()) {
            uint64_t elapsed_ms = (time_us_64() / 1000) - g_shared_state.session_start_time.load();
            uint32_t elapsed_sec = static_cast<uint32_t>(elapsed_ms / 1000);
            len = snprintf(body, sizeof(body), "{\"active\":true,\"elapsed\":%lu}", (unsigned long)elapsed_sec);
        } else {
            len = snprintf(body, sizeof(body), "{\"active\":false}");
        }

        if (len > 0 && static_cast<size_t>(len) < sizeof(body)) {
            SendJsonResponse(conn, body, len);
        }
    }
}
