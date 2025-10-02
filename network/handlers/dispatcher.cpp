#include "dispatcher.h"
#include <algorithm>
#include <cstring>

namespace network::handlers {
    void Dispatch(platform::Connection& conn, std::string_view path) {
        // Find matching route
        auto it = std::find_if(g_routes.begin(), g_routes.end(),
            [path](const Route& route) { return route.path == path; });

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
