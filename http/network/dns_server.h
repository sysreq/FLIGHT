#pragma once

#include <cstddef>
#include <cstdint>
#include "../config/http_config.h"

// Forward declare LWIP types for pointers/parameters
struct udp_pcb;
struct pbuf;
typedef unsigned short u16_t;

// Need full type for member variable
#include "lwip/ip_addr.h"

namespace http::network {

class DnsServer {
public:
    DnsServer() = default;
    ~DnsServer();

    bool start(const ip_addr_t* ip);
    void stop();
    bool is_running() const { return udp_ != nullptr; }
    
    void process_request(struct ::pbuf* p, const ip_addr_t* src_addr, u16_t src_port);

    DnsServer(const DnsServer&) = delete;
    DnsServer& operator=(const DnsServer&) = delete;
    DnsServer(DnsServer&&) = delete;
    DnsServer& operator=(DnsServer&&) = delete;

private:
    struct udp_pcb* udp_ = nullptr;
    ip_addr_t ip_{};
};

} // namespace http::network