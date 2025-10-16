#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include "hardware/gpio.h"
#include "pico/time.h"
#include "hardware/sync.h"

namespace adc {

struct hx711_data {
    int32_t raw_value;
    int32_t tared_value;
    float weight;
    bool valid;
};

struct calibration_point {
    int32_t raw_reading;
    float known_weight_lbs;
};

class HX711 {
private:
    static constexpr uint32_t CLOCK_DELAY_US = 1;

    static constexpr uint8_t DEFAULT_DATA_PIN = 28;
    static constexpr uint8_t DEFAULT_SCK_PIN = 29;
    static constexpr uint8_t DEFAULT_GAIN_PULSES = 1;
    static constexpr float DEFAULT_SCALE_FACTOR = 33358.00f;
    static constexpr int32_t DEFAULT_TARE_OFFSET = 98095;

    uint8_t gain_pulses;
    float scale_factor;
    int32_t tare_offset; 

    hx711_data current_data{};
    bool initialized = false;
    static constexpr uint8_t OVERSAMPLE_COUNT = 16;
    static constexpr uint8_t MAX_OVERSAMPLE_SIZE = 64;

    static constexpr uint8_t MAX_CALIBRATION_POINTS = 8;
    calibration_point calibration_points[MAX_CALIBRATION_POINTS];
    uint8_t calibration_count = 0;
    
    static int32_t convert_to_signed(uint32_t raw) {
        if (raw & 0x800000) {
            return static_cast<int32_t>(raw | 0xFF000000);
        }
        return static_cast<int32_t>(raw & 0x00FFFFFF);
    }

public:
    bool read_raw(int32_t& value) {
        uint32_t timeout = 0;
        while (gpio_get(DEFAULT_DATA_PIN)) {
            sleep_us(100);
            if (++timeout > 10000) {
                printf("HX711: Timeout waiting for data ready\n");
                return false;
            }
        }
        
        uint32_t interrupts = save_and_disable_interrupts();
        uint32_t raw_data = 0;
        
        for (int i = 0; i < 24; i++) {
            gpio_put(DEFAULT_SCK_PIN, true);
            busy_wait_us(CLOCK_DELAY_US);
            
            raw_data = (raw_data << 1);
            if (gpio_get(DEFAULT_DATA_PIN)) {
                raw_data |= 1;
            }
            
            gpio_put(DEFAULT_SCK_PIN, false);
            busy_wait_us(CLOCK_DELAY_US);
        }
        
        for (int i = 0; i < gain_pulses; i++) {
            gpio_put(DEFAULT_SCK_PIN, true);
            busy_wait_us(CLOCK_DELAY_US);
            gpio_put(DEFAULT_SCK_PIN, false);
            busy_wait_us(CLOCK_DELAY_US);
        }
        
        restore_interrupts(interrupts);
        value = convert_to_signed(raw_data);
        
        return true;
    }

    HX711() :
          gain_pulses(DEFAULT_GAIN_PULSES),
          scale_factor(DEFAULT_SCALE_FACTOR),
          tare_offset(DEFAULT_TARE_OFFSET) {}
    
    bool init() {
        if (initialized) {
            return true;
        }
        
        gpio_init(DEFAULT_DATA_PIN);
        gpio_init(DEFAULT_SCK_PIN);
        
        gpio_set_dir(DEFAULT_SCK_PIN, GPIO_IN);
        gpio_set_dir(DEFAULT_SCK_PIN, GPIO_OUT);
        
        gpio_pull_up(DEFAULT_DATA_PIN);
        
        gpio_put(DEFAULT_SCK_PIN, false);
        
        sleep_ms(400);
    
        int32_t dummy;
        read_raw(dummy);
        
        initialized = true;
        printf("HX711: Initialized (data: GPIO%d, clock: GPIO%d)\n", DEFAULT_DATA_PIN, DEFAULT_SCK_PIN);
        
        return true;
    }
    
    void update(uint16_t samples = OVERSAMPLE_COUNT) {
        if (!initialized) {
            current_data.valid = false;
            return;
        }

        if (samples > MAX_OVERSAMPLE_SIZE) {
            printf("HX711: Warning - samples (%u) exceeds MAX_OVERSAMPLE_SIZE (%u), clamping to maximum\n", samples, MAX_OVERSAMPLE_SIZE);
            samples = MAX_OVERSAMPLE_SIZE;
        }

        int64_t sum = 0;
        int32_t current_reading = 0;
        int success_count = 0;
        int attempt_count = 0;
        const int max_attempts = samples * 2; 

        while (success_count < samples) {
            if (read_raw(current_reading)) {
                sum += current_reading;
                success_count++;
            }

            attempt_count++;
            if (attempt_count >= max_attempts) {
                break;
            }
        }

        if (success_count < samples) {
            printf("HX711: Failed to get enough samples. (%d/%u collected after %d attempts)\n", 
                   success_count, samples, attempt_count);
            current_data.valid = false;
            return;
        }

        int32_t raw = sum / samples;

        current_data.raw_value = raw;
        current_data.tared_value = raw + tare_offset;
        current_data.weight = static_cast<float>(current_data.tared_value) / scale_factor;
        current_data.valid = true;
    }
    
