#include "dispatcher.h"
#include "response_helpers.h"
#include "shared_state.h"
#include <cstdio>

namespace network::handlers {
    void HandleStatus(platform::Connection& conn) {
        char body[256];
        int len = snprintf(body, sizeof(body),
            "{\"force\":%.2f,\"speed\":%.2f,\"uptime\":%lu}",
            g_shared_state.force.load(),
            g_shared_state.speed.load(),
            (unsigned long)g_shared_state.uptime_seconds.load());

        if (len > 0 && static_cast<size_t>(len) < sizeof(body)) {
            SendJsonResponse(conn, body, len);
        }
    }
}
