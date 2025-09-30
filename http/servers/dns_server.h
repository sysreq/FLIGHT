#pragma once

#include "..\config\http_config.h"

namespace http::servers {

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

} // namespace http::servers