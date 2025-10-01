# HTTP Framework Usage Guide

The HTTP framework has been refactored to make it trivial to add new routes, events, and handlers.

## Quick Examples

### Adding a New Route

Create a handler file in `http/core/handlers/`:

```cpp
// my_handler.h
#pragma once
#include "handler_interface.h"

namespace http::core {

class MyHandler {
public:
    static size_t handle(const HttpRequest& req, HttpResponse& resp, void* context) {
        const char* response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nHello!";
        return resp.write(response, strlen(response));
    }
};

} // namespace http::core
```

Register it in your initialization code:

```cpp
router.register_route("/my-endpoint", "GET", MyHandler::handle);
```

That's it! Your route is live.

### Adding a New Event Type

1. **Add to enum** in `http/core/events/event_types.h`:
```cpp
enum class EventType : uint8_t {
    // ... existing events
    CalibrationStart,  // <- Add your event here
};
```

2. **Update string converter** (for HTTP query parsing):
```cpp
inline EventType event_type_from_string(const char* name) {
    // ... existing mappings
    if (std::strcmp(name, "calibrate") == 0) return EventType::CalibrationStart;
    return EventType::None;
}
```

3. **Create an event handler** in `http/core/events/handlers/`:
```cpp
// calibration_event_handler.h
class CalibrationEventHandler {
public:
    static void handle_event(const Event& event, void* context) {
        auto* handler = static_cast<CalibrationEventHandler*>(context);
        if (event.type == EventType::CalibrationStart) {
            printf("Calibration started!\n");
        }
    }
};
```

4. **Register the handler** in router initialization:
```cpp
static CalibrationEventHandler calibration_handler;
event_dispatcher.register_handler(
    EventType::CalibrationStart,
    CalibrationEventHandler::handle_event,
    &calibration_handler
);
```

Now events with `?e=calibrate` will be dispatched to your handler!

### Adding a Status Page

Create a handler that generates JSON (or HTML):

```cpp
class MetricsHandler {
public:
    static size_t handle(const HttpRequest& req, HttpResponse& resp, void* context) {
        char json[256];
        size_t len = snprintf(json, sizeof(json),
            "{\"uptime\":%lu,\"requests\":%u}",
            get_uptime(), get_request_count());

        char header[128];
        snprintf(header, sizeof(header),
            "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
            "Content-Length: %zu\r\n\r\n", len);

        resp.write(header, strlen(header));
        resp.write(json, len);
        return resp.content_length;
    }
};
```

Register:
```cpp
router.register_route("/metrics", "GET", MetricsHandler::handle);
```

## Architecture Overview

### Directory Structure
```
http/
├── core/
│   ├── handlers/              # Route handlers
│   │   ├── handler_interface.h
│   │   ├── index_handler.h
│   │   ├── status_handler.h
│   │   └── [your_handler.h]   # Add new handlers here
│   ├── events/                # Event system
│   │   ├── event_types.h      # Event enum & conversions
│   │   ├── event_dispatcher.h # Event routing
│   │   └── handlers/          # Event handlers
│   │       ├── timer_event_handler.h
│   │       ├── telemetry_event_handler.h
│   │       └── [your_event_handler.h]
│   ├── http_router.{h,cpp}    # Route registry
│   ├── http_server.{h,cpp}    # TCP server
│   └── http_events.{h,cpp}    # Event queue
├── network/                   # Network servers (DHCP, DNS)
├── ui/                        # UI generation
└── config/                    # Configuration
```

### Key Components

#### 1. Route Registry (`http_router.h`)
- Stores routes in a fixed-size array (no heap allocation)
- Linear search for routing (fast for <20 routes)
- Easy to add routes via `register_route()`

#### 2. Event Dispatcher (`events/event_dispatcher.h`)
- Type-safe event dispatch (uses enum, not strings)
- Array-indexed lookup (O(1) dispatch)
- Register handlers with `register_handler()`

