#include <cstring>
#include <cmath>

#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "hardware/timer.h"
#include "pico/time.h"

#include "include/hx711_s.h"
#include "misc/config.settings"
#include "misc/utility.h"

using namespace Config;

HX711Device::HX711Device() :
    tare_offset_(HX711::DEFAULT_TARE_OFFSET),
    scale_factor_(HX711::DEFAULT_SCALE_FACTOR)
    {}

HX711Device::~HX711Device() {
    shutdown();
}

// Timer callback (static for C API)
bool HX711Device::timer_callback(repeating_timer* rt) {
    auto* self = static_cast<HX711Device*>(rt->user_data);

    self->update();

    if (self->callback_) {
        self->callback_(self->current_data_);
    }

    return true;
}

bool HX711Device::init() {
    if (initialized_) {
        return true;
    }

    gpio_init(HX711::DATA_PIN);
    gpio_init(HX711::CLOCK_PIN);

    gpio_set_dir(HX711::CLOCK_PIN, GPIO_OUT);
    gpio_pull_up(HX711::DATA_PIN);

    gpio_put(HX711::CLOCK_PIN, false);

    sleep_ms(400);

    int32_t dummy;
    read_raw(dummy);

    initialized_ = true;
    log_device("HX711", "Initialized (data: GPIO%d, clock: GPIO%d)", HX711::DATA_PIN, HX711::CLOCK_PIN);

    return true;
}

void HX711Device::shutdown() {
    if (initialized_) {
        stop_polling();
        initialized_ = false;
        log_device("HX711", "Shutdown");
    }
}

bool HX711Device::read_raw(int32_t& value) {
    auto wait_for_data_ready = [&]() {
        return !gpio_get(HX711::DATA_PIN);
    };

    if (!retry_with_timeout(wait_for_data_ready, Common::TIMEOUT_US, Common::RETRY_DELAY_US)) {
        log_device("HX711", "Timeout waiting for data ready");
        return false;
    }

    uint32_t interrupts = save_and_disable_interrupts();
    uint32_t raw_data = 0;

    for (int i = 0; i < 24; i++) {
        gpio_put(HX711::CLOCK_PIN, true);
        busy_wait_us(HX711::CLOCK_DELAY_US);

        raw_data = (raw_data << 1);
        if (gpio_get(HX711::DATA_PIN)) {
            raw_data |= 1;
        }

        gpio_put(HX711::CLOCK_PIN, false);
        busy_wait_us(HX711::CLOCK_DELAY_US);
    }

    // Apply gain pulses (default 1 pulse for 128 gain)
    for (int i = 0; i < HX711::DEFAULT_GAIN_PULSES; i++) {
        gpio_put(HX711::CLOCK_PIN, true);
        busy_wait_us(HX711::CLOCK_DELAY_US);
        gpio_put(HX711::CLOCK_PIN, false);
        busy_wait_us(HX711::CLOCK_DELAY_US);
    }

    restore_interrupts(interrupts);
    value = convert_to_signed(raw_data);

    return true;
}

void HX711Device::update(uint16_t samples /* = OVERSAMPLE_COUNT */) {
    if (!initialized_) {
        current_data_.valid = false;
        return;
    }

    if (samples > MAX_OVERSAMPLE_SIZE) samples = MAX_OVERSAMPLE_SIZE;

    int64_t sum = 0;
    int32_t current_reading = 0;
    int success_count = 0;
    int attempt_count = 0;
    const int max_attempts = samples * 2;

    while (success_count < samples && attempt_count < max_attempts) {
        if (read_raw(current_reading)) {
            sum += current_reading;
            success_count++;
        }
        attempt_count++;
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
    current_data_.weight = static_cast<float>(current_data_.tared) / HX711::DEFAULT_SCALE_FACTOR;
    current_data_.valid = true;
}

void HX711Device::zero() {
    calibration_count_ = 0;
    memset(calibration_points_, 0, sizeof(calibration_points_));

    tare_offset_ = HX711::DEFAULT_TARE_OFFSET;

    log_device("HX711", "Calibration reset to defaults");
}

bool HX711Device::get_calibration_sample(float weight_lbs, uint8_t samples /* = OVERSAMPLE_COUNT */) {
    if (calibration_count_ >= MAX_CALIBRATION_POINTS) {
        log_device("HX711", "Calibration buffer full (%u points)", MAX_CALIBRATION_POINTS);
        return false;
    }

    if (!initialized_) {
        log_device("HX711", "Cannot calibrate - not initialized");
        return false;
    }

    if (samples > MAX_OVERSAMPLE_SIZE) samples = MAX_OVERSAMPLE_SIZE;

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

bool HX711Device::calibrate_from_samples() {
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

    tare_offset_ = static_cast<int32_t>(b / m);

    log_device("HX711", "Calibration complete - tare_offset=%d (m=%.6f, b=%.2f)",
           tare_offset_, m, b);

    return true;
}

bool HX711Device::start_polling(std::function<void(const Data&)> handler) {
    if (polling_) stop_polling();

    callback_ = handler;
    error_count_ = 0;

    int64_t interval_us = 100000; // 100ms = 10 Hz
    polling_ = add_repeating_timer_us(interval_us, timer_callback, this, nullptr);

    if (polling_) {
        log_device("HX711", "Polling started");
    }
    return polling_;
}

void HX711Device::stop_polling() {
    if (polling_) {
        polling_ = false;
        log_device("HX711", "Polling stopped");
    }
}

