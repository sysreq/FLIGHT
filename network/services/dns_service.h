#pragma once
#include "lwip/ip_addr.h"
#include <cstdint>

// Forward declarations
struct udp_pcb;
struct pbuf;
typedef unsigned short u16_t;

namespace network::services {

class DnsService {
public:
    // Service concept interface
    bool Start();
    void Stop();
    void Process();  // Empty - DNS uses UDP callbacks

    // Configuration
    void Configure(const ip_addr_t* router_ip);

private:
    udp_pcb* udp_ = nullptr;
    ip_addr_t router_ip_{};

    // DNS processing (called from static callback)
    void ProcessRequest(pbuf* p, const ip_addr_t* src_addr, u16_t src_port);

    // Helper function
    int SendReply(const void* buf, size_t len, const ip_addr_t* dest, uint16_t port);

    // Static callback for LWIP
    static void UdpReceiveCallback(void* arg, udp_pcb* pcb, pbuf* p, const ip_addr_t* addr, u16_t port);

    friend void UdpReceiveCallback(void*, udp_pcb*, pbuf*, const ip_addr_t*, u16_t);
};

} // namespace network::services