#### 3. Handler Interface (`handlers/handler_interface.h`)
- Simple function pointer: `size_t(*)(Request&, Response&, void*)`
- Handlers are stateless functions (context passed via void*)
- Return number of bytes written to response

## Accessing System Components

From main.cpp or other code:

```cpp
// Access telemetry data
access_point.telemetry_handler().speed = 42.5f;
access_point.telemetry_handler().altitude = 1000.0f;
access_point.telemetry_handler().force = 9.8f;

// Access event queue
http::core::Event event;
if (access_point.event_handler().pop_event(event)) {
    // Process event
}

// Access timer state
bool running = access_point.timer_handler().is_timer_running();
uint32_t elapsed = access_point.timer_handler().get_elapsed_seconds();
```

## Best Practices

1. **Handlers should be stateless** - Use context parameter for state
2. **Event handlers should be fast** - Defer heavy work to main loop
3. **Use typed events** - Avoid string comparisons at runtime
4. **Keep routes under 16** - Current MAX_ROUTES limit (configurable)
5. **Buffer management** - Always check response buffer size

## Example: Complete Custom Feature

Let's add a `/health` endpoint that shows system health:

**1. Create handler** (`http/core/handlers/health_handler.h`):
```cpp
#pragma once
#include "handler_interface.h"

namespace http::core {

class HealthHandler {
public:
    static size_t handle(const HttpRequest& req, HttpResponse& resp, void* context) {
        bool healthy = check_system_health();  // Your logic here

        char json[128];
        size_t json_len = snprintf(json, sizeof(json),
            "{\"status\":\"%s\",\"timestamp\":%lu}",
            healthy ? "ok" : "error",
            to_ms_since_boot(get_absolute_time()));

        // Write HTTP headers
        char header[128];
        size_t header_len = snprintf(header, sizeof(header),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n\r\n",
            json_len);

        resp.write(header, header_len);
        resp.write(json, json_len);
        return resp.content_length;
    }

private:
    static bool check_system_health() {
        // Check sensors, memory, etc.
        return true;
    }
};

} // namespace http::core
```

**2. Register route** (in `http_router.cpp::register_default_routes()`):
```cpp
#include "handlers/health_handler.h"

// In register_default_routes():
register_route("/health", "GET", HealthHandler::handle);
```

Done! Your `/health` endpoint is now live and accessible.

## Migration Notes

### Old Code (Before Refactor)
```cpp
// Adding a route required modifying router.cpp
if (path == "/newroute") {
    return handle_newroute(req, resp);  // Add handler method
}

// Events used string comparison
if (event.name == "newevent") {  // Runtime string compare
    // Handle event
}
```

### New Code (After Refactor)
```cpp
// Register route in one line
router.register_route("/newroute", "GET", MyHandler::handle);

// Events use typed enum (compile-time)
if (event.type == EventType::NewEvent) {  // Integer compare
    // Handle event
}
```

## Performance Characteristics

- **Route lookup**: O(n) linear search, but n is small (<16 typically)
- **Event dispatch**: O(1) array lookup
- **Memory**: Zero heap allocation, all stack/static
- **Handler overhead**: Single function pointer call

## Future Enhancements

Possible extensions to consider:

1. **Middleware support** - Pre/post processing of requests
2. **Route parameters** - Extract values from paths (e.g., `/user/:id`)
3. **Method routing** - Built-in GET/POST/etc. routing
4. **Async handlers** - For long-running operations
5. **WebSocket support** - Upgrade HTTP connections

## Troubleshooting

**Route not working?**
- Check registration order (first match wins)
- Verify path matches exactly (case-sensitive)
- Check MAX_ROUTES limit not exceeded

**Event not dispatching?**
- Verify EventType enum includes your event
- Check event_type_from_string() includes mapping
- Ensure handler is registered before first event

**Build errors?**
- Include handler header in router.cpp
- Forward declare types in handler headers
- Check namespace declarations (http::core)
