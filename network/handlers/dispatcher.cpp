#include "dispatcher.h"
#include <algorithm>
#include <cstring>

namespace network::handlers {
    void Dispatch(platform::Connection& conn, std::string_view path, HttpMethod method) {
        // Find matching route (match both path and method)
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
        const char* response =
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 9\r\n"
            "\r\n"
            "Not Found";

        size_t len = strlen(response);
        std::memcpy(conn.response_buffer.data(), response, len);
        conn.response_length = len;
    }
}
