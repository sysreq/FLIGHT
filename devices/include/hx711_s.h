#pragma once

#include <cstdint>
#include <functional>

// Forward declaration for pico time types
struct repeating_timer;

class HX711Device {
public:
    static constexpr uint8_t OVERSAMPLE_COUNT = 8;
    static constexpr uint8_t MAX_OVERSAMPLE_SIZE = 32;
    static constexpr uint8_t MAX_CALIBRATION_POINTS = 8;

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

    HX711Device();
    ~HX711Device();

    bool init();
    void shutdown();

    bool read_raw(int32_t& value);
    void update(uint16_t samples = 1);
    void zero();
    bool get_calibration_sample(float weight_lbs, uint8_t samples = OVERSAMPLE_COUNT);
    bool calibrate_from_samples();

    bool start_polling(std::function<void(const Data&)> handler);
    void stop_polling();

    // Persistent calibration management
    bool load_calibration_settings();
    bool save_calibration_settings();
    bool purge_calibration_settings();

    // Getters for current runtime calibration values
    inline const Data& data() const { return current_data_; }
    inline int32_t raw() const { return current_data_.raw; }
    inline int32_t tared() const { return current_data_.tared; }
    inline float weight() const { return current_data_.weight; }
    inline bool valid() const { return current_data_.valid; }
    inline int32_t get_tare_offset() const { return tare_offset_; }
    inline float get_scale_factor() const { return scale_factor_; }

    // Getters for saved calibration values
    inline int32_t get_saved_tare_offset() const { return saved_tare_offset_; }
    inline float get_saved_scale_factor() const { return saved_scale_factor_; }

    inline bool is_polling() const { return polling_; }
    inline uint32_t errors() const { return error_count_; }

private:
    // Runtime calibration values (can be modified during operation)
    int32_t tare_offset_;
    float scale_factor_;

    // Saved calibration values (loaded from/saved to flash)
    int32_t saved_tare_offset_;
    float saved_scale_factor_;

    Data current_data_{};
    bool initialized_ = false;

    bool polling_ = false;
    std::function<void(const Data&)> callback_;
    uint32_t error_count_ = 0;

    CalibrationPoint calibration_points_[MAX_CALIBRATION_POINTS];
    uint8_t calibration_count_ = 0;

    static bool timer_callback(repeating_timer* rt);
};