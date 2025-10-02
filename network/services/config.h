#pragma once
#include <cstdint>

namespace network::services {
    // HTTP configuration
    inline constexpr uint16_t HTTP_PORT = 80;
    inline constexpr size_t MAX_REQUEST_LINE_LENGTH = 512;
    inline constexpr size_t MAX_HEADER_COUNT = 16;

    // DHCP/DNS timeouts (if needed)
    inline constexpr uint32_t DHCP_TIMEOUT_MS = 10000;
    inline constexpr uint32_t DNS_TIMEOUT_MS = 5000;
}
