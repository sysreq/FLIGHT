/**
 * Network Subsystem Test - Access Point Mode
 *
 * This test program demonstrates the new network subsystem in AP mode.
 * The Pico W creates its own WiFi access point and serves HTTP:
 *   - GET / : Returns a welcome page with link to status
 *   - GET /status : Returns JSON with simulated sensor data
 *
 * To test:
 *   1. Flash to Pico W
 *   2. Connect to WiFi network "PICO_AP" (password: "password123")
 *   3. Your device will auto-configure via DHCP
 *   4. Navigate to http://192.168.4.1/ or any domain (captive portal DNS)
 */

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "network/network.h"
#include "network/services/dhcp_service.h"
#include "network/services/dns_service.h"
#include "lwip/netif.h"
#include "lwip/ip4_addr.h"
#include <cstdio>
#include <cmath>

// Access Point configuration
#define AP_SSID "PICO_AP"
#define AP_PASSWORD "password123"

int main() {
    // Initialize stdio for debug output
    stdio_init_all();
    sleep_ms(3000);
    printf("\n");
    printf("========================================\n");
    printf("  Network Subsystem Test (AP Mode)\n");
    printf("========================================\n");
    sleep_ms(3000);
    // Initialize CYW43 wireless chip
    if (cyw43_arch_init()) {
        printf("ERROR: Failed to initialize CYW43\n");
        return 1;
    }

    // Start access point
    printf("Starting WiFi Access Point...\n");
    printf("SSID: %s\n", AP_SSID);
    printf("Password: %s\n", AP_PASSWORD);
    cyw43_arch_enable_ap_mode(AP_SSID, AP_PASSWORD, CYW43_AUTH_WPA2_AES_PSK);

    // Configure static IP for the AP (192.168.4.1)
    ip4_addr_t ipaddr, netmask, gateway;
    IP4_ADDR(&ipaddr, 192, 168, 4, 1);
    IP4_ADDR(&netmask, 255, 255, 255, 0);
    IP4_ADDR(&gateway, 192, 168, 4, 1);

    netif_set_addr(netif_default, &ipaddr, &netmask, &gateway);

    printf("Access Point started!\n");
    printf("IP Address: %s\n", ip4addr_ntoa(&ipaddr));

    // Validate IP configuration
    if (ip4_addr_isany(&ipaddr)) {
        printf("ERROR: Invalid IP address configuration\n");
        cyw43_arch_deinit();
        return 1;
    }

    // Start DHCP server
    printf("\nStarting DHCP server...\n");
    network::services::DhcpService dhcp_service;
    dhcp_service.Configure(&ipaddr, &netmask);
    if (!dhcp_service.Start()) {
        printf("ERROR: Failed to start DHCP server\n");
        cyw43_arch_deinit();
        return 1;
    }

    // Start DNS server (captive portal)
    printf("Starting DNS server...\n");
    network::services::DnsService dns_service;
    dns_service.Configure(&ipaddr);
    if (!dns_service.Start()) {
        printf("ERROR: Failed to start DNS server\n");
        dhcp_service.Stop();
        cyw43_arch_deinit();
        return 1;
    }

    // Start network subsystem
    printf("\n");
    if (!network::Start()) {
        printf("ERROR: Failed to start network subsystem\n");
        dns_service.Stop();
        dhcp_service.Stop();
        cyw43_arch_deinit();
        return 1;
    }

    printf("\n");
    printf("========================================\n");
    printf("  HTTP Server Running\n");
    printf("========================================\n");
    printf("Visit http://192.168.4.1/ in your browser\n");
    printf("Press CTRL+C to exit\n");
    printf("\n");

    // Simulate sensor data updates
    uint32_t uptime = 0;
    float simulated_force = 0.0f;
    float simulated_speed = 0.0f;

    while (true) {
        // Poll CYW43 and lwIP stack
        #if PICO_CYW43_ARCH_POLL
        cyw43_arch_poll();
        #endif

        // Update simulated sensor values
        simulated_force += 0.1f;
        if (simulated_force > 100.0f) {
            simulated_force = 0.0f;
        }

        simulated_speed = 50.0f + 10.0f * sinf(uptime * 0.01f);

        // Update network status
        network::Status::SetForce(simulated_force);
        network::Status::SetSpeed(simulated_speed);
        network::Status::SetUptime(uptime);

        // Print status every 10 seconds
        if (uptime % 10 == 0) {
            printf("[%05u] Force: %.2f, Speed: %.2f\n",
                   uptime, simulated_force, simulated_speed);
        }

        uptime++;
        sleep_ms(1000);
    }

    // Cleanup (unreachable in this test, but shows proper shutdown)
    network::Stop();
    dns_service.Stop();
    dhcp_service.Stop();
    cyw43_arch_disable_ap_mode();
    cyw43_arch_deinit();
    printf("Network stopped\n");

    return 0;
}
