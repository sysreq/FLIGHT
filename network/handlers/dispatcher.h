#pragma once
#include "../platform/connection.h"
#include "../types.h"
#include <array>
#include <string_view>

namespace network::handlers {
    using HandlerFn = void(*)(platform::Connection&);

    struct Route {
        std::string_view path;
        HttpMethod method;
        HandlerFn handler;
    };

    // Forward declarations
    void HandleIndex(platform::Connection& conn);
    void HandleStatus(platform::Connection& conn);
    void HandleSessionStart(platform::Connection& conn);
    void HandleSessionStop(platform::Connection& conn);
    void HandleSessionStatus(platform::Connection& conn);
    void HandleSensors(platform::Connection& conn);

    // Static route table
    inline constexpr std::array<Route, 6> g_routes = {{
        {"/", HttpMethod::GET, HandleIndex},
        {"/status", HttpMethod::GET, HandleStatus},
        {"/api/session", HttpMethod::POST, HandleSessionStart},
        {"/api/session", HttpMethod::DELETE, HandleSessionStop},
        {"/api/session/status", HttpMethod::GET, HandleSessionStatus},
        {"/api/sensors", HttpMethod::GET, HandleSensors}
    }};

    void Dispatch(platform::Connection& conn, std::string_view path, HttpMethod method);
    void SendNotFound(platform::Connection& conn);
}
