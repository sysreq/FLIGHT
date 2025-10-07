#pragma once

#include <cstdint>
#include <cstdio>
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

    static constexpr float _scale_factor = 34718.4f;
    static constexpr int32_t _tare_offset = -100000; 

    hx711_data current_data{};
    bool initialized = false;
    static constexpr uint8_t OVERSAMPLE_COUNT = 16;
    int32_t sample_buffer[OVERSAMPLE_COUNT];
    
    static int32_t convert_to_signed(uint32_t raw) {
        if (raw & 0x800000) {
            return static_cast<int32_t>(raw | 0xFF000000);
        }
        return static_cast<int32_t>(raw & 0x00FFFFFF);
    }

public:
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

    HX711() = default;
    
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
    
    void update(uint16_t samples = OVERSAMPLE_COUNT) {
        if (!initialized) {
            current_data.valid = false;
            return;
        }

        // Collect samples
        int error_count = 0;
        int success_count = 0;
        while (success_count < OVERSAMPLE_COUNT) {
            if (!read_raw(sample_buffer[success_count])) {
                if(++error_count < OVERSAMPLE_COUNT)
                {
                        printf("HX711 failed to gather data.\n");
                        current_data.valid = false;
                        return;  
                }
                continue;
            }
            success_count++;
        }

        // Average the samples
        int64_t sum = 0;
        for (uint8_t i = 0; i < samples; i++) {
            sum += sample_buffer[i];
        }
        int32_t raw = sum / samples;

        current_data.raw_value = raw;
        current_data.tared_value = raw + _tare_offset;
        current_data.weight = static_cast<float>(raw) / _scale_factor;
        current_data.valid = true;
    }
    
    const hx711_data& data() const { return current_data; }
    int32_t raw() const { return current_data.raw_value; }
    int32_t tared() const { return current_data.tared_value; }
    float weight() const { return current_data.weight; }
    bool valid() const { return current_data.valid; }
};

} // namespace drivers