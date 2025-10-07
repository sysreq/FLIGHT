#pragma once

#include "i2c/i2c_driver.h"

namespace i2c::drivers {

class ADS1115 : public I2CDriverBase<ADS1115> {
public:
    enum class Mux : uint16_t {
        DIFF_0_1 = 0x0000, ///< Differential AIN0 - AIN1
        DIFF_0_3 = 0x1000, ///< Differential AIN0 - AIN3
        DIFF_1_3 = 0x2000, ///< Differential AIN1 - AIN3
        DIFF_2_3 = 0x3000, ///< Differential AIN2 - AIN3
        SINGLE_0 = 0x4000, ///< Single-ended AIN0
        SINGLE_1 = 0x5000, ///< Single-ended AIN1
        SINGLE_2 = 0x6000, ///< Single-ended AIN2
        SINGLE_3 = 0x7000, ///< Single-ended AIN3
    };

    enum class Gain : uint16_t {
        FS_6_144V = 0x0000, ///< +/- 6.144V
        FS_4_096V = 0x0200, ///< +/- 4.096V
        FS_2_048V = 0x0400, ///< +/- 2.048V
        FS_1_024V = 0x0600, ///< +/- 1.024V
        FS_0_512V = 0x0800, ///< +/- 0.512V
        FS_0_256V = 0x0A00, ///< +/- 0.256V
    };

    enum class Rate : uint16_t {
        SPS_8   = 0x0000,
        SPS_16  = 0x0020,
        SPS_32  = 0x0040,
        SPS_64  = 0x0060,
        SPS_128 = 0x0080,
        SPS_250 = 0x00A0,
        SPS_475 = 0x00C0,
        SPS_860 = 0x00E0,
    };

private:
    static constexpr uint8_t REG_CONVERSION = 0x00;
    static constexpr uint8_t REG_CONFIG     = 0x01;

    ads1115_data data_{};
    Mux mux_config_{};
    Gain gain_config_{};
    Rate rate_config_{};
    float voltage_per_bit_{};
    bool is_converting_ = false;

    uint16_t build_config(bool continuous) {
        uint16_t config = static_cast<uint16_t>(mux_config_) |
                            static_cast<uint16_t>(gain_config_) |
                            static_cast<uint16_t>(rate_config_) |
                            0x0003; // Disable comparator

        if (continuous) {
            config |= 0x0000; // Continuous conversion mode
        } else {
            config |= 0x0100; // Single-shot mode (for stopping)
        }
        return config;
    }

    float get_voltage_per_bit(Gain gain) {
        float range_v;
        switch(gain) {
            case Gain::FS_6_144V: range_v = 6.144f; break;
            case Gain::FS_4_096V: range_v = 4.096f; break;
            case Gain::FS_2_048V: range_v = 2.048f; break;
            case Gain::FS_1_024V: range_v = 1.024f; break;
            case Gain::FS_0_512V: range_v = 0.512f; break;
            case Gain::FS_0_256V: range_v = 0.256f; break;
            default:              range_v = 6.144f;
        }
        return range_v / 32768.0f;
    }

public:
    ADS1115() : I2CDriverBase() {
        data_.valid = false;
    }

    bool init(i2c_inst_t* instance) {
        i2c_instance = instance;

        if (!device_present()) {
            printf("%s: Device not found at address 0x%02X\n", Traits::name, Traits::address);
            return false;
        }
        
        configure(Mux::DIFF_0_1, Gain::FS_2_048V, Rate::SPS_128);

        initialized = true;
        log_init_success();
        return true;
    }

    void configure(Mux mux, Gain gain, Rate rate) {
        bool was_converting = is_converting_;
        if (was_converting) stop();

        mux_config_ = mux;
        gain_config_ = gain;
        rate_config_ = rate;
        voltage_per_bit_ = get_voltage_per_bit(gain);

        if (was_converting) start();
    }

    bool start() {
        if (is_converting_) return true;

        uint16_t config = build_config(true); // true for continuous
        uint8_t config_bytes[2] = { static_cast<uint8_t>(config >> 8), static_cast<uint8_t>(config & 0xFF) };

        if (!write_registers(REG_CONFIG, config_bytes, 2)) {
            return false;
        }
        is_converting_ = true;
        return true;
    }

    bool stop() {
        uint16_t config = build_config(false); // false for single-shot/power-down
        uint8_t config_bytes[2] = { static_cast<uint8_t>(config >> 8), static_cast<uint8_t>(config & 0xFF) };

        if (!write_registers(REG_CONFIG, config_bytes, 2)) {
            return false;
        }
        is_converting_ = false;
        return true;
    }

    bool update() {
        if (!initialized) {
            data_.valid = false;
            return false;
        }

        if (!is_converting_) {
            start();
            data_.valid = false; // Data is not ready on the first poll after starting
            return true; // Keep polling timer alive
        }

        uint8_t read_buf[2];
        if (!read_registers(REG_CONVERSION, read_buf, 2)) {
            data_.valid = false;
            return false; // I2C error, stop polling after max retries
        }

        data_.raw = utils::merge_bytes<int16_t>(read_buf[0], read_buf[1]);
        data_.voltage = data_.raw * voltage_per_bit_;
        data_.valid = true;
        return true;
    }

    const ads1115_data& get_data() const {
        return data_;
    }
};

} // namespace i2c::drivers
