#include "dispatcher.h"
#include "response_helpers.h"
#include <algorithm>

namespace network::handlers {
    void Dispatch(platform::Connection& conn, std::string_view path, HttpMethod method) {
        auto it = std::find_if(g_routes.begin(), g_routes.end(),
            [path, method](const Route& route) {
                return route.path == path && route.method == method;
            });

        if (it != g_routes.end()) {
            it->handler(conn);
        } else {
            SendNotFound(conn);
        }
    }

    void SendNotFound(platform::Connection& conn) {
        SendPlainTextResponse(conn, "Not Found", 404, "Not Found");
    }
}
