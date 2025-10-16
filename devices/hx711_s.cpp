#include "include/hx711_s.h"
#include "../misc/config.settings"
#include "../misc/utility.h"
#include <cstdio>
#include <cmath>
#include "hardware/gpio.h"
#include "pico/time.h"
#include "hardware/sync.h"

HX711::HX711() :
    gain_pulses_(DEFAULT_GAIN_PULSES),
    scale_factor_(DEFAULT_SCALE_FACTOR),
    tare_offset_(DEFAULT_TARE_OFFSET) {}

bool HX711::init() {
    if (initialized_) {
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
    
    initialized_ = true;
    log_device("HX711", "Initialized (data: GPIO%d, clock: GPIO%d)", DEFAULT_DATA_PIN, DEFAULT_SCK_PIN);

    return true;
}

bool HX711::read_raw(int32_t& value) {
    auto wait_for_data_ready = [&]() {
        return !gpio_get(DEFAULT_DATA_PIN);
    };

    if (!retry_with_timeout(wait_for_data_ready, Config::Common::HX711_TIMEOUT_US, Config::Common::HX711_RETRY_DELAY_US)) {
        log_device("HX711", "Timeout waiting for data ready");
        return false;
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
    
    for (int i = 0; i < gain_pulses_; i++) {
        gpio_put(DEFAULT_SCK_PIN, true);
        busy_wait_us(CLOCK_DELAY_US);
        gpio_put(DEFAULT_SCK_PIN, false);
        busy_wait_us(CLOCK_DELAY_US);
    }
    
    restore_interrupts(interrupts);
    value = convert_to_signed(raw_data);

    return true;
}

void HX711::set_scale(float scale) {
    scale_factor_ = scale;
}

void HX711::set_offset(int32_t offset) {
    tare_offset_ = offset;
}

void HX711::set_gain(uint8_t gain) {
    gain_pulses_ = gain;
}

const HX711::Data& HX711::data() const {
    return current_data_;
}

int32_t HX711::raw() const {
    return current_data_.raw;
}

int32_t HX711::tared() const {
    return current_data_.tared;
}

float HX711::weight() const {
    return current_data_.weight;
}

bool HX711::valid() const {
    return current_data_.valid;
}

float HX711::scale() const {
    return scale_factor_;
}

int32_t HX711::offset() const {
    return tare_offset_;
}

uint8_t HX711::gain() const {
    return gain_pulses_;
}

void HX711::update(uint16_t samples /* = OVERSAMPLE_COUNT */) {
    if (!initialized_) {
        current_data_.valid = false;
        return;
    }

    samples = clamp(samples, static_cast<uint16_t>(0), static_cast<uint16_t>(MAX_OVERSAMPLE_SIZE));

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
        log_device("HX711", "Failed to get enough samples. (%d/%u collected after %d attempts)",
               success_count, samples, attempt_count);
        current_data_.valid = false;
        return;
    }

    int32_t raw = sum / samples;

    current_data_.raw = raw;
    current_data_.tared = raw + tare_offset_;
    current_data_.weight = static_cast<float>(current_data_.tared) / scale_factor_;
    current_data_.valid = true;
}

void HX711::zero() {
    calibration_count_ = 0;
    memset(calibration_points_, 0, sizeof(calibration_points_));

    scale_factor_ = DEFAULT_SCALE_FACTOR;
    tare_offset_ = DEFAULT_TARE_OFFSET;

    log_device("HX711", "Calibration reset to defaults");
}

bool HX711::get_calibration_sample(float weight_lbs, uint8_t samples /* = OVERSAMPLE_COUNT */) {
    if (calibration_count_ >= MAX_CALIBRATION_POINTS) {
        log_device("HX711", "Calibration buffer full (%u points)", MAX_CALIBRATION_POINTS);
        return false;
    }

    if (!initialized_) {
        log_device("HX711", "Cannot calibrate - not initialized");
        return false;
    }

    samples = clamp(samples, static_cast<uint8_t>(0), static_cast<uint8_t>(MAX_OVERSAMPLE_SIZE));

    int32_t temp_samples[MAX_OVERSAMPLE_SIZE];
    int error_count = 0;
    int success_count = 0;

    while (success_count < samples) {
        if (!read_raw(temp_samples[success_count])) {
            if (++error_count >= samples) {
                log_device("HX711", "Failed to gather calibration samples");
                return false;
            }
            continue;
        }
        success_count++;
    }

    int64_t sum = 0;
    for (uint8_t i = 0; i < samples; i++) {
        sum += temp_samples[i];
        // Debug output removed for cleaner logging
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
        log_device("HX711", "No valid samples after filtering");
        return false;
    }

    int32_t averaged_raw = filtered_sum / filtered_count;
    calibration_points_[calibration_count_].raw_reading = averaged_raw;
    calibration_points_[calibration_count_].known_weight_lbs = weight_lbs;
    calibration_count_++;

    log_device("HX711", "Calibration point %u: raw=%d, weight=%.3f lbs (filtered %u/%u samples)",
           calibration_count_, averaged_raw, weight_lbs, filtered_count, samples);

    return true;
}

bool HX711::calibrate_from_samples() {
    if (calibration_count_ < 2) {
        log_device("HX711", "Need at least 2 calibration points (have %u)", calibration_count_);
        return false;
    }

    float sum_x = 0.0f;
    float sum_y = 0.0f;
    float sum_xy = 0.0f;
    float sum_xx = 0.0f;

    for (uint8_t i = 0; i < calibration_count_; i++) {
        float x = static_cast<float>(calibration_points_[i].raw_reading);
        float y = calibration_points_[i].known_weight_lbs;
        sum_x += x;
        sum_y += y;
        sum_xy += x * y;
        sum_xx += x * x;
    }

    float n = static_cast<float>(calibration_count_);

    float denominator = n * sum_xx - sum_x * sum_x;
    if (fabs(denominator) < 1e-9) {
        log_device("HX711", "Calibration failed - all raw readings are identical. Cannot calculate slope.");
        return false;
    }

    float m = (n * sum_xy - sum_x * sum_y) / denominator;
    float b = (sum_y - m * sum_x) / n;

    if (m == 0.0f) {
        log_device("HX711", "Calibration failed - slope is zero");
        return false;
    }

    scale_factor_ = 1.0f / m;
    tare_offset_ = static_cast<int32_t>(b / m);

    log_device("HX711", "Calibration complete - scale_factor=%.2f, tare_offset=%d (m=%.6f, b=%.2f)",
           scale_factor_, tare_offset_, m, b);

    return true;
}