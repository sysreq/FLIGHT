#include "include/ads1115_s.h"
#include "../misc/config.settings"
#include "../misc/utility.h"
#include <cstring>

// Constants are now centralized in config.settings

ADS1115::ADS1115(i2c_inst_t* i2c, uint sda, uint scl)
    : i2c_(i2c), sda_pin_(sda), scl_pin_(scl) {}

ADS1115::~ADS1115() {
    shutdown();
}

// Timer callback (static for C API)
bool ADS1115::timer_callback(repeating_timer_t* rt) {
    auto* self = static_cast<ADS1115*>(rt->user_data);

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

bool ADS1115::write_register(uint8_t reg, uint8_t value) {
    uint8_t buffer[2] = {reg, value};
    return i2c_write_blocking(i2c_, DEVICE_ADDR, buffer, 2, false) == 2;
}

bool ADS1115::write_registers(uint8_t reg, const uint8_t* data, size_t len) {
    uint8_t buffer[len + 1];
    buffer[0] = reg;
    memcpy(&buffer[1], data, len);
    return i2c_write_blocking(i2c_, DEVICE_ADDR, buffer, len + 1, false) == len + 1;
}

bool ADS1115::read_registers(uint8_t reg, uint8_t* buffer, size_t len) {
    if (i2c_write_blocking(i2c_, DEVICE_ADDR, &reg, 1, true) != 1) 
        return false;
    return i2c_read_blocking(i2c_, DEVICE_ADDR, buffer, len, false) == len;
}

uint16_t ADS1115::build_config(bool continuous) {
    uint16_t config = static_cast<uint16_t>(mux_) |
                     static_cast<uint16_t>(gain_) |
                     static_cast<uint16_t>(rate_) |
                     0x0003;
    
    if (!continuous) {
        config |= 0x0100; // Single-shot mode
    }
    return config;
}

float ADS1115::calculate_voltage_per_bit(uint16_t gain) {
    switch(gain) {
        case Config::ADS1115::Gain::FS_6_144V: return Config::Common::HX711_VOLTAGE_PER_BIT_6_144V;
        case Config::ADS1115::Gain::FS_4_096V: return Config::Common::HX711_VOLTAGE_PER_BIT_4_096V;
        case Config::ADS1115::Gain::FS_2_048V: return Config::Common::HX711_VOLTAGE_PER_BIT_2_048V;
        case Config::ADS1115::Gain::FS_1_024V: return Config::Common::HX711_VOLTAGE_PER_BIT_1_024V;
        case Config::ADS1115::Gain::FS_0_512V: return Config::Common::HX711_VOLTAGE_PER_BIT_0_512V;
        case Config::ADS1115::Gain::FS_0_256V: return Config::Common::HX711_VOLTAGE_PER_BIT_0_256V;
        default: return Config::Common::HX711_VOLTAGE_PER_BIT_6_144V;
    }
}

bool ADS1115::init() {
    if (initialized_) return true;

    i2c_init(i2c_, DEFAULT_BAUDRATE);
    gpio_set_function(sda_pin_, GPIO_FUNC_I2C);
    gpio_set_function(scl_pin_, GPIO_FUNC_I2C);
    gpio_pull_up(sda_pin_);
    gpio_pull_up(scl_pin_);

    log_device("I2C", "Initialized (SDA=%u, SCL=%u, %u Hz)",
           sda_pin_, scl_pin_, DEFAULT_BAUDRATE);

    uint8_t dummy;
    if (i2c_read_blocking(i2c_, DEVICE_ADDR, &dummy, 1, false) < 0) {
        log_device("ADS1115", "Not found at address 0x%02X", DEVICE_ADDR);
        return false;
    }

    voltage_per_bit_ = calculate_voltage_per_bit(gain_);

    initialized_ = true;
    log_device("ADS1115", "Initialized");
    return true;
}

void ADS1115::shutdown() {
    if (initialized_) {
        stop_polling();
        stop_conversion();
        i2c_deinit(i2c_);
        initialized_ = false;
        log_device("ADS1115", "Shutdown");
    }
}

void ADS1115::set_mux(uint16_t mux) {
    mux_ = mux;
}

void ADS1115::set_gain(uint16_t gain) {
    gain_ = gain;
    voltage_per_bit_ = calculate_voltage_per_bit(gain);
}

void ADS1115::set_rate(uint16_t rate) {
    rate_ = rate;
}

bool ADS1115::start_conversion() {
    if (converting_) return true;

    uint16_t config = build_config(true);
    uint8_t bytes[2] = {
        static_cast<uint8_t>(config >> 8),
        static_cast<uint8_t>(config & 0xFF)
    };

    if (!write_registers(REG_CONFIG, bytes, 2)) {
        return false;
    }

    converting_ = true;
    return true;
}

bool ADS1115::stop_conversion() {
    if (!converting_) return true;

    uint16_t config = build_config(false);
    uint8_t bytes[2] = {
        static_cast<uint8_t>(config >> 8),
        static_cast<uint8_t>(config & 0xFF)
    };

    if (!write_registers(REG_CONFIG, bytes, 2)) {
        return false;
    }

    converting_ = false;
    return true;
}

bool ADS1115::update() {
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
    if (!read_registers(REG_CONVERSION, buffer, 2)) {
        data_.valid = false;
        return false;
    }

    data_.raw = convert_u16_to_signed(combine_bytes(buffer[0], buffer[1]));
    data_.voltage = data_.raw * voltage_per_bit_;
    data_.valid = true;
    return true;
}

bool ADS1115::start_polling(std::function<void(const Data&)> handler, uint32_t rate_hz /* = DEFAULT_POLL_RATE */) {
    if (polling_) stop_polling();

    callback_ = handler;
    poll_rate_hz_ = rate_hz;
    error_count_ = 0;

    int64_t interval_us = -static_cast<int64_t>(1000000 / poll_rate_hz_);
    polling_ = add_repeating_timer_us(interval_us, timer_callback, this, &timer_);

    if (polling_) {
        log_device("ADS1115", "Polling at %u Hz", poll_rate_hz_);
    }
    return polling_;
}

void ADS1115::stop_polling() {
    if (polling_) {
        cancel_repeating_timer(&timer_);
        polling_ = false;
        log_device("ADS1115", "Polling stopped");
    }
}

uint16_t ADS1115::mux() const {
    return mux_;
}

uint16_t ADS1115::gain() const {
    return gain_;
}

uint16_t ADS1115::rate() const {
    return rate_;
}

const ADS1115::Data& ADS1115::data() const {
    return data_;
}

int16_t ADS1115::raw() const {
    return data_.raw;
}

float ADS1115::voltage() const {
    return data_.voltage;
}

bool ADS1115::valid() const {
    return data_.valid;
}

bool ADS1115::is_polling() const {
    return polling_;
}

bool ADS1115::is_converting() const {
    return converting_;
}

uint32_t ADS1115::errors() const {
    return error_count_;
}