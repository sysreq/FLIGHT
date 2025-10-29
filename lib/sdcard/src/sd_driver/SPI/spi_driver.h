#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>

#include "hardware/spi.h"

namespace SPI {

namespace Settings {
    enum BusIdentifier : uint8_t {
        SPI0 = 0,
        SPI1 = 1
    };

    namespace Pins {
        struct PinRange {
            uint8_t miso;
            uint8_t mosi;
            uint8_t sck;
            uint8_t cs;
        };
        
        constexpr PinRange Bus0_Default   = {.miso = 16, .mosi = 19, .sck = 18, .cs = 17};
        constexpr PinRange Bus1_Default   = {.miso = 12, .mosi = 15, .sck = 14, .cs = 13};
        constexpr PinRange Bus0_Alternate = {.miso =  4, .mosi =  7, .sck =  6, .cs =  5};
    }

    enum OpMode : uint8_t {
        MODE0 = 0,
        MODE1 = 1,
        MODE2 = 2,
        MODE3 = 3
    };

    enum class BaudRate : uint32_t {
        Min       =   400'000,    // 400 KHz
        Slow      = 1'000'000,    // 1 MHz
        Standard  = 10'000'000,   // 10 MHz
        Fast      = 20'000'000,   // 20 MHz
        VeryFast  = 37'000'000,   // 37 MHz
        Max       = 50'000'000    // 50 MHz
    };
} // namespace Settings

namespace Internal {
    struct Details {
        spi_inst_t* hw_inst;
        std::atomic<bool> locked;
    };
        
    template<Settings::BusIdentifier bus>
    Details* GetDetails();
    
    bool _init(Details* bus, Settings::Pins::PinRange pins, 
               Settings::BaudRate rate, Settings::OpMode mode);
    
    uint32_t _set_baud_rate(Details* bus, Settings::BaudRate rate);
    
    bool _burst_write_blocking(Details* bus, std::span<const uint8_t> data, 
                               uint32_t timeout_ms);
    
    bool _burst_read_blocking(Details* bus, std::span<uint8_t> data, 
                             uint32_t timeout_ms);
    
    bool _burst_transfer_blocking(Details* bus, std::span<const uint8_t> tx, 
                                 std::span<uint8_t> rx, uint32_t timeout_ms);

    void _signal_select(Details* bus_base, bool on);
    
}  // namespace Internal

template<Settings::BusIdentifier bus, Settings::Pins::PinRange pins>
struct Bus {
    class ScopedLock {
    private:
        Internal::Details* bus_details;
        bool owns_lock;
        
    public:
        explicit ScopedLock(Internal::Details* details, uint32_t timeout_ms);
        ~ScopedLock();
        void release();
        
        ScopedLock(const ScopedLock&) = delete;
        ScopedLock& operator=(const ScopedLock&) = delete;
        
        ScopedLock(ScopedLock&& other) noexcept 
            : bus_details(other.bus_details), owns_lock(other.owns_lock) {
            other.owns_lock = false;
        }
        
        ScopedLock& operator=(ScopedLock&& other) noexcept {
            if (this != &other) {
                release();
                bus_details = other.bus_details;
                owns_lock = other.owns_lock;
                other.owns_lock = false;
            }
            return *this;
        }
    };

    static bool init(Settings::BaudRate rate = Settings::BaudRate::Min,
                     Settings::OpMode mode = Settings::OpMode::MODE0) {
        return Internal::_init(Internal::GetDetails<bus>(), pins, rate, mode);
    }

    static uint32_t set_baud_rate(Settings::BaudRate rate) {
        return Internal::_set_baud_rate(Internal::GetDetails<bus>(), rate);
    }

    static inline uint8_t read_single_byte() {
        auto* details = Internal::GetDetails<bus>();
        uint8_t rx;
        spi_read_blocking(details->hw_inst, 0xFF, &rx, 1);
        return rx;
    }

    static inline bool write_single_byte(const uint8_t value) {
        auto* details = Internal::GetDetails<bus>();
        return spi_write_blocking(details->hw_inst, &value, 1) == 1;
    }

    static inline uint8_t write_read_byte(const uint8_t value) {
        auto* details = Internal::GetDetails<bus>();
        uint8_t rx;
        spi_write_read_blocking(details->hw_inst, &value, &rx, 1);
        return rx;
    }

    static bool burst_write_blocking(std::span<const uint8_t> data, 
                                     uint32_t timeout_ms = 1000) {
        return Internal::_burst_write_blocking(Internal::GetDetails<bus>(), data, timeout_ms);
    }

    static bool burst_read_blocking(std::span<uint8_t> data, 
                                    uint32_t timeout_ms = 1000) {
        return Internal::_burst_read_blocking(Internal::GetDetails<bus>(), data, timeout_ms);
    }

    static bool burst_transfer_blocking(std::span<const uint8_t> tx, 
                                       std::span<uint8_t> rx,
                                       uint32_t timeout_ms = 1000) {
        return Internal::_burst_transfer_blocking(Internal::GetDetails<bus>(), tx, rx, timeout_ms);
    }

    static inline void cs_select() { Internal::_signal_select(Internal::GetDetails<bus>(), false); }
    static inline void cs_deselect() { Internal::_signal_select(Internal::GetDetails<bus>(), true); }

    [[nodiscard]] static ScopedLock lock(uint32_t timeout_ms = UINT32_MAX) {
        return ScopedLock(Internal::GetDetails<bus>(), timeout_ms);
    }
};

} // namespace SPI