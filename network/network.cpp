#include "network.h"
#include "platform/pico_wrapper.h"
#include "platform/lwip_wrapper.h"
#include "services/service_manager.h"
#include "services/http_service.h"
#include "services/dhcp_service.h"
#include "services/dns_service.h"
#include "handlers/shared_state.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include <cstdio>
#include <cstring>

namespace network {
    namespace detail {
        // Network configuration constants
        constexpr uint8_t AP_IP_ADDR[4] = {192, 168, 4, 1};
        constexpr uint8_t AP_NETMASK[4] = {255, 255, 255, 0};
        constexpr uint8_t AP_GATEWAY[4] = {192, 168, 4, 1};

        // Internal state
        static bool g_initialized = false;
        static services::ServiceManager<services::HttpService, services::DhcpService, services::DnsService> g_service_manager;

        // WiFi credentials
        static char g_wifi_ssid[32] = "";
        static char g_wifi_password[64] = "";
    }

    bool Start() {
        if (detail::g_initialized) {
            printf("Network: Already initialized\n");
            return false;
        }

        printf("Network: Starting...\n");

        // Note: WiFi/CYW43 must be initialized externally before calling Start()
        // This allows the caller to configure AP or STA mode as needed

        // Auto-configure DHCP and DNS services if netif is available
        if (netif_default != nullptr) {
            const ip4_addr_t* ip_ptr = netif_ip_addr4(netif_default);
            const ip4_addr_t* mask_ptr = netif_ip_netmask4(netif_default);

            if (ip_ptr != nullptr && mask_ptr != nullptr) {
                ip_addr_t router_ip;
                ip_addr_t netmask;
                ip_addr_copy_from_ip4(router_ip, *ip_ptr);
                ip_addr_copy_from_ip4(netmask, *mask_ptr);

                // Configure DHCP service
                auto& dhcp = std::get<1>(detail::g_service_manager.GetServices());
                dhcp.Configure(&router_ip, &netmask);

                // Configure DNS service
                auto& dns = std::get<2>(detail::g_service_manager.GetServices());
                dns.Configure(&router_ip);
            }
        }

        // Start services
        if (!detail::g_service_manager.StartAll()) {
            printf("Network: Service start failed\n");
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

        detail::g_service_manager.StopAll();
        // Note: WiFi/CYW43 deinit is handled externally
        detail::g_initialized = false;

        printf("Network: Stopped\n");
    }

    bool IsInitialized() {
        return detail::g_initialized;
    }

    void Process() {
        if (detail::g_initialized) {
            detail::g_service_manager.ProcessAll();
        }
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
