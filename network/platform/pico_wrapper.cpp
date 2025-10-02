#include "pico_wrapper.h"
#include "../network.h"  // For ConnectionStatus enum
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/timer.h"
#include "lwip/netif.h"

namespace network::platform::pico {
    namespace detail {
        // Static timer handle
        static repeating_timer_t g_timer{};
        static bool g_timer_active = false;
    }

    std::expected<void, ErrorCode> InitWiFi() {
        // Initialize WiFi in polling mode
        if (cyw43_arch_init()) {
            return std::unexpected(ErrorCode::ConnectionFailed);
        }

        cyw43_arch_enable_sta_mode();

        // Note: Actual WiFi connection (SSID/password) should be done by application
        // This just initializes the hardware
        return {};
    }

    void DeinitWiFi() {
        cyw43_arch_deinit();
    }

    bool StartTimer(bool (*callback)(void*), void* user_data, uint32_t interval_ms) {
        if (!callback) {
            return false;
        }

        if (detail::g_timer_active) {
            // Timer already running
            return false;
        }

        // Convert milliseconds to microseconds (negative for repeating timer)
        int64_t interval_us = -static_cast<int64_t>(interval_ms * 1000);

        bool success = add_repeating_timer_us(interval_us,
            reinterpret_cast<repeating_timer_callback_t>(callback),
            user_data,
            &detail::g_timer);

        if (success) {
            detail::g_timer_active = true;
        }

        return success;
    }

    void StopTimer() {
        if (detail::g_timer_active) {
            cancel_repeating_timer(&detail::g_timer);
            detail::g_timer_active = false;
        }
    }

    network::WiFi::ConnectionStatus GetWiFiStatus() {
        // Check if WiFi is connected
        int status = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);

        switch (status) {
            case CYW43_LINK_DOWN:
                return network::WiFi::ConnectionStatus::Disconnected;
            case CYW43_LINK_JOIN:
            case CYW43_LINK_NOIP:
                return network::WiFi::ConnectionStatus::Connecting;
            case CYW43_LINK_UP:
                return network::WiFi::ConnectionStatus::Connected;
            default:
                return network::WiFi::ConnectionStatus::Error;
        }
    }

    uint32_t GetIpAddress() {
        // Get the default netif (network interface)
        struct netif* netif = netif_default;
        if (netif && netif_is_up(netif)) {
            return netif->ip_addr.addr;  // Already in network byte order
        }
        return 0;  // 0.0.0.0
    }

    bool WaitForConnection(uint32_t timeout_ms) {
        uint32_t start_time = to_ms_since_boot(get_absolute_time());

        while (to_ms_since_boot(get_absolute_time()) - start_time < timeout_ms) {
            if (GetWiFiStatus() == network::WiFi::ConnectionStatus::Connected) {
                return true;
            }

            // Poll to keep network stack alive
            cyw43_arch_poll();
            sleep_ms(10);
        }

        return false;
    }
}
