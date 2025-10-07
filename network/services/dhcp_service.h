#pragma once
#include "../platform/lwip_wrapper.h"
#include "lwip/ip_addr.h"
#include <array>
#include <cstdint>

// Forward declarations
struct udp_pcb;
struct pbuf;
struct netif;
typedef unsigned short u16_t;

namespace network::services {

class DhcpService {
public:
    static constexpr uint8_t DHCP_BASE_IP = 2;      // Start at .2 (router is .1)
    static constexpr uint8_t DHCP_MAX_CLIENTS = 8;  // Support up to 8 clients
    static constexpr size_t MAC_ADDRESS_LENGTH = 6;

    // Service concept interface
    bool Start();
    void Stop();
    void Process();  // Empty - DHCP uses UDP callbacks

    // Configuration
    void Configure(const ip_addr_t* router_ip, const ip_addr_t* netmask);

private:
    struct Lease {
        std::array<uint8_t, MAC_ADDRESS_LENGTH> mac{};
        uint16_t expiry = 0;  // Expiry time in 65.5 second units
    };

    udp_pcb* udp_ = nullptr;
    ip_addr_t router_ip_{};
    ip_addr_t netmask_{};
    std::array<Lease, DHCP_MAX_CLIENTS> leases_{};

    // DHCP processing (called from static callback)
    void ProcessRequest(pbuf* p, const ip_addr_t* src_addr, u16_t src_port);

    // Helper functions
    int FindLeaseSlot(const uint8_t* mac);
    const uint8_t* FindOption(const uint8_t* opt, uint8_t cmd) const;
    uint8_t* WriteOption(uint8_t* opt, uint8_t cmd, uint8_t val);
    uint8_t* WriteOption(uint8_t* opt, uint8_t cmd, uint32_t val);
    uint8_t* WriteOption(uint8_t* opt, uint8_t cmd, size_t len, const void* data);
    bool SendReply(netif* nif, const void* buf, size_t len, uint32_t dest_ip, uint16_t dest_port);

    // Static callback for LWIP
    static void UdpReceiveCallback(void* arg, udp_pcb* pcb, pbuf* p, const ip_addr_t* addr, u16_t port);

    friend void UdpReceiveCallback(void*, udp_pcb*, pbuf*, const ip_addr_t*, u16_t);
};

} // namespace network::services
