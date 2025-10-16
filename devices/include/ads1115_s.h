#pragma once

#include "hardware/i2c.h"
#include "hardware/timer.h"
#include "hardware/gpio.h"
#include "pico/time.h"
#include <functional>
#include <cstdint>
#include <cstdio>

class ADS1115 {
public:
    static constexpr uint16_t MUX_DIFF_0_1 = 0x0000;
    static constexpr uint16_t MUX_DIFF_0_3 = 0x1000;
    static constexpr uint16_t MUX_DIFF_1_3 = 0x2000;
    static constexpr uint16_t MUX_DIFF_2_3 = 0x3000;
    static constexpr uint16_t MUX_SINGLE_0 = 0x4000;
    static constexpr uint16_t MUX_SINGLE_1 = 0x5000;
    static constexpr uint16_t MUX_SINGLE_2 = 0x6000;
    static constexpr uint16_t MUX_SINGLE_3 = 0x7000;

    static constexpr uint16_t GAIN_6_144V = 0x0000;
    static constexpr uint16_t GAIN_4_096V = 0x0200;
    static constexpr uint16_t GAIN_2_048V = 0x0400;
    static constexpr uint16_t GAIN_1_024V = 0x0600;
    static constexpr uint16_t GAIN_0_512V = 0x0800;
    static constexpr uint16_t GAIN_0_256V = 0x0A00;

    static constexpr uint16_t RATE_8   = 0x0000;
    static constexpr uint16_t RATE_16  = 0x0020;
    static constexpr uint16_t RATE_32  = 0x0040;
    static constexpr uint16_t RATE_64  = 0x0060;
    static constexpr uint16_t RATE_128 = 0x0080;
    static constexpr uint16_t RATE_250 = 0x00A0;
    static constexpr uint16_t RATE_475 = 0x00C0;
    static constexpr uint16_t RATE_860 = 0x00E0;

    struct Data {
        int16_t raw;
        float voltage;
        bool valid;
    };

    ADS1115(i2c_inst_t* i2c, uint sda, uint scl);
    ~ADS1115();

    bool init();
    void shutdown();

    void set_mux(uint16_t mux);
    void set_gain(uint16_t gain);
    void set_rate(uint16_t rate);

    uint16_t mux() const { return mux_; }
    uint16_t gain() const { return gain_; }
    uint16_t rate() const { return rate_; }

    bool start_conversion();
    bool stop_conversion();
    bool update();

    bool start_polling(std::function<void(const Data&)> handler, uint32_t rate_hz = DEFAULT_POLL_RATE);
    void stop_polling();

    const Data& data() const { return data_; }
    int16_t raw() const { return data_.raw; }
    float voltage() const { return data_.voltage; }
    bool valid() const { return data_.valid; }

    bool is_polling() const { return polling_; }
    bool is_converting() const { return converting_; }
    uint32_t errors() const { return error_count_; }

private:
    static constexpr uint8_t DEVICE_ADDR = 0x48;
    static constexpr uint8_t REG_CONVERSION = 0x00;
    static constexpr uint8_t REG_CONFIG = 0x01;
    static constexpr uint32_t DEFAULT_BAUDRATE = 400'000;
    static constexpr uint32_t DEFAULT_POLL_RATE = 10; // Hz

    i2c_inst_t* i2c_;
    uint sda_pin_;
    uint scl_pin_;

    Data data_{};
    bool initialized_ = false;

    uint16_t mux_ = MUX_DIFF_0_1;
    uint16_t gain_ = GAIN_2_048V;
    uint16_t rate_ = RATE_128;
    float voltage_per_bit_ = 0.0f;

    bool converting_ = false;

    repeating_timer_t timer_{};
    std::function<void(const Data&)> callback_;
    uint32_t poll_rate_hz_ = DEFAULT_POLL_RATE;
    uint32_t error_count_ = 0;
    bool polling_ = false;

    static bool timer_callback(repeating_timer_t* rt);

    bool write_register(uint8_t reg, uint8_t value);
    bool write_registers(uint8_t reg, const uint8_t* data, size_t len);
    bool read_registers(uint8_t reg, uint8_t* buffer, size_t len);

    uint16_t build_config(bool continuous);
    float calculate_voltage_per_bit(uint16_t gain);
};