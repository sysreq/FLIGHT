#pragma once

#include "hardware/i2c.h"
#include "hardware/timer.h"
#include "hardware/gpio.h"
#include "pico/time.h"
#include <functional>
#include <cstdint>
#include <cstdio>
#include "../misc/config.settings"

class ADS1115 {
public:
    // MUX settings - use centralized constants
    static constexpr uint16_t MUX_DIFF_0_1 = Config::ADS1115::Mux::DIFF_0_1;
    static constexpr uint16_t MUX_DIFF_0_3 = Config::ADS1115::Mux::DIFF_0_3;
    static constexpr uint16_t MUX_DIFF_1_3 = Config::ADS1115::Mux::DIFF_1_3;
    static constexpr uint16_t MUX_DIFF_2_3 = Config::ADS1115::Mux::DIFF_2_3;
    static constexpr uint16_t MUX_SINGLE_0 = Config::ADS1115::Mux::SINGLE_0;
    static constexpr uint16_t MUX_SINGLE_1 = Config::ADS1115::Mux::SINGLE_1;
    static constexpr uint16_t MUX_SINGLE_2 = Config::ADS1115::Mux::SINGLE_2;
    static constexpr uint16_t MUX_SINGLE_3 = Config::ADS1115::Mux::SINGLE_3;

    // Gain settings - use centralized constants
    static constexpr uint16_t GAIN_6_144V = Config::ADS1115::Gain::FS_6_144V;
    static constexpr uint16_t GAIN_4_096V = Config::ADS1115::Gain::FS_4_096V;
    static constexpr uint16_t GAIN_2_048V = Config::ADS1115::Gain::FS_2_048V;
    static constexpr uint16_t GAIN_1_024V = Config::ADS1115::Gain::FS_1_024V;
    static constexpr uint16_t GAIN_0_512V = Config::ADS1115::Gain::FS_0_512V;
    static constexpr uint16_t GAIN_0_256V = Config::ADS1115::Gain::FS_0_256V;

    // Rate settings - use centralized constants
    static constexpr uint16_t RATE_8   = Config::ADS1115::Rate::SPS_8;
    static constexpr uint16_t RATE_16  = Config::ADS1115::Rate::SPS_16;
    static constexpr uint16_t RATE_32  = Config::ADS1115::Rate::SPS_32;
    static constexpr uint16_t RATE_64  = Config::ADS1115::Rate::SPS_64;
    static constexpr uint16_t RATE_128 = Config::ADS1115::Rate::SPS_128;
    static constexpr uint16_t RATE_250 = Config::ADS1115::Rate::SPS_250;
    static constexpr uint16_t RATE_475 = Config::ADS1115::Rate::SPS_475;
    static constexpr uint16_t RATE_860 = Config::ADS1115::Rate::SPS_860;

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
    // Use centralized configuration constants
    static constexpr uint8_t DEVICE_ADDR = Config::ADS1115::DEVICE_ADDRESS;
    static constexpr uint8_t REG_CONVERSION = Config::ADS1115::CONVERSION_REGISTER;
    static constexpr uint8_t REG_CONFIG = Config::ADS1115::CONFIG_REGISTER;
    static constexpr uint32_t DEFAULT_BAUDRATE = Config::ADS1115::DEFAULT_BAUDRATE;
    static constexpr uint32_t DEFAULT_POLL_RATE = Config::ADS1115::DEFAULT_POLL_RATE;

    i2c_inst_t* i2c_;
    uint sda_pin_;
    uint scl_pin_;

    Data data_{};
    bool initialized_ = false;

    uint16_t mux_ = Config::ADS1115::DEFAULT_MUX;
    uint16_t gain_ = Config::ADS1115::DEFAULT_GAIN;
    uint16_t rate_ = Config::ADS1115::DEFAULT_RATE;
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