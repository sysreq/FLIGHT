#pragma once
#include <atomic>
#include <cstdint>

namespace network::handlers {
    // Global state updated by application, read by handlers
    struct SharedState {
        // Legacy telemetry (keep for compatibility)
        std::atomic<float> force{0.0f};
        std::atomic<float> speed{0.0f};
        std::atomic<uint32_t> uptime_seconds{0};

        // Session management
        std::atomic<bool> session_active{false};
        std::atomic<uint64_t> session_start_time{0};  // Absolute time in milliseconds

        // Sensor initialization status
        std::atomic<bool> airspeed_ready{false};
        std::atomic<bool> force_sensor_ready{false};
        std::atomic<bool> power_ready{false};
        std::atomic<bool> accel_ready{false};
        std::atomic<bool> gyro_ready{false};

        // Sensor values (only valid when session is active)
        std::atomic<float> airspeed{0.0f};          // m/s
        std::atomic<float> force_value{0.0f};       // N
        std::atomic<float> power{0.0f};             // W
        std::atomic<float> accel_x{0.0f};           // m/s²
        std::atomic<float> accel_y{0.0f};           // m/s²
        std::atomic<float> accel_z{0.0f};           // m/s²
        std::atomic<float> gyro_x{0.0f};            // rad/s
        std::atomic<float> gyro_y{0.0f};            // rad/s
        std::atomic<float> gyro_z{0.0f};            // rad/s
    };

    inline SharedState g_shared_state{};
}
