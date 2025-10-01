#include "dhcp_server.h"
#include "../http_platform.h"
#include "cyw43_config.h"

extern "C" void dhcp_server_process_callback(void* arg, struct udp_pcb* upcb, struct pbuf* p,
                                            const ip_addr_t* src_addr, u16_t src_port) {
    auto* server = static_cast<http::network::DhcpServer*>(arg);
    server->process_request(p, src_addr, src_port);
}

namespace {

// DHCP Message Types
constexpr uint8_t DHCPDISCOVER = 1;
constexpr uint8_t DHCPOFFER = 2;
constexpr uint8_t DHCPREQUEST = 3;
constexpr uint8_t DHCPACK = 5;

// DHCP Options
constexpr uint8_t DHCP_OPT_SUBNET_MASK = 1;
constexpr uint8_t DHCP_OPT_ROUTER = 3;
constexpr uint8_t DHCP_OPT_DNS = 6;
constexpr uint8_t DHCP_OPT_REQUESTED_IP = 50;
constexpr uint8_t DHCP_OPT_IP_LEASE_TIME = 51;
constexpr uint8_t DHCP_OPT_MSG_TYPE = 53;
constexpr uint8_t DHCP_OPT_SERVER_ID = 54;
constexpr uint8_t DHCP_OPT_END = 255;

// Use config constants instead of duplicating them
using config::dhcp::SERVER_PORT;
using config::dhcp::CLIENT_PORT;
using config::dhcp::LEASE_TIME_SECONDS;
using config::dhcp::MIN_MESSAGE_SIZE;

struct dhcp_msg_t {
    uint8_t op;
    uint8_t htype;
    uint8_t hlen;
    uint8_t hops;
    uint32_t xid;
    uint16_t secs;
    uint16_t flags;
    uint8_t ciaddr[4];
    uint8_t yiaddr[4];
    uint8_t siaddr[4];
    uint8_t giaddr[4];
    uint8_t chaddr[16];
    uint8_t sname[64];
    uint8_t file[128];
    uint8_t options[312];
};

} // anonymous namespace

