#pragma once
// ============================================
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <algorithm>
#include <array>
#include <string>
#include <queue>
// ============================================
#include "pico/critical_section.h"
#include "pico/cyw43_arch.h"
#include "pico/time.h"
// ============================================
#include "lwip/ip_addr.h"
#include "lwip/udp.h"
#include "lwip/tcp.h"
#include "lwip/errno.h"
#include "lwip/pbuf.h"
// ============================================
namespace http::platform {
    // ============================================
    // TIME UTILITIES
    // ============================================
    inline uint64_t get_uptime_us() {
        return to_us_since_boot(get_absolute_time());
    }

    inline uint32_t get_uptime_ms() {
        return to_ms_since_boot(get_absolute_time());
    }

    inline uint64_t time_diff_us(absolute_time_t from, absolute_time_t to) {
        return absolute_time_diff_us(from, to);
    }

    inline uint32_t time_diff_ms(absolute_time_t from, absolute_time_t to) {
        return static_cast<uint32_t>(absolute_time_diff_us(from, to) / 1000);
    }
    // ============================================
    // NETWORK UTILITIES
    // ============================================
    inline void copy_ip_addr(ip_addr_t& dest, const ip_addr_t& src) {
        ip_addr_copy(dest, src);
    }

    inline uint32_t ip_to_u32(const ip_addr_t& addr) {
        return ip4_addr_get_u32(&addr);
    }

    inline void u32_to_ip(ip_addr_t& addr, uint32_t val) {
        IP4_ADDR(&addr, (val) & 0xFF, ((val) >> 8) & 0xFF,
                ((val) >> 16) & 0xFF, ((val) >> 24) & 0xFF);
    }
    // ============================================
    // CRITICAL SECTION WRAPPER
    // ============================================
    class CriticalSection {
    public:
        CriticalSection(critical_section_t& cs) : cs_(cs) {
            critical_section_enter_blocking(&cs_);
        }

        ~CriticalSection() {
            critical_section_exit(&cs_);
        }

        CriticalSection(const CriticalSection&) = delete;
        CriticalSection& operator=(const CriticalSection&) = delete;

    private:
        critical_section_t& cs_;
    };
// ============================================
} // namespace http::platform
