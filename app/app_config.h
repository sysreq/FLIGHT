#pragma once
// ============================================
#include <concepts>
#include <cstdint>
#include <ctime>
#include <type_traits>
// ============================================
#include <pico/stdlib.h>
#include <pico/stdio.h>
#include <pico/multicore.h>
// ============================================
#include "common/channels.h"
#include "i2c/i2c_bus.h"
// ============================================
#include "app_logging.h"
#include "app_error.h"
// ============================================
// CORE CONTRACT DEFINITION
// ============================================
namespace core0 {
    bool init();
    bool loop();
    void shutdown();
}

namespace core1 {
    bool init();
    bool loop();
    void shutdown();
}

// ============================================
// MESSAGE CHANNELS
// ============================================
struct SensorData {};
struct SystemCommands {};

using SensorChannel = MessageChannel<SensorData>;
using CommandChannel = MessageChannel<SystemCommands>;

// ============================================
// MESSAGE TYPES
// ============================================
enum SensorTypes : uint8_t {
    MSG_IMU_DATA = 1,
    MSG_BMP581_DATA = 2,
    MSG_MS4525_DATA = 3
};

enum CommandTypes : uint8_t {
    MSG_CMD_SHUTDOWN = 1,
    MSG_CMD_START_POLLING = 2,
    MSG_CMD_STOP_POLLING = 3
};

// ============================================
// TIMING CONFIGURATION
// ============================================
constexpr uint32_t LOADCELL_UPDATE_INTERVAL_US = 20'000;    // 20ms
constexpr uint32_t FILE_SYNC_INTERVAL_US = 1'000'000;       // 1 second
constexpr uint32_t MS4525_OVERSAMPLE_COUNT = 16;

// ============================================
// SYSTEM CONFIGURATION
// ============================================
constexpr int ARIZONA_OFFSET_SECONDS = -7 * 3600;  // UTC-7

// ============================================
// I2C BUS CONFIGURATION
// ============================================
using TelemetryBus = i2c::I2CBus<i2c0, 4, 5, 400000>;  // SDA=4, SCL=5, 400kHz

