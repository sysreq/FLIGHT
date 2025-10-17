#pragma once

#include <cstdint>
#include <functional>

// Forward declarations for pico types
struct repeating_timer;
struct i2c_inst;

class ADS1115Device {
public:
    struct Data {
        int16_t raw;
        float voltage;
        bool valid;
    };

    ADS1115Device() = default;
    ~ADS1115Device();

    bool init();
    void shutdown();

    bool start_conversion();
    bool stop_conversion();
    bool update();

    bool start_polling(std::function<void(const Data&)> handler);
    void stop_polling();

    inline const Data& data() const { return data_; }
    inline int16_t raw() const { return data_.raw; }
    inline float voltage() const { return data_.voltage; }
    inline bool valid() const { return data_.valid; }

    inline bool is_polling() const { return polling_; }
    inline bool is_converting() const { return converting_; }
    inline uint32_t errors() const { return error_count_; }

private:
    repeating_timer* timer_ = nullptr;

    Data data_{};
    bool initialized_ = false;

    float voltage_per_bit_ = 0.0f;

    bool converting_ = false;
    std::function<void(const Data&)> callback_;
    uint32_t poll_rate_hz_ = 0;
    uint32_t error_count_ = 0;
    bool polling_ = false;

    static bool timer_callback(repeating_timer* rt);
};