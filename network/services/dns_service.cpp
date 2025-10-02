#include "dns_service.h"
#include "lwip/udp.h"
#include "lwip/ip_addr.h"
#include "lwip/pbuf.h"
#include "lwip/def.h"
#include <cstring>
#include <cstdio>

namespace network::services {

namespace {
    // DNS Constants
    constexpr uint16_t DNS_SERVER_PORT = 53;
    constexpr size_t DNS_MAX_MESSAGE_SIZE = 512;
    constexpr uint32_t DNS_DEFAULT_TTL_SECONDS = 60;

    struct dns_header_t {
        uint16_t id;
        uint16_t flags;
        uint16_t question_count;
        uint16_t answer_record_count;
        uint16_t authority_record_count;
        uint16_t additional_record_count;
    };
}

void DnsService::UdpReceiveCallback(void* arg, udp_pcb* /*pcb*/, pbuf* p,
                                     const ip_addr_t* addr, u16_t port) {
    auto* service = static_cast<DnsService*>(arg);
    service->ProcessRequest(p, addr, port);
}

bool DnsService::Start() {
    if (udp_ != nullptr) {
        return false;  // Already started
    }

    udp_ = udp_new();
    if (udp_ == nullptr) {
        printf("DNS: Failed to create UDP PCB\n");
        return false;
    }

    udp_recv(udp_, UdpReceiveCallback, this);

    err_t err = udp_bind(udp_, IP_ANY_TYPE, DNS_SERVER_PORT);
    if (err != ERR_OK) {
        printf("DNS: Failed to bind to port %d: %d\n", DNS_SERVER_PORT, err);
        udp_remove(udp_);
        udp_ = nullptr;
        return false;
    }

    printf("DNS: Service started on port %d\n", DNS_SERVER_PORT);
    return true;
}

void DnsService::Stop() {
    if (udp_ != nullptr) {
        udp_remove(udp_);
        udp_ = nullptr;
        printf("DNS: Service stopped\n");
    }
}

void DnsService::Process() {
    // DNS is callback-driven via UDP, nothing to do here
}

void DnsService::Configure(const ip_addr_t* router_ip) {
    ip_addr_copy(router_ip_, *router_ip);
}

void DnsService::ProcessRequest(pbuf* p, const ip_addr_t* src_addr, u16_t src_port) {
    // Auto-free pbuf on exit
    struct PbufGuard {
        pbuf* p;
        ~PbufGuard() { if (p) pbuf_free(p); }
    } guard{p};

    uint8_t dns_msg[DNS_MAX_MESSAGE_SIZE];
    dns_header_t* dns_hdr = reinterpret_cast<dns_header_t*>(dns_msg);

    size_t msg_len = pbuf_copy_partial(p, dns_msg, sizeof(dns_msg), 0);
    if (msg_len < sizeof(dns_header_t)) {
        return;
    }

    uint16_t flags = lwip_ntohs(dns_hdr->flags);
    uint16_t question_count = lwip_ntohs(dns_hdr->question_count);

    // Check QR indicates a query (bit 15 should be 0)
    if (((flags >> 15) & 0x1) != 0) {
        return;
    }

    // Check for standard query (opcode should be 0)
    if (((flags >> 11) & 0xf) != 0) {
        return;
    }

    // Check question count
    if (question_count < 1) {
        return;
    }

    // Parse question section to find end of QNAME
    const uint8_t* question_ptr_start = dns_msg + sizeof(dns_header_t);
    const uint8_t* question_ptr_end = dns_msg + msg_len;
    const uint8_t* question_ptr = question_ptr_start;

    while (question_ptr < question_ptr_end) {
        if (*question_ptr == 0) {
            question_ptr++;
            break;
        } else {
            int label_len = *question_ptr++;
            if (label_len > 63) {
                return;  // Invalid label length
            }
            question_ptr += label_len;
        }
    }

    // Check question length
    if (question_ptr - question_ptr_start > 255) {
        return;
    }

    // Skip QTYPE and QCLASS (4 bytes)
    question_ptr += 4;

    // Generate answer (respond with our IP for all queries - captive portal behavior)
    uint8_t* answer_ptr = dns_msg + (question_ptr - dns_msg);

    // NAME: pointer to question name
    *answer_ptr++ = 0xc0;
    *answer_ptr++ = static_cast<uint8_t>(question_ptr_start - dns_msg);

    // TYPE: A record (host address)
    *answer_ptr++ = 0;
    *answer_ptr++ = 1;

    // CLASS: Internet
    *answer_ptr++ = 0;
    *answer_ptr++ = 1;

    // TTL: 60 seconds
    *answer_ptr++ = 0;
    *answer_ptr++ = 0;
    *answer_ptr++ = 0;
    *answer_ptr++ = DNS_DEFAULT_TTL_SECONDS;

    // RDLENGTH: 4 bytes (IPv4 address)
    *answer_ptr++ = 0;
    *answer_ptr++ = 4;

    // RDATA: IP address
    std::memcpy(answer_ptr, &router_ip_.addr, 4);
    answer_ptr += 4;

    // Update DNS header for response
    dns_hdr->flags = lwip_htons(
        0x1 << 15 |  // QR = response
        0x1 << 10 |  // AA = authoritative
        0x1 << 7);   // RA = recursion available
    dns_hdr->question_count = lwip_htons(1);
    dns_hdr->answer_record_count = lwip_htons(1);
    dns_hdr->authority_record_count = 0;
    dns_hdr->additional_record_count = 0;

    SendReply(dns_msg, answer_ptr - dns_msg, src_addr, src_port);
}

int DnsService::SendReply(const void* buf, size_t len, const ip_addr_t* dest, uint16_t port) {
    if (len > 0xffff) {
        len = 0xffff;
    }

    pbuf* p = pbuf_alloc(PBUF_TRANSPORT, len, PBUF_RAM);
    if (p == nullptr) {
        printf("DNS: Failed to allocate pbuf for response\n");
        return -1;
    }

    std::memcpy(p->payload, buf, len);
    err_t err = udp_sendto(udp_, p, dest, port);

    pbuf_free(p);

    if (err != ERR_OK) {
        printf("DNS: Failed to send response: %d\n", err);
        return -1;
    }

    return len;
}

} // namespace network::services
