#pragma once

#include <cstdint>
#include <cstdio>
#include <functional>
#include "hardware/gpio.h"
#include "pico/time.h"
#include "hardware/sync.h"
#include <cstring>

namespace adc {

struct hx711_data {
    int32_t raw_value;
    int32_t tared_value;
    float weight;
    bool valid;
};

class HX711 {
private:
    static constexpr uint32_t CLOCK_DELAY_US = 1;
    static constexpr uint8_t GAIN_PULSES = 1;
    
    static constexpr uint8_t DATA = 6; 
    static constexpr uint8_t SCK = 7;

    static constexpr float _scale_factor = 29007.5f;
    static constexpr int32_t _tare_offset = -26200; 

    hx711_data current_data{};
    repeating_timer_t timer{};
    std::function<void(const hx711_data&)> callback;
    uint32_t poll_rate_hz = 10;
    uint32_t error_count = 0;
    bool initialized = false;
    bool polling_active = false;
    
    static int32_t convert_to_signed(uint32_t raw) {
        if (raw & 0x800000) {
            return static_cast<int32_t>(raw | 0xFF000000);
        }
        return static_cast<int32_t>(raw & 0x00FFFFFF);
    }

public:
    static bool timer_callback(repeating_timer_t* rt) {
        auto* self = static_cast<HX711*>(rt->user_data);
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
                printf("HX711: Too many errors (%u), stopping timer\n", error_count);
                polling_active = false;
                return false;
            }
        }
        return true;
    }
    
    bool read_raw(int32_t& value) {
        uint32_t timeout = 0;
        while (gpio_get(DATA)) {
            sleep_us(100);
            if (++timeout > 10000) {
                printf("HX711: Timeout waiting for data ready\n");
                return false;
            }
        }
        
        uint32_t interrupts = save_and_disable_interrupts();
        uint32_t raw_data = 0;
        
        for (int i = 0; i < 24; i++) {
            gpio_put(SCK, true);
            busy_wait_us(CLOCK_DELAY_US);
            
            raw_data = (raw_data << 1);
            if (gpio_get(DATA)) {
                raw_data |= 1;
            }
            
            gpio_put(SCK, false);
            busy_wait_us(CLOCK_DELAY_US);
        }
        
        for (int i = 0; i < GAIN_PULSES; i++) {
            gpio_put(SCK, true);
            busy_wait_us(CLOCK_DELAY_US);
            gpio_put(SCK, false);
            busy_wait_us(CLOCK_DELAY_US);
        }
        
        restore_interrupts(interrupts);
        value = convert_to_signed(raw_data);
        
        return true;
    }

    HX711() {
        memset(&timer, 0, sizeof(repeating_timer_t));
    }
    
    ~HX711() {
        stop();
    }
    
    bool init() {
        if (initialized) {
            return true;
        }
        
        gpio_init(DATA);
        gpio_init(SCK);
        
        gpio_set_dir(SCK, GPIO_IN);
        gpio_set_dir(SCK, GPIO_OUT);
        
        gpio_pull_up(DATA);
        
        gpio_put(SCK, false);
        
        sleep_ms(400);
    
        int32_t dummy;
        read_raw(dummy);
        
        initialized = true;
        printf("HX711: Initialized (data: GPIO%d, clock: GPIO%d)\n", DATA, SCK);
        
        return true;
    }
    
    bool update() {
        if (!initialized) return false;
        
        int32_t raw;
        if (!read_raw(raw)) {
            current_data.valid = false;
            return false;
        }
        
        current_data.raw_value = raw;
        current_data.tared_value = raw + _tare_offset;
        current_data.weight = static_cast<float>(raw) / _scale_factor;
        current_data.valid = true;
        
        return true;
    }
    
    bool start(std::function<void(const hx711_data&)> handler, uint32_t rate_hz = 10) {
        if (!initialized) {
            if (!init()) {
                return false;
            }
        }
        
        if (polling_active) {
            return true;
        }
        
        callback = handler;
        poll_rate_hz = rate_hz;
        error_count = 0;
        
        int64_t interval_us = -static_cast<int64_t>(1000000 / poll_rate_hz);
        polling_active = add_repeating_timer_us(interval_us, timer_callback, this, &timer);
        
        if (polling_active) {
            printf("HX711: Started polling at %u Hz\n", poll_rate_hz);
        }
        
        return polling_active;
    }
    
    void stop() {
        if (polling_active) {
            cancel_repeating_timer(&timer);
            polling_active = false;
            
            if (initialized) {
                gpio_put(SCK, true);
                sleep_us(70);
            }
            
            printf("HX711: Stopped polling\n");
        }
    }
    
    hx711_data get_data() const { return current_data; }
    bool is_active() const { return polling_active; }
};

} // namespace drivers