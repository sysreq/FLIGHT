#pragma once
// ============================================
#include <cstdint>
#include <cstddef>
// ============================================
namespace http {

class AccessPoint;

namespace core {
    class HttpServer;
    class HttpRequestRouter;
    class HttpEventHandler;
    class EventDispatcher;
    class TimerEventHandler;
    class TelemetryEventHandler;

    struct HttpRequest;
    struct HttpResponse;
    struct Route;
    struct Event;

    enum class EventType : uint8_t;

    using RouteHandler = size_t(*)(const HttpRequest& req, HttpResponse& resp, void* context);
    using EventHandler = void(*)(const Event& event, void* context);
}

namespace network {
    class DhcpServer;
    class DnsServer;
}

namespace ui {
    class HttpGenerator;
}

namespace config {
    // Config namespaces are defined in config/http_config.h
    namespace access_point {}
    namespace dhcp {}
    namespace dns {}
    namespace http {}
    namespace buffers {}
    namespace timeouts {}
    namespace captive_portal {}
}

} // namespace http
