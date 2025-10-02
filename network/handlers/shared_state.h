#pragma once
#include <atomic>

namespace network::handlers {
    // Global state updated by application, read by handlers
    struct SharedState {
        std::atomic<float> force{0.0f};
        std::atomic<float> speed{0.0f};
        std::atomic<uint32_t> uptime_seconds{0};
        // Add more as needed
    };

    inline SharedState g_shared_state{};
}
