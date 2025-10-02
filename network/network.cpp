#include "network.h"
#include "platform/pico_wrapper.h"
#include "platform/lwip_wrapper.h"
#include "services/service_manager.h"
#include "services/http_service.h"
#include "handlers/shared_state.h"
#include <cstdio>
#include <cstring>

namespace network {
    namespace detail {
        // Internal state
        static bool g_initialized = false;
        static services::ServiceManager<services::HttpService> g_service_manager;

        // WiFi credentials
        static char g_wifi_ssid[32] = "";
        static char g_wifi_password[64] = "";

        // Timer callback
        static bool TimerCallback(void* /*user_data*/) {
            platform::lwip::Poll();
            g_service_manager.ProcessAll();
            return true;  // Keep timer running
        }
    }

    bool Start() {
        if (detail::g_initialized) {
            printf("Network: Already initialized\n");
            return false;
        }

        printf("Network: Starting...\n");

        // Note: WiFi/CYW43 must be initialized externally before calling Start()
        // This allows the caller to configure AP or STA mode as needed

        // Start services
        if (!detail::g_service_manager.StartAll()) {
            printf("Network: Service start failed\n");
            return false;
        }

        // Start timer (10ms interval)
        if (!platform::pico::StartTimer(detail::TimerCallback, nullptr, 10)) {
            printf("Network: Timer start failed\n");
            detail::g_service_manager.StopAll();
            return false;
        }

        detail::g_initialized = true;
        printf("Network: Initialized successfully\n");
        return true;
    }

    void Stop() {
        if (!detail::g_initialized) {
            return;
        }

        printf("Network: Stopping...\n");

        platform::pico::StopTimer();
        detail::g_service_manager.StopAll();
        // Note: WiFi/CYW43 deinit is handled externally
        detail::g_initialized = false;

        printf("Network: Stopped\n");
    }

    bool IsInitialized() {
        return detail::g_initialized;
    }

    namespace WiFi {
        void SetCredentials(const char* ssid, const char* password) {
            if (ssid) {
                strncpy(detail::g_wifi_ssid, ssid, sizeof(detail::g_wifi_ssid) - 1);
                detail::g_wifi_ssid[sizeof(detail::g_wifi_ssid) - 1] = '\0';
            }
            if (password) {
                strncpy(detail::g_wifi_password, password, sizeof(detail::g_wifi_password) - 1);
                detail::g_wifi_password[sizeof(detail::g_wifi_password) - 1] = '\0';
            }
        }

        ConnectionStatus GetConnectionStatus() {
            return platform::pico::GetWiFiStatus();
        }

        IpAddress GetIpAddress() {
            auto addr = platform::pico::GetIpAddress();
            return {
                static_cast<uint8_t>(addr & 0xFF),
                static_cast<uint8_t>((addr >> 8) & 0xFF),
                static_cast<uint8_t>((addr >> 16) & 0xFF),
                static_cast<uint8_t>((addr >> 24) & 0xFF)
            };
        }

        bool WaitForConnection(uint32_t timeout_ms) {
            return platform::pico::WaitForConnection(timeout_ms);
        }
    }

    namespace Status {
        void SetForce(float value) {
            handlers::g_shared_state.force.store(value);
        }

        void SetSpeed(float value) {
            handlers::g_shared_state.speed.store(value);
        }

        void SetUptime(uint32_t seconds) {
            handlers::g_shared_state.uptime_seconds.store(seconds);
        }
    }
}
