#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include "../config/http_config.h"

// Forward declare LWIP types for pointers/parameters
struct udp_pcb;
struct pbuf;
struct netif;
typedef unsigned short u16_t;

// Need full type for member variables
#include "lwip/ip_addr.h"

namespace http::network {

class DhcpServer {
public:
    static constexpr uint8_t DHCPS_BASE_IP = config::dhcp::BASE_IP;
    static constexpr uint8_t DHCPS_MAX_IP = config::dhcp::MAX_CLIENTS;
    static constexpr size_t MAC_LEN = config::dhcp::MAC_ADDRESS_LENGTH;

    struct Lease {
        std::array<uint8_t, MAC_LEN> mac{};
        uint16_t expiry = 0;
    };

    DhcpServer() = default;
    ~DhcpServer();

    bool start(const ip_addr_t* ip, const ip_addr_t* netmask);
    void stop();
    bool is_running() const { return udp_ != nullptr; }
    
    void process_request(struct ::pbuf* p, const ip_addr_t* src_addr, u16_t src_port);

    DhcpServer(const DhcpServer&) = delete;
    DhcpServer& operator=(const DhcpServer&) = delete;
    DhcpServer(DhcpServer&&) = delete;
    DhcpServer& operator=(DhcpServer&&) = delete;

private:
    struct udp_pcb* udp_ = nullptr;
    ip_addr_t ip_{};
    ip_addr_t netmask_{};
    std::array<Lease, DHCPS_MAX_IP> leases_{};
    
    uint8_t* write_option(uint8_t* opt, uint8_t cmd, uint8_t val);
    uint8_t* write_option(uint8_t* opt, uint8_t cmd, uint32_t val);
    uint8_t* write_option(uint8_t* opt, uint8_t cmd, size_t len, const void* data);
    
    const uint8_t* find_option(const uint8_t* opt, uint8_t cmd) const;
    int find_lease_slot(const uint8_t* mac);
    bool send_reply(struct ::netif* nif, const void* buf, size_t len, uint32_t dest_ip, uint16_t dest_port);
};

} // namespace http::network