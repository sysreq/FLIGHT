#include "pico/stdlib.h"

#include "core.h"
#include "sdcard.h"

#include "pico/cyw43_arch.h"
#include "network/network.h"
#include "network/services/dhcp_service.h"
#include "network/services/dns_service.h"
#include "lwip/netif.h"
#include "lwip/ip4_addr.h"
#include <cstdio>
#include <cmath>

// Access Point configuration
#define AP_SSID "STARv4"
#define AP_PASSWORD "hunteradams"

namespace core0 {

static auto &local_state(){
    static struct {
        uint32_t data;
    } local_state;
    return local_state;
}


bool start_filesystem() {
    using namespace sdcard;
    REQUIRE(SDCard::mount(), "Failed to mount SDCard.\n");
    REQUIRE(SDFile<LogFile>::Open(), "Failed to initialize LogFile.\n");
    REQUIRE(SDFile<Force>::Open(), "Failed to initialize load cell result file.\n");
    REQUIRE(SDFile<Current>::Open(), "Failed to initialize Mauch 250U result file.\n");
    REQUIRE(SDFile<Speed>::Open(), "Failed to initialize Pitot tube result file.\n");
    sleep_ms(100);
    printf("File System Started.\n");
    SDFile<LogFile>::Write("Device Started.\n");
    SDFile<LogFile>::Sync();
    return true;
}

bool start_control_panel() {
    using namespace sdcard;
    printf("Starting WiFi...\n");
    sleep_ms(10);
    REQUIRE(cyw43_arch_init(), "ERROR: Failed to initialize CYW43\n");

    printf("Starting WiFi Access Point...\n");
    printf("SSID: %s\n", AP_SSID);
    printf("Password: %s\n", AP_PASSWORD);
    cyw43_arch_enable_ap_mode(AP_SSID, AP_PASSWORD, CYW43_AUTH_WPA2_AES_PSK);

    ip4_addr_t ipaddr, netmask, gateway;
    IP4_ADDR(&ipaddr, 192, 168, 4, 1);
    IP4_ADDR(&netmask, 255, 255, 255, 0);
    IP4_ADDR(&gateway, 192, 168, 4, 1);

    netif_set_addr(netif_default, &ipaddr, &netmask, &gateway);

    printf("Access Point started!\n");
    printf("IP Address: %s\n", ip4addr_ntoa(&ipaddr));

    // Start network subsystem (auto-configures HTTP, DHCP, and DNS)
    printf("\n");
    if (!network::Start()) {
        printf("ERROR: Failed to start network subsystem\n");
        cyw43_arch_deinit();
        return 1;
    }

    printf("HTTP Server Running\n");
    SDFile<LogFile>::Write("HTTP Server Running\n");
    SDFile<LogFile>::Write("Starting WiFi Access Point...\n");
    SDFile<LogFile>::Write("SSID: %s\n", AP_SSID);
    SDFile<LogFile>::Write("Password: %s\n", AP_PASSWORD);
    SDFile<LogFile>::Sync();
    sleep_ms(50);
    return true;
}

bool init() {
    using namespace sdcard;
    REQUIRE(start_filesystem(), "Filesystem failed. Shutting down.\n");
    REQUIRE(start_control_panel(), "Failed to start remote control. Shutting down.\n");

    sleep_ms(50);
    printf("Core 0 Started.\n");
    SDFile<LogFile>::Write("Core 0 Started.\n");
    SDFile<LogFile>::Sync();
    sleep_ms(50);
    return true;
}

static constexpr uint32_t NETWORK_POlL_RATE = 5 * 1000;

bool loop() {

    uint32_t last_network_service = time_us_32();
    uint32_t now = 0;
    int i = 0;
    while (true) {
        now = time_us_32();
        if(now - last_network_service >= NETWORK_POlL_RATE) {
            cyw43_arch_poll();
            network::Process();
            last_network_service = now;
        }

        sleep_ms(1);
    }

}

void shutdown() {
    // network::Stop();
    // dns_service.Stop();
    // dhcp_service.Stop();
    // cyw43_arch_disable_ap_mode();
    // cyw43_arch_deinit();
    // printf("Network stopped\n");
    return;
}

} // namespace core0