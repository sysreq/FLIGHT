#include "pico/stdlib.h"
#include "app_config.h"
#include "i2c/drivers/icm20948.h"
#include "i2c/drivers/bmp581.h"
#include "i2c/drivers/ms4525d0.h"
#include <array>

namespace core1 {

static bool polling_enabled = false;
static std::array<i2c::drivers::ms4525d0_data, MS4525_OVERSAMPLE_COUNT> ms4525_samples;
static size_t ms4525_sample_count = 0;

// Sensor data handlers
static void handle_imu_data(const i2c::drivers::icm20948_data& data) {
    if (data.valid && polling_enabled) {
        Message* msg = SensorChannel::acquire();
        if (msg) {
            msg->type = MSG_IMU_DATA;
            msg->Put(data);
            SensorChannel::commit(msg);
        }
    }
}

static void handle_bmp581_data(const i2c::drivers::bmp581_data& data) {
    if (data.valid && polling_enabled) {
        Message* msg = SensorChannel::acquire();
        if (msg) {
            msg->type = MSG_BMP581_DATA;
            msg->Put(data);
            SensorChannel::commit(msg);
        }
    }
}

static void handle_ms4525d0_data(const i2c::drivers::ms4525d0_data data) {
    if (!polling_enabled) return;

    // Oversample for better accuracy
    ms4525_samples[ms4525_sample_count++] = data;

    if (ms4525_sample_count >= MS4525_OVERSAMPLE_COUNT) {
        float avg_pressure = 0.0f;
        float avg_temperature = 0.0f;
        int valid_count = 0;

        for (const auto& sample : ms4525_samples) {
            if (sample.valid) {
                valid_count++;
                avg_pressure += sample.pressure_pa;
                avg_temperature += sample.temperature_c;
            }
        }

        if (valid_count > 0) {
            Message* msg = SensorChannel::acquire();
            if (msg) {
                i2c::drivers::ms4525d0_data avg_data;
                avg_data.pressure_pa = avg_pressure / valid_count;
                avg_data.temperature_c = avg_temperature / valid_count;
                avg_data.valid = true;

                msg->type = MSG_MS4525_DATA;
                msg->Put(avg_data);
                SensorChannel::commit(msg);
            }
        }

        ms4525_sample_count = 0;
    }
}

// Initialize Core 1 hardware
bool init() {
    using namespace i2c::drivers;
    printf("---------- CORE 0 INITIALIZATION ----------\n");
    printf("\t------ STARTING I2C BUS ------\n");

    REQUIRE(TelemetryBus::start(), "\tERROR: Failed to initialize I2C bus\n");
    printf("\t------ STARTING I2C BUS ------\n");

    if (!TelemetryBus::add_device<MS4525D0>(handle_ms4525d0_data)) {
        printf("\tWARNING: MS4525D0 not detected\n");
    }

    if (!TelemetryBus::add_device<BMP581>(handle_bmp581_data)) {
        printf("\tWARNING: BMP581 not detected\n");
    }

    printf("\t------ I2C BUS ONLINE ------\n");
    return true;
}

bool loop() {
    if (!CommandChannel::empty()) {
        Message* msg = CommandChannel::pop();
        if (msg) {
            switch(msg->type) {
                case MSG_CMD_SHUTDOWN:
                    CommandChannel::release(msg);
                    return false;

                case MSG_CMD_START_POLLING:
                    if (!polling_enabled) {
                        printf("Core 1: Starting sensor polling\n");
                        TelemetryBus::enable();
                        polling_enabled = true;
                    }
                    break;

                case MSG_CMD_STOP_POLLING:
                    if (polling_enabled) {
                        printf("Core 1: Stopping sensor polling\n");
                        TelemetryBus::disable();
                        polling_enabled = false;
                    }
                    break;
            }
            CommandChannel::release(msg);
        }
    }

    sleep_ms(10);
    return true;
}

void shutdown() {
    printf("---------- CORE 1 SHUTDOWN ----------\n");
    if (polling_enabled) {
        TelemetryBus::disable();
    }
    TelemetryBus::shutdown();
    printf("\t------ I2C BUS OFFLINE ------\n");
    printf("----------  CORE 1 ENDED  ----------\n");
}

} // namespace core1