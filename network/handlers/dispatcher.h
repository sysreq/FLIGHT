#pragma once
#include "../platform/config.h"
#include <array>
#include <string_view>

namespace network::handlers {
    using HandlerFn = void(*)(platform::Connection&);

    struct Route {
        std::string_view path;
        HandlerFn handler;
    };

    // Forward declarations
    void HandleIndex(platform::Connection& conn);
    void HandleStatus(platform::Connection& conn);

    // Static route table
    inline constexpr std::array<Route, 2> g_routes = {{
        {"/", HandleIndex},
        {"/status", HandleStatus}
    }};

    void Dispatch(platform::Connection& conn, std::string_view path);
    void SendNotFound(platform::Connection& conn);
}
