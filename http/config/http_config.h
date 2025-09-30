#pragma once

#include <algorithm>
#include <array>
#include <cstring>
#include <cstdlib>
#include <string>
#include <queue>


#include "pico\critical_section.h"

#include "lwipopts.h"

#include <pico\cyw43_arch.h>

#include <lwip\ip_addr.h>
#include <lwip\udp.h>
#include <lwip\tcp.h>
#include <lwip\errno.h>
#include <lwip\pbuf.h>

namespace config {

// ============================================
// ACCESS POINT CONFIGURATION
// ============================================
namespace access_point {
    // Network credentials
    static constexpr const char* DEFAULT_SSID = "PicoAP";
    static constexpr const char* DEFAULT_PASSWORD = "password123";
    
    // Network addressing
    static constexpr uint32_t DEFAULT_IP = 0xC0A80401;         // 192.168.4.1
    static constexpr uint32_t DEFAULT_NETMASK = 0xFFFFFF00;    // 255.255.255.0
    
    // Security
    static constexpr uint32_t AUTH_MODE = CYW43_AUTH_WPA2_AES_PSK;
    
    // Polling configuration
    static constexpr uint32_t POLL_INTERVAL_MS = 100;          // CYW43 poll interval
}

// ============================================
// DHCP SERVER CONFIGURATION
// ============================================
namespace dhcp {
    // Port configuration
    static constexpr uint16_t SERVER_PORT = 67;
    static constexpr uint16_t CLIENT_PORT = 68;
    
    // IP address pool
    static constexpr uint8_t BASE_IP = 16;                     // Start at x.x.x.16
    static constexpr uint8_t MAX_CLIENTS = 8;                  // Max 8 IP addresses
    
    // Lease configuration
    static constexpr uint32_t LEASE_TIME_SECONDS = 24 * 60 * 60;  // 24 hours
    
    // Protocol settings
    static constexpr size_t MIN_MESSAGE_SIZE = 243;            // 240 + 3 for minimal DHCP
    static constexpr size_t MAX_OPTIONS_SIZE = 312;            // DHCP options field size
    
    // Hardware addressing
    static constexpr size_t MAC_ADDRESS_LENGTH = 6;
}

// ============================================
// DNS SERVER CONFIGURATION  
// ============================================
namespace dns {
    // Port configuration
    static constexpr uint16_t SERVER_PORT = 53;
    
    // Protocol settings
    static constexpr size_t MAX_MESSAGE_SIZE = 300;            // Maximum DNS message
    static constexpr uint32_t DEFAULT_TTL_SECONDS = 60;        // DNS record TTL
    static constexpr uint8_t MAX_LABEL_LENGTH = 63;            // DNS label limit
    static constexpr size_t MAX_DOMAIN_LENGTH = 255;           // Full domain name limit
    
    // Response flags
    static constexpr uint16_t FLAG_RESPONSE = 0x8000;         // QR = 1 (response)
    static constexpr uint16_t FLAG_AUTHORITATIVE = 0x0400;    // AA = 1
    static constexpr uint16_t FLAG_RECURSION_AVAIL = 0x0080;  // RA = 1
}

// ============================================
// HTTP SERVER CONFIGURATION
// ============================================
namespace http {
    // Port configuration
    static constexpr uint16_t SERVER_PORT = 80;
    
    // Connection management
    static constexpr size_t MAX_CONNECTIONS = 3;              // Simultaneous connections
    static constexpr size_t RESPONSE_BUFFER_SIZE = 8192;        // Per-connection buffer
    
    // Content settings
    static constexpr const char* CONTENT_TYPE = "text/html; charset=utf-8";
    static constexpr uint32_t AUTO_REFRESH_MS = 1000;          // Page refresh interval
}

// ============================================
// NETWORK BUFFER CONFIGURATION
// ============================================
namespace buffers {
    // LWIP buffer settings
    //static constexpr size_t PBUF_POOL_SIZE = 16;              // Number of pbufs
    //static constexpr size_t PBUF_POOL_BUFSIZE = 512;          // Size of each pbuf
    
    // Application buffers
    static constexpr size_t UDP_BUFFER_SIZE = 1500;           // UDP packet buffer
    static constexpr size_t TCP_BUFFER_SIZE = 2048;           // TCP segment buffer
}

// ============================================
// NETWORK TIMEOUTS
// ============================================
namespace timeouts {
    // Connection timeouts (milliseconds)
    static constexpr uint32_t TCP_CONNECT_MS = 10000;         // TCP connection timeout
    static constexpr uint32_t TCP_IDLE_MS = 30000;            // TCP idle disconnect
    static constexpr uint32_t UDP_RECEIVE_MS = 5000;          // UDP receive timeout
    
    // Retry settings
    static constexpr uint8_t MAX_RETRIES = 3;                 // Max retry attempts
    static constexpr uint32_t RETRY_DELAY_MS = 1000;          // Delay between retries
}

// ============================================
// CAPTIVE PORTAL CONFIGURATION
// ============================================
namespace captive_portal {
    // Enable/disable captive portal features
    static constexpr bool ENABLE_DNS_HIJACK = true;           // Redirect all DNS to AP
    static constexpr bool ENABLE_HTTP_REDIRECT = false;       // Redirect HTTP to portal
    
    // Portal behavior
    static constexpr const char* PORTAL_DOMAIN = "portal.local";
    static constexpr bool AUTO_POPUP = true;                  // Try to auto-show portal
}

} // namespace config