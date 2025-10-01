#include "dns_server.h"
#include "../http_platform.h"

extern "C" void dns_server_process_callback(void* arg, struct udp_pcb* upcb, struct pbuf* p,
                                           const ip_addr_t* src_addr, u16_t src_port) {
    auto* server = static_cast<http::network::DnsServer*>(arg);
    server->process_request(p, src_addr, src_port);
}
namespace {

// Use config constants instead of duplicating them
using config::dns::SERVER_PORT;
using config::dns::MAX_MESSAGE_SIZE;
using config::dns::DEFAULT_TTL_SECONDS;

struct dns_header_t {
    uint16_t id;
    uint16_t flags;
    uint16_t question_count;
    uint16_t answer_record_count;
    uint16_t authority_record_count;
    uint16_t additional_record_count;
};

static int dns_socket_sendto(struct udp_pcb* udp, const void* buf, size_t len, 
                            const ip_addr_t* dest, uint16_t port) {
    if (len > 0xffff) {
        len = 0xffff;
    }

    struct ::pbuf* p = pbuf_alloc(PBUF_TRANSPORT, len, PBUF_RAM);
    if (p == nullptr) {
        printf("DNS: Failed to send message out of memory\n");
        return -ENOMEM;
    }

    std::memcpy(p->payload, buf, len);
    err_t err = udp_sendto(udp, p, dest, port);

    pbuf_free(p);

    if (err != ERR_OK) {
        printf("DNS: Failed to send message %d\n", err);
        return err;
    }

    return len;
}

} // anonymous namespace

namespace http::network {

DnsServer::~DnsServer() {
    stop();
}

bool DnsServer::start(const ip_addr_t* ip) {
    if (udp_ != nullptr) {
        return false;
    }

    udp_ = udp_new();
    if (udp_ == nullptr) {
        return false;
    }

    udp_recv(udp_, ::dns_server_process_callback, this);

    err_t err = udp_bind(udp_, IP_ANY_TYPE, SERVER_PORT);
    if (err != ERR_OK) {
        printf("DNS server failed to bind to port %u: %d\n", SERVER_PORT, err);
        udp_remove(udp_);
        udp_ = nullptr;
        return false;
    }

    ip_addr_copy(ip_, *ip);
    printf("DNS server listening on port %d\n", SERVER_PORT);
    return true;
}

void DnsServer::stop() {
    if (udp_ != nullptr) {
        udp_remove(udp_);
        udp_ = nullptr;
    }
}

void DnsServer::process_request(struct ::pbuf* p, const ip_addr_t* src_addr, u16_t src_port) {
    // RAII wrapper for pbuf cleanup
    struct PbufGuard {
        struct ::pbuf* p;  // Add :: here too
        ~PbufGuard() { if (p) pbuf_free(p); }
    } pbuf_guard{p};

    uint8_t dns_msg[MAX_MESSAGE_SIZE];
    dns_header_t* dns_hdr = reinterpret_cast<dns_header_t*>(dns_msg);

    size_t msg_len = pbuf_copy_partial(p, dns_msg, sizeof(dns_msg), 0);
    if (msg_len < sizeof(dns_header_t)) {
        return;
    }

    uint16_t flags = lwip_ntohs(dns_hdr->flags);
    uint16_t question_count = lwip_ntohs(dns_hdr->question_count);

    // Check QR indicates a query
    if (((flags >> 15) & 0x1) != 0) {
        return;
    }

    // Check for standard query
    if (((flags >> 11) & 0xf) != 0) {
        return;
    }

    // Check question count
    if (question_count < 1) {
        return;
    }

    const uint8_t* question_ptr_start = dns_msg + sizeof(dns_header_t);
    const uint8_t* question_ptr_end = dns_msg + msg_len;
    const uint8_t* question_ptr = question_ptr_start;
    
    while(question_ptr < question_ptr_end) {
        if (*question_ptr == 0) {
            question_ptr++;
            break;
        } else {
            int label_len = *question_ptr++;
            if (label_len > 63) {
                return;
            }
            question_ptr += label_len;
        }
    }

    // Check question length
    if (question_ptr - question_ptr_start > 255) {
        return;
    }

    // Skip QNAME and QTYPE
    question_ptr += 4;

    // Generate answer
    uint8_t* answer_ptr = dns_msg + (question_ptr - dns_msg);
    *answer_ptr++ = 0xc0;
    *answer_ptr++ = static_cast<uint8_t>(question_ptr_start - dns_msg);
    
    *answer_ptr++ = 0;
    *answer_ptr++ = 1; // host address

    *answer_ptr++ = 0;
    *answer_ptr++ = 1; // Internet class

    *answer_ptr++ = 0;
    *answer_ptr++ = 0;
    *answer_ptr++ = 0;
    *answer_ptr++ = 60; // ttl 60s

    *answer_ptr++ = 0;
    *answer_ptr++ = 4; // length
    std::memcpy(answer_ptr, &ip_.addr, 4);
    answer_ptr += 4;

    dns_hdr->flags = lwip_htons(
                0x1 << 15 | // QR = response
                0x1 << 10 | // AA = authoritative
                0x1 << 7);   // RA = authenticated
    dns_hdr->question_count = lwip_htons(1);
    dns_hdr->answer_record_count = lwip_htons(1);
    dns_hdr->authority_record_count = 0;
    dns_hdr->additional_record_count = 0;

    dns_socket_sendto(udp_, dns_msg, answer_ptr - dns_msg, src_addr, src_port);
}

} // namespace http::network