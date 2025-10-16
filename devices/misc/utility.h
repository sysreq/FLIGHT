#pragma once

#include <cstdint>
#include <cstdio>
#include <functional>

template<typename Func>
bool retry_with_error_limit(Func operation, uint32_t max_errors, uint32_t& error_count) {
    if (operation()) {
        error_count = 0;
        return true;
    } else {
        error_count++;
        return error_count <= max_errors;
    }
}

template<typename Func>
bool retry_with_timeout(Func operation, uint32_t timeout_us, uint32_t delay_us = 100) {
    uint32_t elapsed = 0;
    while (elapsed < timeout_us) {
        if (operation()) {
            return true;
        }
        sleep_us(delay_us);
        elapsed += delay_us;
    }
    return false;
}

inline int32_t convert_to_signed(uint32_t raw) {
    if (raw & 0x800000) {
        return static_cast<int32_t>(raw | 0xFF000000);
    }
    return static_cast<int32_t>(raw & 0x00FFFFFF);
}

inline int16_t convert_u16_to_signed(uint16_t raw) {
    if (raw & 0x8000) {
        return static_cast<int16_t>(raw | 0xFFFF0000);
    }
    return static_cast<int16_t>(raw & 0x0000FFFF);
}

inline uint16_t combine_bytes(uint8_t high, uint8_t low) {
    return (static_cast<uint16_t>(high) << 8) | low;
}

inline void split_bytes(uint16_t value, uint8_t& high, uint8_t& low) {
    high = static_cast<uint8_t>(value >> 8);
    low = static_cast<uint8_t>(value & 0xFF);
}

template<typename... Args>
void log_device(const char* device_name, const char* message, Args... args) {
    printf("%s: ", device_name);
    printf(message, args...);
    printf("\n");
}

template<typename T>
T clamp(T value, T min, T max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

template<typename T>
T lerp(T a, T b, float t) {
    return a + (b - a) * clamp(t, 0.0f, 1.0f);
}