    void zero() {
        calibration_count = 0;
        memset(calibration_points, 0, sizeof(calibration_points));

        scale_factor = DEFAULT_SCALE_FACTOR;
        tare_offset = DEFAULT_TARE_OFFSET;


        printf("HX711: Calibration reset to defaults\n");
    }

    bool get_calibration_sample(float weight_lbs, uint8_t samples = OVERSAMPLE_COUNT) {
        if (calibration_count >= MAX_CALIBRATION_POINTS) {
            printf("HX711: Calibration buffer full (%u points)\n", MAX_CALIBRATION_POINTS);
            return false;
        }

        if (!initialized) {
            printf("HX711: Cannot calibrate - not initialized\n");
            return false;
        }

        if (samples > MAX_OVERSAMPLE_SIZE) {
            samples = MAX_OVERSAMPLE_SIZE;
        }

        int32_t temp_samples[MAX_OVERSAMPLE_SIZE];
        int error_count = 0;
        int success_count = 0;

        while (success_count < samples) {
            if (!read_raw(temp_samples[success_count])) {
                if (++error_count >= samples) {
                    printf("HX711: Failed to gather calibration samples\n");
                    return false;
                }
                continue;
            }
            success_count++;
        }

        int64_t sum = 0;
        for (uint8_t i = 0; i < samples; i++) {
            sum += temp_samples[i];
            printf("%d\n", temp_samples[i]);
        }
        float mean = static_cast<float>(sum) / samples;

        float variance_sum = 0.0f;
        for (uint8_t i = 0; i < samples; i++) {
            float diff = temp_samples[i] - mean;
            variance_sum += diff * diff;
        }
        float std_dev = sqrtf(variance_sum / samples);

        int64_t filtered_sum = 0;
        uint8_t filtered_count = 0;
        for (uint8_t i = 0; i < samples; i++) {
            float diff = temp_samples[i] - mean;
            if (diff >= -std_dev && diff <= std_dev) {
                filtered_sum += temp_samples[i];
                filtered_count++;
            }
        }

        if (filtered_count == 0) {
            printf("HX711: No valid samples after filtering\n");
            return false;
        }

        int32_t averaged_raw = filtered_sum / filtered_count;
        calibration_points[calibration_count].raw_reading = averaged_raw;
        calibration_points[calibration_count].known_weight_lbs = weight_lbs;
        calibration_count++;

        printf("HX711: Calibration point %u: raw=%d, weight=%.3f lbs (filtered %u/%u samples)\n",
               calibration_count, averaged_raw, weight_lbs, filtered_count, samples);

        return true;
    }

    bool calibrate_from_samples() {
        if (calibration_count < 2) {
            printf("HX711: Need at least 2 calibration points (have %u)\n", calibration_count);
            return false;
        }

        float sum_x = 0.0f;
        float sum_y = 0.0f;
        float sum_xy = 0.0f;
        float sum_xx = 0.0f;

        for (uint8_t i = 0; i < calibration_count; i++) {
            float x = static_cast<float>(calibration_points[i].raw_reading);
            float y = calibration_points[i].known_weight_lbs;
            sum_x += x;
            sum_y += y;
            sum_xy += x * y;
            sum_xx += x * x;
        }

        float n = static_cast<float>(calibration_count);

        float denominator = n * sum_xx - sum_x * sum_x;
        if (fabs(denominator) < 1e-9) {
            printf("HX711: Calibration failed - all raw readings are identical. Cannot calculate slope.\n");
            return false;
        }

        float m = (n * sum_xy - sum_x * sum_y) / denominator;
        float b = (sum_y - m * sum_x) / n;

        if (m == 0.0f) {
            printf("HX711: Calibration failed - slope is zero\n");
            return false;
        }

        scale_factor = 1.0f / m;
        tare_offset = static_cast<int32_t>(b / m);

        printf("HX711: Calibration complete - scale_factor=%.2f, tare_offset=%d (m=%.6f, b=%.2f)\n",
               scale_factor, tare_offset, m, b);

        return true;
    }

    const hx711_data& data() const { return current_data; }
    int32_t raw() const { return current_data.raw_value; }
    int32_t tared() const { return current_data.tared_value; }
    float weight() const { return current_data.weight; }
    bool valid() const { return current_data.valid; }

    void set_scale(float scale) { scale_factor = scale; }
    void set_offset(int32_t offset) { tare_offset = offset; }
    void set_gain(uint8_t gain) { gain_pulses = gain; }

    float get_scale() { return scale_factor; }
    uint32_t get_offset() { return tare_offset; }
};

} // namespace adc