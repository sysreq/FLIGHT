#include <ctime>
// ============================================
#include "pico/stdlib.h"
// ============================================
#include "http/access_point.h"
#include "http/ui/http_generator.h"
#include "adc/hx711.h"
// ============================================
#include "app_config.h"

namespace core0 {

static http::AccessPoint* access_point = nullptr;
static adc::HX711* loadcell = nullptr;

// State tracking
static uint32_t last_loadcell_update = 0;
static uint32_t last_file_sync = 0;
static uint32_t save_sequence = 0;
static bool polling_active = false;

bool init() {
    using namespace sdcard;
    PRINT_BANNER(0, "INITIALIZING CORE 0");
    sleep_ms(10);
    PRINT_BANNER(1, "STARTING FILESYSTEM");
    PRINT_BANNER(1, "FILESYSTEM ONLINE");
    PRINT_BANNER(1, "STARTING LOAD CELL");
    loadcell = new adc::HX711();
    if (loadcell->init()) {
        int32_t value;
        if (loadcell->read_raw(value)) {
            PRINT_BANNER(1, "LOAD CELL ONLINE");
        }
    } else {
        printf("WARNING: Load cell not detected, continuing without it\n");
    }
    PRINT_BANNER(1, "STARTING ACCESS POINT");
    access_point = new http::AccessPoint();
    REQUIRE(access_point->initialize(), "ERROR: Failed to initialize access point\n");
    PRINT_BANNER(1, "ACCESS POINT ONLINE");
    PRINT_BANNER(0, "CORE 0 INITIALIZED");
    return true;
}

static void process_sensor_data() {
    using namespace sdcard;
    using namespace i2c::drivers;

    while (!SensorChannel::empty()) {
        Message* msg = SensorChannel::pop();
        if (msg) {
            switch(msg->type) {
                case MSG_BMP581_DATA: {
                    const auto* data = msg->As<bmp581_data>();
                    SDFile<TelemetryFile>::Write(
                        "BMP581: Temp=%.2f°C, Pressure=%.1f Pa, Altitude=%.1f m\n",
                        data->temperature, data->pressure, data->altitude);
                    http::servers::HttpGenerator::altitude = data->altitude;
                    break;
                }
                case MSG_MS4525_DATA: {
                    const auto* data = msg->As<ms4525d0_data>();
                    SDFile<SpeedFile>::Write(
                        "Pressure: %.2f Pa, Temp: %.1f°C (16x oversample)\n",
                        data->pressure_pa, data->temperature_c);
                    break;
                }
                case MSG_IMU_DATA: {
                    const auto* data = msg->As<icm20948_data>();
                    SDFile<TelemetryFile>::Write(
                        "IMU: Accel=[%.2f, %.2f, %.2f] m/s², Gyro=[%.2f, %.2f, %.2f] rad/s\n",
                        data->accel_x, data->accel_y, data->accel_z,
                        data->gyro_x, data->gyro_y, data->gyro_z);
                    break;
                }
            }
            SensorChannel::release(msg);
        }
    }
}

// Update load cell
static void update_loadcell() {
    uint32_t now = time_us_32();
    if (loadcell && (now - last_loadcell_update > LOADCELL_UPDATE_INTERVAL_US)) {
        if (loadcell->update()) {
            auto data = loadcell->get_data();
            sdcard::SDFile<sdcard::HX711DataLog>::Write(
                "Load: %.2f | Tared: %d | Raw: %d | Time: %d\n",
                data.weight, data.tared_value, data.raw_value, now/1000);
            http::servers::HttpGenerator::force = data.weight;
            last_loadcell_update = now;
        }
    }
}

// Sync files periodically
static void sync_files() {
    uint32_t now = time_us_32();
    if (now - last_file_sync > FILE_SYNC_INTERVAL_US) {
        using namespace sdcard;
        switch(save_sequence % 3) {
            case 0: SDFile<TelemetryFile>::Sync(); break;
            case 1: SDFile<SpeedFile>::Sync(); break;
            case 2: SDFile<HX711DataLog>::Sync(); break;
        }
        save_sequence++;
        last_file_sync = now;
    }
}

// Helper to format unix time
static void unix_to_string(uint32_t unix_time, char* buffer, size_t buffer_size) {
    time_t rawtime = unix_time;
    struct tm* timeinfo = gmtime(&rawtime);
    strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", timeinfo);
}

// Handle HTTP events
static void process_http_events() {
    using namespace sdcard;
    http::servers::Event event;

    if (access_point && access_point->event_handler().pop_event(event)) {
        if (event.name == "start") {
            uint32_t client_unix_time = event.value1 + ARIZONA_OFFSET_SECONDS;
            char time_str[32];
            unix_to_string(client_unix_time, time_str, sizeof(time_str));

            SDFile<TelemetryFile>::Write("Started at: %s.%03d\n", time_str, event.value2);
            SDFile<HX711DataLog>::Write("Started at: %s.%03d\n", time_str, event.value2);
            SDFile<SpeedFile>::Write("Started at: %s.%03d\n", time_str, event.value2);
            printf("Started at: %s.%03d\n", time_str, event.value2);

            // Send start command to Core 1
            if (!polling_active) {
                Message* msg = CommandChannel::acquire();
                if (msg) {
                    msg->type = MSG_CMD_START_POLLING;
                    CommandChannel::commit(msg);
                    polling_active = true;
                }
            }

        } else if (event.name == "stop") {
            uint32_t client_unix_time = event.value1 + ARIZONA_OFFSET_SECONDS;
            char time_str[32];
            unix_to_string(client_unix_time, time_str, sizeof(time_str));

            SDFile<TelemetryFile>::Write("Stopped at: %s.%03d\n", time_str, event.value2);
            SDFile<HX711DataLog>::Write("Stopped at: %s.%03d\n", time_str, event.value2);
            SDFile<SpeedFile>::Write("Stopped at: %s.%03d\n", time_str, event.value2);
            printf("Stopped at: %s.%03d\n", time_str, event.value2);

            // Send stop command to Core 1
            if (polling_active) {
                Message* msg = CommandChannel::acquire();
                if (msg) {
                    msg->type = MSG_CMD_STOP_POLLING;
                    CommandChannel::commit(msg);
                    polling_active = false;
                }
            }
        }
    }
}

// Main loop for Core 0
bool loop() {
    // Check for shutdown
    if (access_point && access_point->is_shutdown_requested()) {
        return false;
    }

    // Check for user exit (x key)
    int c = getchar_timeout_us(0);
    if (c == 'x' || c == 'X') {
        printf("User requested shutdown\n");
        return false;
    }

    // Process all tasks
    update_loadcell();
    process_sensor_data();
    process_http_events();
    sync_files();

    // Handle WiFi polling
    #if PICO_CYW43_ARCH_POLL
    cyw43_arch_poll();
    cyw43_arch_wait_for_work_until(make_timeout_time_ms(50));
    #else
    sleep_ms(1);
    #endif

    return true;
}

// Shutdown Core 0
void shutdown() {
    using namespace sdcard;

    printf("---------- CORE 0 SHUTDOWN ----------\n");

    // Send shutdown to Core 1
    Message* msg = CommandChannel::acquire();
    if (msg) {
        msg->type = MSG_CMD_SHUTDOWN;
        CommandChannel::commit(msg);
    }

    // Final file sync
    SDFile<TelemetryFile>::Sync();
    SDFile<SpeedFile>::Sync();
    SDFile<HX711DataLog>::Sync();
    SDFile<LogFile>::Sync();

    // Clean up hardware
    delete access_point;
    delete loadcell;

    // Close files and unmount
    SDFile<HX711DataLog>::Close();
    SDFile<SpeedFile>::Close();
    SDFile<TelemetryFile>::Close();
    SDFile<LogFile>::Close();
    SDCard::unmount();

    printf("---------- FILESYSTEM SHUTDOWN ----------\n");
}

} // namespace core0