#pragma once

#include "hardware/i2c.h"
#include "pico/time.h"
#include <functional>
#include <cstdint>
#include <cstdio>

class ADS1115Device {
public:
    struct Data {
        int16_t raw;
        float voltage;
        bool valid;
    };

    ADS1115Device(i2c_inst_t* i2c);
    ~ADS1115Device();

    bool init();
    void shutdown();

    bool start_conversion();
    bool stop_conversion();
    bool update();

    bool start_polling(std::function<void(const Data&)> handler);
    void stop_polling();

    const Data& data() const { return data_; }
    int16_t raw() const { return data_.raw; }
    float voltage() const { return data_.voltage; }
    bool valid() const { return data_.valid; }

    bool is_polling() const { return polling_; }
    bool is_converting() const { return converting_; }
    uint32_t errors() const { return error_count_; }

private:
    i2c_inst_t* i2c_;
    uint sda_pin_;
    uint scl_pin_;

    Data data_{};
    bool initialized_ = false;

    const uint16_t mux_;
    const uint16_t gain_;
    const uint16_t rate_;
    float voltage_per_bit_ = 0.0f;

    bool converting_ = false;

    repeating_timer_t timer_{};
    std::function<void(const Data&)> callback_;
    uint32_t poll_rate_hz_ = 0;
    uint32_t error_count_ = 0;
    bool polling_ = false;

    static bool timer_callback(repeating_timer_t* rt);

    bool write_register(uint8_t reg, uint8_t value);
    bool write_registers(uint8_t reg, const uint8_t* data, size_t len);
    bool read_registers(uint8_t reg, uint8_t* buffer, size_t len);

    uint16_t build_config(bool continuous);
    float calculate_voltage_per_bit(uint16_t gain);
};