namespace http::network {

DhcpServer::~DhcpServer() {
    stop();
}

bool DhcpServer::start(const ip_addr_t* ip, const ip_addr_t* netmask) {
    if (udp_ != nullptr) {
        return false;
    }

    udp_ = udp_new();
    if (udp_ == nullptr) {
        return false;
    }

    udp_recv(udp_, ::dhcp_server_process_callback, this);

    err_t err = udp_bind(udp_, IP_ANY_TYPE, SERVER_PORT);
    if (err != ERR_OK) {
        udp_remove(udp_);
        udp_ = nullptr;
        return false;
    }

    ip_addr_copy(ip_, *ip);
    ip_addr_copy(netmask_, *netmask);
    
    // Clear lease table
    for (auto& lease : leases_) {
        lease.mac.fill(0);
        lease.expiry = 0;
    }
    
    return true;
}

void DhcpServer::stop() {
    if (udp_ != nullptr) {
        udp_remove(udp_);
        udp_ = nullptr;
    }
}

void DhcpServer::process_request(struct ::pbuf* p, const ip_addr_t* src_addr, u16_t src_port) {
    struct PbufGuard {
        struct ::pbuf* p;
        ~PbufGuard() { if (p) pbuf_free(p); }
    } pbuf_guard{p};

    if (p->tot_len < MIN_MESSAGE_SIZE) {
        return;
    }

    dhcp_msg_t dhcp_msg;
    size_t len = pbuf_copy_partial(p, &dhcp_msg, sizeof(dhcp_msg), 0);
    if (len < MIN_MESSAGE_SIZE) {
        return;
    }

    dhcp_msg.op = DHCPOFFER;
    std::memcpy(&dhcp_msg.yiaddr, &ip4_addr_get_u32(ip_2_ip4(&ip_)), 4);

    uint8_t* opt = dhcp_msg.options + 4;  // Skip magic cookie
    const uint8_t* msgtype = find_option(opt, DHCP_OPT_MSG_TYPE);
    
    if (msgtype == nullptr) {
        return;
    }

    uint8_t* opt_ptr = opt;
    
    switch (msgtype[2]) {
        case DHCPDISCOVER: {
            int yi = find_lease_slot(dhcp_msg.chaddr);
            if (yi == DHCPS_MAX_IP) {
                return;  // No IP addresses available
            }
            
            dhcp_msg.yiaddr[3] = DHCPS_BASE_IP + yi;
            opt_ptr = write_option(opt_ptr, DHCP_OPT_MSG_TYPE, DHCPOFFER);
            break;
        }

        case DHCPREQUEST: {
            const uint8_t* requested_ip = find_option(opt, DHCP_OPT_REQUESTED_IP);
            if (requested_ip == nullptr) {
                return;
            }
            
            if (std::memcmp(requested_ip + 2, &ip4_addr_get_u32(ip_2_ip4(&ip_)), 3) != 0) {
                return;
            }
            
            uint8_t yi = requested_ip[5] - DHCPS_BASE_IP;
            if (yi >= DHCPS_MAX_IP) {
                return;
            }
            
            auto& lease = leases_[yi];
            if (std::memcmp(lease.mac.data(), dhcp_msg.chaddr, MAC_LEN) == 0) {
                // MAC match
            } else if (std::all_of(lease.mac.begin(), lease.mac.end(), [](uint8_t b){ return b == 0; })) {
                // IP unused, claim it
                std::memcpy(lease.mac.data(), dhcp_msg.chaddr, MAC_LEN);
            } else {
                return;  // IP already in use by different MAC
            }
            
            lease.expiry = static_cast<uint16_t>((cyw43_hal_ticks_ms() + LEASE_TIME_SECONDS * 1000) >> 16);
            dhcp_msg.yiaddr[3] = DHCPS_BASE_IP + yi;
            opt_ptr = write_option(opt_ptr, DHCP_OPT_MSG_TYPE, DHCPACK);
            
            printf("DHCP: Client connected %02x:%02x:%02x:%02x:%02x:%02x -> %u.%u.%u.%u\n",
                dhcp_msg.chaddr[0], dhcp_msg.chaddr[1], dhcp_msg.chaddr[2], 
                dhcp_msg.chaddr[3], dhcp_msg.chaddr[4], dhcp_msg.chaddr[5],
                dhcp_msg.yiaddr[0], dhcp_msg.yiaddr[1], dhcp_msg.yiaddr[2], dhcp_msg.yiaddr[3]);
            break;
        }

        default:
            return;
    }

    // Add common DHCP options
    opt_ptr = write_option(opt_ptr, DHCP_OPT_SERVER_ID, 4, &ip4_addr_get_u32(ip_2_ip4(&ip_)));
    opt_ptr = write_option(opt_ptr, DHCP_OPT_SUBNET_MASK, 4, &ip4_addr_get_u32(ip_2_ip4(&netmask_)));
    opt_ptr = write_option(opt_ptr, DHCP_OPT_ROUTER, 4, &ip4_addr_get_u32(ip_2_ip4(&ip_)));
    opt_ptr = write_option(opt_ptr, DHCP_OPT_DNS, 4, &ip4_addr_get_u32(ip_2_ip4(&ip_)));
    opt_ptr = write_option(opt_ptr, DHCP_OPT_IP_LEASE_TIME, LEASE_TIME_SECONDS);
    *opt_ptr++ = DHCP_OPT_END;
    
    struct ::netif* nif = ip_current_input_netif();
    send_reply(nif, &dhcp_msg, opt_ptr - reinterpret_cast<uint8_t*>(&dhcp_msg), 0xffffffff, CLIENT_PORT);
}

int DhcpServer::find_lease_slot(const uint8_t* mac) {
    int free_slot = DHCPS_MAX_IP;
    
    for (int i = 0; i < DHCPS_MAX_IP; ++i) {
        auto& lease = leases_[i];
        
        // Existing lease for this MAC
        if (std::memcmp(lease.mac.data(), mac, MAC_LEN) == 0) {
            return i;
        }
        
        // Track first free slot
        if (free_slot == DHCPS_MAX_IP) {
            bool is_empty = std::all_of(lease.mac.begin(), lease.mac.end(), 
                                       [](uint8_t b){ return b == 0; });
            if (is_empty) {
                free_slot = i;
            } else {
                // Check if lease expired
                uint32_t expiry = static_cast<uint32_t>(lease.expiry) << 16 | 0xffff;
                if (static_cast<int32_t>(expiry - cyw43_hal_ticks_ms()) < 0) {
                    lease.mac.fill(0);
                    free_slot = i;
                }
            }
        }
    }
    
    return free_slot;
}

const uint8_t* DhcpServer::find_option(const uint8_t* opt, uint8_t cmd) const {
    for (int i = 0; i < 308 && opt[i] != DHCP_OPT_END;) {
        if (opt[i] == cmd) {
            return &opt[i];
        }
        i += 2 + opt[i + 1];
    }
    return nullptr;
}

uint8_t* DhcpServer::write_option(uint8_t* opt, uint8_t cmd, uint8_t val) {
    *opt++ = cmd;
    *opt++ = 1;
    *opt++ = val;
    return opt;
}

uint8_t* DhcpServer::write_option(uint8_t* opt, uint8_t cmd, uint32_t val) {
    *opt++ = cmd;
    *opt++ = 4;
    *opt++ = val >> 24;
    *opt++ = val >> 16;
    *opt++ = val >> 8;
    *opt++ = val;
    return opt;
}

uint8_t* DhcpServer::write_option(uint8_t* opt, uint8_t cmd, size_t len, const void* data) {
    *opt++ = cmd;
    *opt++ = static_cast<uint8_t>(len);
    std::memcpy(opt, data, len);
    return opt + len;
}

bool DhcpServer::send_reply(struct ::netif* nif, const void* buf, size_t len, uint32_t dest_ip, uint16_t dest_port) {
    if (len > 0xffff) {
        len = 0xffff;
    }

    struct ::pbuf* p = pbuf_alloc(PBUF_TRANSPORT, len, PBUF_RAM);
    if (p == nullptr) {
        return false;
    }

    std::memcpy(p->payload, buf, len);

    ip_addr_t dest;
    IP4_ADDR(ip_2_ip4(&dest), dest_ip >> 24 & 0xff, dest_ip >> 16 & 0xff, 
             dest_ip >> 8 & 0xff, dest_ip & 0xff);
    
    err_t err = (nif != nullptr) 
        ? udp_sendto_if(udp_, p, &dest, dest_port, nif)
        : udp_sendto(udp_, p, &dest, dest_port);

    pbuf_free(p);
    return err == ERR_OK;
}

} // namespace http::network