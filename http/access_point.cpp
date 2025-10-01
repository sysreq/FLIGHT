#include "access_point.h"
#include "http_platform.h"
#include <cstdio>

namespace http {

AccessPoint::~AccessPoint() {
    if (initialized_) {
        dhcp_server_.stop();
        dns_server_.stop();
        http_server_.stop();
        cyw43_arch_deinit();
    }
}

bool AccessPoint::initialize() {
    if (initialized_) {
        printf("AccessPoint already initialized\n");
        return false;
    }
    
    // Initialize CYW43 architecture
    if (cyw43_arch_init()) {
        printf("Failed to initialize CYW43\n");
        return false;
    }
    
    // Configure IP addresses using centralized config
    IP4_ADDR(ip_2_ip4(&gateway_ip_), 
             (config::access_point::DEFAULT_IP >> 24) & 0xff,
             (config::access_point::DEFAULT_IP >> 16) & 0xff,
             (config::access_point::DEFAULT_IP >> 8) & 0xff,
             config::access_point::DEFAULT_IP & 0xff);
    
    IP4_ADDR(ip_2_ip4(&netmask_),
             (config::access_point::DEFAULT_NETMASK >> 24) & 0xff,
             (config::access_point::DEFAULT_NETMASK >> 16) & 0xff,
             (config::access_point::DEFAULT_NETMASK >> 8) & 0xff,
             config::access_point::DEFAULT_NETMASK & 0xff);
    
    // Setup WiFi access point with valid IP addresses
    if (!setup_wifi_ap()) {
        cyw43_arch_deinit();
        return false;
    }
    
    // Start all servers
    if (!start_servers()) {
        cyw43_arch_deinit();
        return false;
    }
    
    start_time_ = get_absolute_time();
    initialized_ = true;
    
    printf("Access Point started\n");
    printf("SSID: %s\n", config::access_point::DEFAULT_SSID);
    printf("IP: %u.%u.%u.%u\n",
           (config::access_point::DEFAULT_IP >> 24) & 0xff,
           (config::access_point::DEFAULT_IP >> 16) & 0xff,
           (config::access_point::DEFAULT_IP >> 8) & 0xff,
           config::access_point::DEFAULT_IP & 0xff);
    
    return true;
}

bool AccessPoint::setup_wifi_ap() {
    cyw43_arch_enable_ap_mode(
        config::access_point::DEFAULT_SSID, 
        config::access_point::DEFAULT_PASSWORD, 
        config::access_point::AUTH_MODE);
        
    printf("WiFi AP mode enabled\n");
    return true;
}

bool AccessPoint::start_servers() {
    if (ip4_addr_isany_val(*ip_2_ip4(&gateway_ip_))) {
        printf("Invalid gateway IP, cannot start servers\n");
        return false;
    }
    
    // Start DHCP server
    if (!dhcp_server_.start(&gateway_ip_, &netmask_)) {
        printf("Failed to start DHCP server\n");
        return false;
    }
    
    // Start DNS server (captive portal)
    if (!dns_server_.start(&gateway_ip_)) {
        printf("Failed to start DNS server\n");
        dhcp_server_.stop();
        return false;
    }
    
    // Start HTTP server
    if (!http_server_.start(&gateway_ip_, start_time_)) {
        printf("Failed to start HTTP server\n");
        dns_server_.stop();
        dhcp_server_.stop();
        return false;
    }
    
    return true;
}

void AccessPoint::run() {
    if (!initialized_) {
        printf("AccessPoint not initialized\n");
        return;
    }
    
    while (!shutdown_requested_) {
        #if PICO_CYW43_ARCH_POLL
        cyw43_arch_poll();
        cyw43_arch_wait_for_work_until(make_timeout_time_ms(config::access_point::POLL_INTERVAL_MS));
        #else
        sleep_ms(config::access_point::POLL_INTERVAL_MS);
        #endif
    }
    
    // Clean up when shutdown is requested
    cleanup();
}

void AccessPoint::shutdown() {
    printf("Shutting down Access Point\n");
    shutdown_requested_ = true;
}

void AccessPoint::cleanup() {
    if (initialized_) {
        printf("Stopping servers\n");
        http_server_.stop();
        dns_server_.stop();
        dhcp_server_.stop();
        
        printf("Disabling WiFi AP\n");
        cyw43_arch_disable_ap_mode();
        
        initialized_ = false;
    }
}

} // namespace http