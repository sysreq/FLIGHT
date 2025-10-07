#pragma once

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "app/SystemCore.h"
#include "app/Scheduler.h"
#include "sdcard.h"
#include "network/network.h"
#include "lwip/netif.h"
#include "lwip/ip4_addr.h"

class Core0Controller : public SystemCore<Core0Controller> {
private:
    friend class SystemCore<Core0Controller>;

    Scheduler scheduler_;

    bool start_filesystem() {
        using namespace sdcard;
        if (!SDCard::mount()) { return false; }
        if (!SDFile<LogFile>::Open()) { return false; }
        if (!SDFile<Force>::Open()) { return false; }
        if (!SDFile<Current>::Open()) { return false; }
        if (!SDFile<Speed>::Open()) { return false; }
        
        printf("Core 0: File System Started.\n");
        SDFile<LogFile>::Write("Device Started.\n");
        SDFile<LogFile>::Sync();
        return true;
    }

    bool start_network() {
        if (cyw43_arch_init() != 0) {
            printf("Core 0: Failed to initialize CYW43\n");
            return false;
        }

        const char* ap_ssid = "STARv4";
        const char* ap_pass = "hunteradams";
        cyw43_arch_enable_ap_mode(ap_ssid, ap_pass, CYW43_AUTH_WPA2_AES_PSK);

        ip4_addr_t ipaddr, netmask, gateway;
        IP4_ADDR(&ipaddr, 192, 168, 4, 1);
        IP4_ADDR(&netmask, 255, 255, 255, 0);
        IP4_ADDR(&gateway, 192, 168, 4, 1);
        netif_set_addr(netif_default, &ipaddr, &netmask, &gateway);

        printf("Core 0: WiFi Access Point started at %s\n", ip4addr_ntoa(&ipaddr));

        if (!network::Start()) {
            printf("Core 0: Failed to start network subsystem\n");
            return false;
        }
        printf("Core 0: HTTP Server Running.\n");
        sdcard::SDFile<sdcard::LogFile>::Write("HTTP Server Running.\n");
        sdcard::SDFile<sdcard::LogFile>::Sync();
        return true;
    }

    bool init_impl() {
        printf("Core 0: Initializing...\n");
        if (!start_filesystem()) {
            printf("Core 0: Filesystem failed to start.\n");
            return false;
        }
        if (!start_network()) {
            printf("Core 0: Network failed to start.\n");
            return false;
        }

        scheduler_.add_task([]() {
            cyw43_arch_poll();
            network::Process();
        }, 10);

        printf("Core 0: Initialized successfully.\n");
        return true;
    }

    void loop_impl() {
        while (true) {
            scheduler_.run();
            sleep_ms(1);
        }
    }
};
