#include "spi_driver.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

#include "pico/stdlib.h"
#include "pico/types.h"
#include "pico/multicore.h"

#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"

namespace {
    constexpr auto DEFAULT_PIN_DRIVE_STRENGTH = GPIO_DRIVE_STRENGTH_8MA;
    constexpr uint8_t RP2350_DREQ_START = DREQ_SPI0_TX;
    constexpr uint8_t SPI_FILL_CHAR = 0xFF;

    constexpr spi_inst_t* to_spi_inst(const SPI::Settings::BusIdentifier bus) {
        switch (bus) {
            case SPI::Settings::BusIdentifier::SPI0: return spi0;
            case SPI::Settings::BusIdentifier::SPI1: return spi1;
        }
        panic("SPI: Invalid Bus Identifier\n");
        return nullptr;
    }

    constexpr std::pair<spi_cpol_t, spi_cpha_t> to_cpol_cpha(const SPI::Settings::OpMode mode) {
        switch (mode) {
            case 0: return {SPI_CPOL_0, SPI_CPHA_0};
            case 1: return {SPI_CPOL_0, SPI_CPHA_1};
            case 2: return {SPI_CPOL_1, SPI_CPHA_0};
            case 3: return {SPI_CPOL_1, SPI_CPHA_1};
        }
        return {SPI_CPOL_0, SPI_CPHA_0};
    }

    bool check_spi_status(spi_inst_t* spi) {
        uint32_t sr = spi_get_const_hw(spi)->sr;
        const uint32_t bad_bits = SPI_SSPSR_BSY_BITS | SPI_SSPSR_RFF_BITS | SPI_SSPSR_RNE_BITS;
        const uint32_t required_bits = SPI_SSPSR_TNF_BITS | SPI_SSPSR_TFE_BITS;
        return (sr & bad_bits) == 0 && (sr & required_bits) == required_bits;
    }

    bool check_dma_channel(uint channel) {
        return !(dma_hw->ch[channel].ctrl_trig & (
            DMA_CH0_CTRL_TRIG_AHB_ERROR_BITS |
            DMA_CH0_CTRL_TRIG_READ_ERROR_BITS |
            DMA_CH0_CTRL_TRIG_WRITE_ERROR_BITS |
            DMA_CH0_CTRL_TRIG_BUSY_BITS
        ));
    }
}

namespace SPI::Internal {
enum Direction : uint8_t {
    RX = 0,
    TX = 1
};

template<Direction dir>
struct DMA_Channel {
    int8_t channel;
    uint8_t dreq;
    dma_channel_config config;

    explicit DMA_Channel(Settings::BusIdentifier bus) :
        channel(dma_claim_unused_channel(true)),
        dreq(RP2350_DREQ_START + 
             ((bus == Settings::BusIdentifier::SPI0) ? 0 : 2) + 
             ((dir == Direction::RX) ? 1 : 0)),
        config(dma_channel_get_default_config(channel)) 
    {
        channel_config_set_transfer_data_size(&config, DMA_SIZE_8);
        channel_config_set_dreq(&config, dreq);
        
        channel_config_set_read_increment(&config, (dir == Direction::TX));
        channel_config_set_write_increment(&config, (dir == Direction::RX));
    }

    ~DMA_Channel() { dma_channel_unclaim(channel); }
    DMA_Channel(const DMA_Channel&) = delete;
    DMA_Channel& operator=(const DMA_Channel&) = delete;
};

struct DetailsExtended : Details {
    const Settings::BusIdentifier bus;
    Settings::Pins::PinRange pins;
    Settings::BaudRate desired_rate;
    uint32_t actual_rate;
    Settings::OpMode mode;
    
    DMA_Channel<RX> rx_channel;
    DMA_Channel<TX> tx_channel;
    
    bool initialized;

