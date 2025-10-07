#pragma once

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cmath>
#include <cstring>

#include <functional>
#include <optional>
#include <variant>
#include <array>
#include <concepts>
#include <type_traits>

#include <hardware\i2c.h>

namespace i2c {

// ============================================================================
// SYSTEM CONFIGURATION
// ============================================================================
static constexpr size_t MAX_DEVICES = 8;
static constexpr uint32_t DEFAULT_BUS_SPEED = 400'000;
static constexpr uint32_t MAX_ERRORS = 10;

} // namespace i2c

// ============================================================================
// DRIVER FORWARD DECLARATIONS
// ============================================================================
namespace i2c::drivers {
    class ICM20948;
    struct icm20948_data {
        float accel_x;
        float accel_y;
        float accel_z;
        float gyro_x;
        float gyro_y;
        float gyro_z;
        bool valid;
    };
    
    class BMP581;
    struct bmp581_data {
        float temperature;
        float pressure;
        float altitude;
        bool valid;
    };
    
    class MS4525D0;
    struct ms4525d0_data {
        float pressure_pa;
        float temperature_c;
        bool valid;
    };

    class ADS1115;
    struct ads1115_data {
        int16_t raw;
        float voltage;
        bool valid;
    };
}

// ============================================================================
// DEVICE TRAITS CONFIGURATION
// ============================================================================
namespace i2c {

template<typename DeviceType>
struct DeviceTraits;

template<>
struct DeviceTraits<i2c::drivers::ICM20948> {
    static constexpr uint8_t address = 0x69;
    static constexpr const char* name = "ICM20948";
    static constexpr uint32_t default_poll_rate = 50;
    using data_type = drivers::icm20948_data;
};

template<>
struct DeviceTraits<i2c::drivers::BMP581> {
    static constexpr uint8_t address = 0x47;
    static constexpr const char* name = "BMP581";
    static constexpr uint32_t default_poll_rate = 20;
    using data_type = drivers::bmp581_data;
};

template<>
struct DeviceTraits<i2c::drivers::MS4525D0> {
    static constexpr uint8_t address = 0x58;
    static constexpr const char* name = "MS4525D0";
    static constexpr uint32_t default_poll_rate = 500;  // 50Hz for airspeed
    using data_type = drivers::ms4525d0_data;
};

template<>
struct DeviceTraits<i2c::drivers::ADS1115> {
    static constexpr uint8_t address = 0x48; // Default for ADDR to GND
    static constexpr const char* name = "ADS1115";
    static constexpr uint32_t default_poll_rate = 10;
    using data_type = drivers::ads1115_data;
};

} // namespace i2c