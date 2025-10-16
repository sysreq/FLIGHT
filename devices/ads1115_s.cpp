#include <functional>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "include/ads1115_s.h"
#include "../misc/config.settings"
#include "../misc/utility.h"

#include "pico/time.h"

#include "hardware/i2c.h"
#include "hardware/timer.h"
#include "hardware/gpio.h"

using namespace Config;

static i2c_inst_t* i2c_;

inline bool write_register(i2c_inst_t* i2c, uint8_t reg, uint8_t value) {
    uint8_t buffer[2] = {reg, value};
    return i2c_write_blocking(i2c, ADS1115::DEVICE_ADDRESS, buffer, 2, false) == 2;
}

inline bool write_registers(i2c_inst_t* i2c, uint8_t reg, const uint8_t* data, size_t len) {
    uint8_t buffer[len + 1];
    buffer[0] = reg;
    memcpy(&buffer[1], data, len);
    return i2c_write_blocking(i2c, ADS1115::DEVICE_ADDRESS, buffer, len + 1, false) == len + 1;
}

inline bool read_registers(i2c_inst_t* i2c, uint8_t reg, uint8_t* buffer, size_t len) {
    if (i2c_write_blocking(i2c, ADS1115::DEVICE_ADDRESS, &reg, 1, true) != 1)
        return false;
    return i2c_read_blocking(i2c, ADS1115::DEVICE_ADDRESS, buffer, len, false) == len;
}

uint16_t build_config(bool continuous) {
    uint16_t config = static_cast<uint16_t>(ADS1115::DEFAULT_MUX) |
                     static_cast<uint16_t>(ADS1115::DEFAULT_GAIN) |
                     static_cast<uint16_t>(ADS1115::DEFAULT_RATE) |
                     0x0003;
    
    if (!continuous) {
        config |= 0x0100; // Single-shot mode
    }
    return config;
}

constexpr float calculate_voltage_per_bit(uint16_t gain) {
    using namespace Config::ADS1115::Gain;

    switch(gain) {
        case FS_6_144V: return Config::Common::VOLTAGE_PER_BIT_6_144V;
        case FS_4_096V: return Config::Common::VOLTAGE_PER_BIT_4_096V;
        case FS_2_048V: return Config::Common::VOLTAGE_PER_BIT_2_048V;
        case FS_1_024V: return Config::Common::VOLTAGE_PER_BIT_1_024V;
        case FS_0_512V: return Config::Common::VOLTAGE_PER_BIT_0_512V;
        case FS_0_256V: return Config::Common::VOLTAGE_PER_BIT_0_256V;
        default: return Config::Common::VOLTAGE_PER_BIT_6_144V;
    }
}

ADS1115Device::ADS1115Device(i2c_inst_t* i2c) : i2c_(i2c) {}

ADS1115Device::~ADS1115Device() {
    shutdown();
}

// Timer callback (static for C API)
bool ADS1115Device::timer_callback(repeating_timer_t* rt) {
    auto* self = static_cast<ADS1115Device*>(rt->user_data);

    auto update_operation = [&]() { return self->update(); };
    if (retry_with_error_limit(update_operation, 10, self->error_count_)) {
        if (self->callback_) {
            self->callback_(self->data_);
        }
        return true;
    } else {
        log_device("ADS1115", "Too many errors, stopping");
        return false; // Stop timer
    }
}

bool ADS1115Device::init() {
    if (initialized_) return true;

    i2c_init(i2c_, ADS1115::BAUDRATE);
    gpio_set_function(ADS1115::DATA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(ADS1115::CLOCK_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(ADS1115::DATA_PIN);
    gpio_pull_up(ADS1115::CLOCK_PIN);

    log_device("I2C", "Initialized (SDA=%u, SCL=%u, %u Hz)",
            ADS1115::DATA_PIN, ADS1115::CLOCK_PIN, ADS1115::BAUDRATE);

    uint8_t dummy;
    if (i2c_read_blocking(i2c_, ADS1115::DEVICE_ADDRESS, &dummy, 1, false) < 0) {
        log_device("ADS1115", "Not found at address 0x%02X", ADS1115::DEVICE_ADDRESS);
        return false;
    }

    voltage_per_bit_ = calculate_voltage_per_bit(ADS1115::DEFAULT_GAIN);

    initialized_ = true;
    log_device("ADS1115", "Initialized");
    return true;
}

void ADS1115Device::shutdown() {
    if (initialized_) {
        stop_polling();
        stop_conversion();
        i2c_deinit(i2c_);
        initialized_ = false;
        log_device("ADS1115", "Shutdown");
    }
}


bool ADS1115Device::start_conversion() {
    if (converting_) return true;

    uint16_t config = build_config(true);
    uint8_t bytes[2] = {
        static_cast<uint8_t>(config >> 8),
        static_cast<uint8_t>(config & 0xFF)
    };

    if (!write_registers(i2c_, ADS1115::CONFIG_REGISTER, bytes, 2)) {
        return false;
    }

    converting_ = true;
    return true;
}

bool ADS1115Device::stop_conversion() {
    if (!converting_) return true;

    uint16_t config = build_config(false);
    uint8_t bytes[2] = {
        static_cast<uint8_t>(config >> 8),
        static_cast<uint8_t>(config & 0xFF)
    };

    if (!write_registers(i2c_, ADS1115::CONFIG_REGISTER, bytes, 2)) {
        return false;
    }

    converting_ = false;
    return true;
}

bool ADS1115Device::update() {
    if (!initialized_) {
        data_.valid = false;
        return false;
    }

    if (!converting_) {
        start_conversion();
        data_.valid = false;
        return true;
    }

    uint8_t buffer[2];
    if (!read_registers(i2c_, ADS1115::CONVERSION_REGISTER, buffer, 2)) {
        data_.valid = false;
        return false;
    }

    data_.raw = convert_u16_to_signed(combine_bytes(buffer[0], buffer[1]));
    data_.voltage = data_.raw * voltage_per_bit_;
    data_.valid = true;
    return true;
}

bool ADS1115Device::start_polling(std::function<void(const Data&)> handler) {
    if (polling_) stop_polling();

    callback_ = handler;
    error_count_ = 0;

    int64_t interval_us = -static_cast<int64_t>(1000000 / poll_rate_hz_);
    polling_ = add_repeating_timer_us(interval_us, timer_callback, this, &timer_);

    if (polling_) {
        log_device("ADS1115", "Polling at %u Hz", poll_rate_hz_);
    }
    return polling_;
}

void ADS1115Device::stop_polling() {
    if (polling_) {
        cancel_repeating_timer(&timer_);
        polling_ = false;
        log_device("ADS1115", "Polling stopped");
    }
}

const ADS1115Device::Data& ADS1115Device::data() const {
    return data_;
}

int16_t ADS1115Device::raw() const {
    return data_.raw;
}

float ADS1115Device::voltage() const {
    return data_.voltage;
}

bool ADS1115Device::valid() const {
    return data_.valid;
}

bool ADS1115Device::is_polling() const {
    return polling_;
}

bool ADS1115Device::is_converting() const {
    return converting_;
}

uint32_t ADS1115Device::errors() const {
    return error_count_;
}