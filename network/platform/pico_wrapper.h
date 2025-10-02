#pragma once
#include "../include/error.h"
#include <expected>
#include <cstdint>

// Forward declaration to avoid including network.h
namespace network::WiFi {
    enum class ConnectionStatus : uint8_t;
}

namespace network::platform::pico {
    std::expected<void, ErrorCode> InitWiFi();
    void DeinitWiFi();

    bool StartTimer(bool (*callback)(void*), void* user_data, uint32_t interval_ms);
    void StopTimer();

    // WiFi status functions
    network::WiFi::ConnectionStatus GetWiFiStatus();
    uint32_t GetIpAddress();  // Returns IP in network byte order
    bool WaitForConnection(uint32_t timeout_ms);
}
