#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include "hardware/adc.h"
#include "pico/time.h"

namespace adc {

struct acs770_data {
    float current_a;
    float voltage_v;
};

class ACS770 {
private:
    static constexpr float SENSITIVITY_MV_PER_A = 7.830445f;
    static constexpr float ZERO_CURRENT_V = 0.330f;
    static constexpr float VCC = 3.3f;
    static constexpr float VOLTAGE_DIVIDER_RATIO = 18.94141f;
    
    static constexpr uint8_t CURRENT_CHANNEL = 0;
    static constexpr uint8_t VOLTAGE_CHANNEL = 1;
    
    acs770_data current_data{};
    repeating_timer_t timer{};
    std::function<void(const acs770_data&)> callback;
    float zero_offset = ZERO_CURRENT_V * 1000.0f;
    uint32_t poll_rate_hz = 50;
    uint32_t error_count = 0;
    bool initialized = false;
    bool polling_active = false;
    
    static bool timer_callback(repeating_timer_t* rt) {
        auto* self = static_cast<ACS770*>(rt->user_data);
        return self->handle_timer();
    }
    
    bool handle_timer() {
        if (update()) {
            error_count = 0;
            if (callback) {
                callback(current_data);
            }
        } else {
            error_count++;
            if (error_count > 10) {
                printf("ACS770: Too many errors (%u), stopping timer\n", error_count);
                polling_active = false;
                return false;
            }
        }
        return true;
    }
    
    uint32_t read_oversampled(uint8_t channel, uint8_t samples = 32) {
        adc_select_input(channel);
        uint32_t sum = 0;
        for (uint8_t i = 0; i < samples; i++) {
            sum += adc_read();
        }
        return static_cast<uint32_t>(sum / samples);
    }

public:
    ACS770() {
        memset(&timer, 0, sizeof(repeating_timer_t));
    }
    
    ~ACS770() {
        stop_polling();
    }
    
    bool init() {
        // Initialize ADC hardware
        static bool adc_initialized = false;
        if (!adc_initialized) {
            adc_init();
            adc_gpio_init(26);
            adc_gpio_init(27);
            adc_initialized = true;
        }
        
        initialized = true;
        printf("ACS770: Initialized (sensitivity: %.3f mV/A)\n", SENSITIVITY_MV_PER_A);
        printf("ACS770: Voltage monitoring enabled (divider: %.2f)\n", VOLTAGE_DIVIDER_RATIO);
        return true;
    }
        
    bool calibrate_zero() {
        if (!initialized) return false;
        
        uint16_t avg = read_oversampled(CURRENT_CHANNEL, 64);
        zero_offset = (avg * VCC / 4095.0f) * 1000.0f;
        
        printf("ACS770: Zero calibrated to %.1f mV\n", zero_offset);
        return true;
    }
    
    bool update() {
        if (!initialized) return false;
        
        uint32_t current_adc = read_oversampled(CURRENT_CHANNEL);
        float voltage_mv = (current_adc * VCC / 4095.0f) * 1000.0f;
        
        // Calculate current
        float current = (voltage_mv - zero_offset) / SENSITIVITY_MV_PER_A;
        if (current < 0.0f) current = 0.0f;
        
        current_data.current_a = current;
        
        uint32_t voltage_adc = read_oversampled(VOLTAGE_CHANNEL);
        current_data.voltage_v = (voltage_adc * VCC / 4095.0f) * VOLTAGE_DIVIDER_RATIO;
        
        return true;
    }
    
    bool start_polling(std::function<void(const acs770_data&)> handler, uint32_t rate_hz = 50) {
        if (!initialized || polling_active) {
            return polling_active;
        }
        
        callback = handler;
        poll_rate_hz = rate_hz;
        
        int64_t interval_us = -static_cast<int64_t>(1000000 / poll_rate_hz);
        polling_active = add_repeating_timer_us(interval_us, timer_callback, this, &timer);
        
        if (polling_active) {
            printf("ACS770: Started polling at %u Hz\n", poll_rate_hz);
        }
        
        return polling_active;
    }
    
    void stop_polling() {
        if (polling_active) {
            cancel_repeating_timer(&timer);
            polling_active = false;
            printf("ACS770: Stopped polling\n");
        }
    }
    
    acs770_data get_data() const { return current_data; }
    bool is_active() const { return polling_active; }
};

} // adc namespace