    explicit DetailsExtended(Settings::BusIdentifier bus_id) :
        Details{to_spi_inst(bus_id), std::atomic<bool>(false)},
        bus(bus_id),
        pins{},
        desired_rate(Settings::BaudRate::Min),
        actual_rate(0),
        mode(Settings::OpMode::MODE0),
        rx_channel(bus_id),
        tx_channel(bus_id),
        initialized(false)
    {}
};

template<>
Details* GetDetails<Settings::BusIdentifier::SPI0>() {
    static DetailsExtended impl(Settings::BusIdentifier::SPI0);
    return &impl;
}

template<>
Details* GetDetails<Settings::BusIdentifier::SPI1>() {
    static DetailsExtended impl(Settings::BusIdentifier::SPI1);
    return &impl;
}

bool _init(Details* bus_base, Settings::Pins::PinRange pins, 
           Settings::BaudRate rate, Settings::OpMode mode) {
    auto* bus = static_cast<DetailsExtended*>(bus_base);
    const auto [cpol, cpha] = to_cpol_cpha(mode);

    if (bus->initialized) return true;

    bus->desired_rate = rate;
    bus->actual_rate = spi_init(bus->hw_inst, static_cast<uint32_t>(rate));
    spi_set_format(bus->hw_inst, 8, cpol, cpha, SPI_MSB_FIRST);
    bus->mode = mode;

    gpio_set_function(pins.miso, GPIO_FUNC_SPI);
    gpio_pull_up(pins.miso);

    gpio_set_function(pins.mosi, GPIO_FUNC_SPI);
    gpio_set_drive_strength(pins.mosi, DEFAULT_PIN_DRIVE_STRENGTH);

    gpio_set_function(pins.sck, GPIO_FUNC_SPI);
    gpio_set_drive_strength(pins.sck, DEFAULT_PIN_DRIVE_STRENGTH);
    gpio_set_slew_rate(pins.sck, GPIO_SLEW_RATE_FAST);

    gpio_init(pins.cs);
    gpio_set_dir(pins.cs, GPIO_OUT);
    gpio_put(pins.cs, 1);

    bus->pins = pins;
    bus->initialized = true;

    return true;
}

uint32_t _set_baud_rate(Details* bus_base, Settings::BaudRate rate) {
    auto* bus = static_cast<DetailsExtended*>(bus_base);
    bus->desired_rate = rate;
    bus->actual_rate = spi_set_baudrate(bus->hw_inst, static_cast<uint32_t>(rate));
    return bus->actual_rate;
}

static bool dma_transfer_blocking(Details* bus_base, const uint8_t* tx, uint8_t* rx, 
                                  size_t length, uint32_t timeout_ms = 0xFFFFFFFF) {
    auto* bus = static_cast<DetailsExtended*>(bus_base);
    
    static const uint8_t tx_dummy = SPI_FILL_CHAR;
    static uint8_t rx_dummy = 0xA5;

    const uint8_t* tx_ptr = tx ? tx : &tx_dummy;
    uint8_t* rx_ptr = rx ? rx : &rx_dummy;

    dma_channel_config tx_config = bus->tx_channel.config;
    channel_config_set_read_increment(&tx_config, tx != nullptr);
    
    dma_channel_configure(
        bus->tx_channel.channel,
        &tx_config,
        &spi_get_hw(bus->hw_inst)->dr,
        tx_ptr,
        length,
        false
    );

    dma_channel_config rx_config = bus->rx_channel.config;
    channel_config_set_write_increment(&rx_config, rx != nullptr);
    
    dma_channel_configure(
        bus->rx_channel.channel,
        &rx_config,
        rx_ptr,
        &spi_get_hw(bus->hw_inst)->dr,
        length,
        false
    );

    dma_start_channel_mask(
        (1u << bus->tx_channel.channel) | (1u << bus->rx_channel.channel)
    );

    absolute_time_t timeout_time = make_timeout_time_ms(timeout_ms);
    
    while (dma_channel_is_busy(bus->rx_channel.channel) || 
           dma_channel_is_busy(bus->tx_channel.channel)) {
        if (time_reached(timeout_time)) {
            dma_channel_abort(bus->tx_channel.channel);
            dma_channel_abort(bus->rx_channel.channel);
            return false;
        }
        tight_loop_contents();
    }

    timeout_time = make_timeout_time_ms(timeout_ms);
    while (spi_is_busy(bus->hw_inst)) {
        if (time_reached(timeout_time)) {
            return false;
        }
        tight_loop_contents();
    }

    bool spi_ok = check_spi_status(bus->hw_inst);
    bool tx_ok = check_dma_channel(bus->tx_channel.channel);
    bool rx_ok = check_dma_channel(bus->rx_channel.channel);

    return spi_ok && tx_ok && rx_ok;
}

bool _burst_write_blocking(Details* bus_base, std::span<const uint8_t> data, 
                           uint32_t timeout_ms = 0xFFFFFFFF) {
    if (data.empty()) {
        return true;
    }
    
    auto* bus = static_cast<DetailsExtended*>(bus_base);   
    return dma_transfer_blocking(bus_base, data.data(), nullptr, data.size(), timeout_ms);
}

bool _burst_read_blocking(Details* bus_base, std::span<uint8_t> data, 
                          uint32_t timeout_ms = 0xFFFFFF) {
    auto* bus = static_cast<DetailsExtended*>(bus_base);
    if (data.empty()) return true;
    return dma_transfer_blocking(bus_base, nullptr, data.data(), data.size(), timeout_ms);
}

bool _burst_transfer_blocking(Details* bus_base, std::span<const uint8_t> tx, 
                              std::span<uint8_t> rx, uint32_t timeout_ms = 0xFFFFFF) {
    auto* bus = static_cast<DetailsExtended*>(bus_base);  

    if (tx.size() != rx.size()) return false;
    if (tx.empty()) return true;
    
    return dma_transfer_blocking(bus_base, tx.data(), rx.data(), tx.size(), timeout_ms);
}

void _signal_select(Details* bus_base, bool on) {
    auto* bus = static_cast<DetailsExtended*>(bus_base);
    gpio_put(bus->pins.cs, on);
}
} // namespace SPI::Internal

template<SPI::Settings::BusIdentifier bus, SPI::Settings::Pins::PinRange pins>
SPI::Bus<bus, pins>::ScopedLock::ScopedLock(SPI::Internal::Details* details, uint32_t timeout_ms) 
    : bus_details(details), owns_lock(false) {
    
    if (timeout_ms == UINT32_MAX) {
        bool expected = false;
        while (!bus_details->locked.compare_exchange_weak(expected, true, 
                                                          std::memory_order_acquire,
                                                          std::memory_order_relaxed)) {
            expected = false;
            __wfe();
        }
        owns_lock = true;
    } else {
        absolute_time_t timeout_time = make_timeout_time_ms(timeout_ms);
        bool expected = false;
        
        while (!bus_details->locked.compare_exchange_weak(expected, true,
                                                          std::memory_order_acquire,
                                                          std::memory_order_relaxed)) {
            if (time_reached(timeout_time)) {
                owns_lock = false;
                return;  // Failed to acquire lock
            }
            expected = false;
            __wfe();
        }
        owns_lock = true;
    }
}

template<SPI::Settings::BusIdentifier bus, SPI::Settings::Pins::PinRange pins>
SPI::Bus<bus, pins>::ScopedLock::~ScopedLock() {
    release();
}

template<SPI::Settings::BusIdentifier bus, SPI::Settings::Pins::PinRange pins>
void SPI::Bus<bus, pins>::ScopedLock::release() {
    if (owns_lock) {
        bus_details->locked.store(false, std::memory_order_release);
        __sev();
        owns_lock = false;
    }
}