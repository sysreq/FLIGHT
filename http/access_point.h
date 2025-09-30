#pragma once

#include "config\http_config.h"

#include "servers\dhcp_server.h"
#include "servers\dns_server.h"
#include "servers\http_server.h"

namespace http {

class AccessPoint {
public:   
    AccessPoint() = default;
    ~AccessPoint();
    
    bool initialize();
    void run();
    void shutdown();
    bool is_shutdown_requested() const { return shutdown_requested_; }

    servers::HttpEventHandler& event_handler() { 
        return http_server_.event_handler(); 
    }
    
    AccessPoint(const AccessPoint&) = delete;
    AccessPoint& operator=(const AccessPoint&) = delete;
    AccessPoint(AccessPoint&&) = delete;
    AccessPoint& operator=(AccessPoint&&) = delete;

private:
    servers::DhcpServer dhcp_server_;
    servers::DnsServer dns_server_;
    servers::HttpServer http_server_;
    
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