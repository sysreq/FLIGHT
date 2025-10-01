#pragma once
// ============================================
#include "config/http_config.h"

#include "network/dhcp_server.h"
#include "network/dns_server.h"
#include "core/http_server.h"

#include "pico/time.h"
#include "lwip/ip_addr.h"

namespace http {

class AccessPoint {
public:
    AccessPoint() = default;
    ~AccessPoint();

    bool initialize();
    void run();
    void shutdown();
    bool is_shutdown_requested() const { return shutdown_requested_; }

    core::HttpEventHandler& event_handler() {
        return http_server_.event_handler();
    }

    core::TelemetryEventHandler& telemetry_handler() {
        return http_server_.telemetry_handler();
    }

    AccessPoint(const AccessPoint&) = delete;
    AccessPoint& operator=(const AccessPoint&) = delete;
    AccessPoint(AccessPoint&&) = delete;
    AccessPoint& operator=(AccessPoint&&) = delete;

private:
    network::DhcpServer dhcp_server_;
    network::DnsServer dns_server_;
    core::HttpServer http_server_;
    
    ip_addr_t gateway_ip_{};
    ip_addr_t netmask_{};
    absolute_time_t start_time_{};
    
    bool initialized_ = false;
    volatile bool shutdown_requested_ = false;
    
    bool setup_wifi_ap();
    bool start_servers();
    void cleanup();
};

} // namespace http