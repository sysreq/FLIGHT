#pragma once

#include <cstdint>
#include <cstring>
#include "../misc/config.settings"

class HX711 {
public:
    struct Data {
        int32_t raw;
        int32_t tared;
        float weight;
        bool valid;
    };

    struct CalibrationPoint {
        int32_t raw_reading;
        float known_weight_lbs;
    };

    HX711();

    bool init();

    bool read_raw(int32_t& value);
    void update(uint16_t samples = OVERSAMPLE_COUNT);
    void zero();
    bool get_calibration_sample(float weight_lbs, uint8_t samples = OVERSAMPLE_COUNT);
    bool calibrate_from_samples();

    const Data& data() const { return current_data_; }
    int32_t raw() const { return current_data_.raw; }
    int32_t tared() const { return current_data_.tared; }
    float weight() const { return current_data_.weight; }
    bool valid() const { return current_data_.valid; }

private:
    // Use centralized configuration constants
    static constexpr uint32_t CLOCK_DELAY_US = Config::HX711::CLOCK_DELAY_US;
    static constexpr uint8_t DEFAULT_DATA_PIN = Config::HX711::DATA_PIN;
    static constexpr uint8_t DEFAULT_SCK_PIN = Config::HX711::CLOCK_PIN;
    static constexpr uint8_t DEFAULT_GAIN_PULSES = Config::HX711::DEFAULT_GAIN_PULSES;
    static constexpr float DEFAULT_SCALE_FACTOR = Config::HX711::DEFAULT_SCALE_FACTOR;
    static constexpr int32_t DEFAULT_TARE_OFFSET = Config::HX711::DEFAULT_TARE_OFFSET;

    // Compile-time immutable settings
    const uint8_t gain_pulses_ = DEFAULT_GAIN_PULSES;
    const float scale_factor_ = DEFAULT_SCALE_FACTOR;

    // Tare offset can be modified through calibration interface only
    int32_t tare_offset_ = DEFAULT_TARE_OFFSET;

    Data current_data_{};
    bool initialized_ = false;

    static constexpr uint8_t OVERSAMPLE_COUNT = Config::HX711::OVERSAMPLE_COUNT;
    static constexpr uint8_t MAX_OVERSAMPLE_SIZE = Config::HX711::MAX_OVERSAMPLE_SIZE;
    static constexpr uint8_t MAX_CALIBRATION_POINTS = Config::HX711::MAX_CALIBRATION_POINTS;
    CalibrationPoint calibration_points_[MAX_CALIBRATION_POINTS];
    uint8_t calibration_count_ = 0;
};