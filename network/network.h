#pragma once
#include <cstdint>

namespace network {
    // Initialize network subsystem and start services
    bool Start();

    // Shutdown network subsystem
    void Stop();

    // Check if network is initialized
    bool IsInitialized();

    // WiFi Configuration API
    namespace WiFi {
        // Set WiFi credentials (must be called before Start())
        void SetCredentials(const char* ssid, const char* password);

        // Get current connection status
        enum class ConnectionStatus : uint8_t {
            Disconnected,
            Connecting,
            Connected,
            Error
        };
        ConnectionStatus GetConnectionStatus();

        // Get current IP address (returns 0.0.0.0 if not connected)
        struct IpAddress {
            uint8_t a, b, c, d;
        };
        IpAddress GetIpAddress();

        // Wait for connection (blocks until connected or timeout)
        bool WaitForConnection(uint32_t timeout_ms);
    }

    // Status update API (called by application)
    namespace Status {
        void SetForce(float value);
        void SetSpeed(float value);
        void SetUptime(uint32_t seconds);
    }
}
