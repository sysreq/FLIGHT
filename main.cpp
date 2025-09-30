#include <ctime>

#include "pico\stdlib.h"
#include "pico\stdio.h"
#include "pico\multicore.h"

#include "common\channels.h"

#include "http\access_point.h"

//#include "storage.h"
#include "sdcard.h"
#include "i2c.h"

#include "adc\hx711.h"

using TelemetryBus = i2c::I2CBus<i2c0, 4, 5, 400000>;  // SDA=4, SCL=5, 400kHz

template<typename T>
inline bool is_failure(T expr) {
    if constexpr (std::is_same_v<std::remove_cvref_t<T>, int>) {
        return expr < 0;
    } else if constexpr (std::is_same_v<std::remove_cvref_t<T>, bool>) {
        return expr == false;
    }
    return false;
}

#define LOG printf

#define REQUIRE(expr, msg) \
    if (is_failure(expr)) { \
        printf(msg); \
        sleep_ms(50); \
        return -1; \
    }

struct SensorData {};
struct SystemCommands {};

using SensorChannel = MessageChannel<SensorData>;
using CommandChannel = MessageChannel<SystemCommands>;

enum SensorTypes : uint8_t {
    MSG_IMU_DATA = 1,
    MSG_BMP581_DATA = 2,
    MSG_MS4525_DATA = 3
};

void handle_imu_data(const i2c::drivers::icm20948_data& data) {
    if (data.valid) {
        Message* msg = SensorChannel::acquire();
        if (msg) {
            msg->type = MSG_IMU_DATA;
            msg->Put(data);
            SensorChannel::commit(msg);
        } else {
            LOG("Failed to acquire ICM buffer!");
        }
    }
}

void handle_bmp581_data(const i2c::drivers::bmp581_data& data) {
    if (data.valid) {
        Message* msg = SensorChannel::acquire();
        if (msg) {
            msg->type = MSG_BMP581_DATA;
            msg->Put(data);
            SensorChannel::commit(msg);
        } else {
            LOG("Failed to acquire BMP buffer!");
        }
    }
}

void handle_ms4525d0_data(const i2c::drivers::ms4525d0_data data) {
    static std::array<i2c::drivers::ms4525d0_data, 16> oversample;
    static size_t sample_size = 0;

    oversample[sample_size++] = data;

    if (sample_size >= 16) {
        float avg_pressure = 0.0f;
        float avg_temperature = 0.0f;
        int valid_count = 0;
        
        for (const auto& sample : oversample) {
            if(sample.valid == true) {
                valid_count += 1;
                avg_pressure += sample.pressure_pa;
                avg_temperature += sample.temperature_c;
            }

        }
        
        if(valid_count > 0) {
            Message* msg = SensorChannel::acquire();
            if (msg) {
                i2c::drivers::ms4525d0_data temp;
                temp.pressure_pa = avg_pressure / valid_count;
                temp.temperature_c = avg_temperature / valid_count;

                msg->type = MSG_MS4525_DATA;
                msg->Put(temp);
                SensorChannel::commit(msg);
            } else {
                LOG("Failed to acquire MS4525 buffer!");
            }
        }
        
        sample_size = 0;
    }
}

int init_i2c_bus0() {
    using namespace i2c::drivers;

    LOG("---------- STARTING I2C BUS 0 (IMU and BMP) ----------\n");
    REQUIRE(TelemetryBus::start(), "Failed to initialize I2C bus");
    //REQUIRE(TelemetryBus::add_device<ICM20948>(handle_imu_data), "Failed to add ICM20948\n");
    REQUIRE(TelemetryBus::add_device<MS4525D0>(handle_ms4525d0_data), "Failed to add the MS4525D0\n");
    REQUIRE(TelemetryBus::add_device<BMP581>(handle_bmp581_data), "Failed to add BMP581\n");
    LOG("----------  I2C BUS 0 ONLINE ----------\n");
    return 0;
}

enum CommandTypes : uint8_t {
    MSG_CMD_SHUTDOWN = 1
};

bool check_for_exit() {
    int c = getchar_timeout_us(0);
    if (c == 'x' || c == 'X') {
        Message* msg = CommandChannel::acquire();
        if (msg) {
            msg->type = MSG_CMD_SHUTDOWN;
            CommandChannel::commit(msg);
        } else {
            LOG("Failed to acquire Cmd buffer!");
        }
        return true;
    }
    return false;
}

void core1_entry() {
    printf("---------- ENABLING DATA POLLING ----------\n");
    TelemetryBus::enable();

    while(true) {
        if (!CommandChannel::empty()) { 
            Message* msg = CommandChannel::pop();
            if (msg && msg->type == MSG_CMD_SHUTDOWN) {
                CommandChannel::release(msg); 
                break;
            }
            sleep_ms(100);
        }
    }

    TelemetryBus::disable();
    printf("---------- DISABLING DATA POLLING ----------\n");

    using namespace sdcard;
    SDFile<TelemetryFile>::Sync();
    SDFile<SpeedFile>::Sync();
    SDFile<HX711DataLog>::Sync();

    printf("Synced.\n");
}

int init_file_system() {
    using namespace sdcard;
    LOG("---------- STARTING FILESYSTEM ----------\n");
    REQUIRE(SDCard::mount(), "Failed to mount SDCard.\n");
    REQUIRE(SDFile<LogFile>::Open(), "Failed to initialize LogFile.\n");
    REQUIRE(SDFile<TelemetryFile>::Open(), "Failed to initialize TelemetryFile.\n");
    REQUIRE(SDFile<SpeedFile>::Open(), "Failed to initialize SpeedFile.\n");
    REQUIRE(SDFile<HX711DataLog>::Open(), "Failed to initialize HX711DataLog.\n");
    LOG("---------- FILESYSTEM ONLINE ----------\n");
    return 0;
}

int main2() {
    stdio_init_all();
    sleep_ms(5000);

    REQUIRE(init_i2c_bus0(), "Failed to start inertial sensor bus.\n");

    sleep_ms(1000);

    LOG("---------- STARTING MAIN LOOP ----------\n");
    
    bool running = true;

    uint32_t last_sync = to_ms_since_boot(get_absolute_time());
    uint32_t writes_since_sync = 0;

    uint32_t imu_last = 0;
    uint32_t bmp_last = 0;
    uint32_t air_last = 0;

    multicore_launch_core1(core1_entry);

    while (running) {
        if(check_for_exit()) {
            sleep_ms(200);
            running = false;
        }

        while (!SensorChannel::empty()) {
            using namespace i2c::drivers;
            Message* msg = SensorChannel::pop();
            if (msg) {
                switch(msg->type) {
                    case MSG_IMU_DATA: {
                        const auto* imu_data = msg->As<icm20948_data>();
                        // SDFile<TelemetryFile>::Write("IMU: Accel=[%.2f, %.2f, %.2f] m/s², Gyro=[%.2f, %.2f, %.2f] rad/s\n",
                        //     imu_data->accel_x, imu_data->accel_y, imu_data->accel_z,
                        //     imu_data->gyro_x, imu_data->gyro_y, imu_data->gyro_z);
                        // SDFile<IMUTimeFile>::Write("%d\n", msg->acquire_time - imu_last);
                        imu_last = msg->acquire_time;
                        writes_since_sync++;
                        break;
                    }

                    case MSG_MS4525_DATA: {
                        const auto* ms4525_data = msg->As<ms4525d0_data>();
                        // SDFile<TelemetryFile>::Write("Pressure: %.2f Pa, Temp: %.1f°C (16x oversample)\n", 
                        //         ms4525_data->pressure_pa, ms4525_data->temperature_c);
                        // SDFile<AIRTimeFile>::Write("%d\n", msg->acquire_time - air_last);
                        air_last = msg->acquire_time;
                        writes_since_sync++;
                        break;
                    }
                }
                SensorChannel::release(msg); 
            }
        }
    
        sleep_ms(1);
    }
    
    LOG("---------- STOPPED MAIN LOOP ----------\n");

    sleep_ms(500);
    LOG("Sensor Queue Empty? %s\n", SensorChannel::empty() ? "True" : "False");
    LOG("Logger shutting down.\n");
    sleep_ms(500);
    sleep_ms(500);
    LOG("Goodbye.\n");
    return 0;
}

void unix_to_string(uint32_t unix_time, char* buffer, size_t buffer_size) {
    time_t rawtime = unix_time;
    struct tm* timeinfo = gmtime(&rawtime);
    
    strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", timeinfo);
}

int main3() {
    stdio_init_all();
    sleep_ms(1000);

    http::AccessPoint access_point;
    if (!access_point.initialize()) {
        printf("Failed to initialize access point!\n");
        return 1;
    }
    printf("Access point started successfully\n");

    int32_t update_time = time_us_32();
    int32_t now = update_time;

    while (!access_point.is_shutdown_requested()) {
        #if PICO_CYW43_ARCH_POLL
        cyw43_arch_poll();
        cyw43_arch_wait_for_work_until(make_timeout_time_ms(50));
        #else
        sleep_ms(50);
        #endif
        
        http::servers::Event event;
        if (access_point.event_handler().pop_event(event)) {
            if (event.name == "start" || event.name == "stop") {
                static constexpr int ARIZONA_OFFSET_SECONDS = -7 * 3600;  // UTC-7
                uint32_t client_unix_time = event.value1 + ARIZONA_OFFSET_SECONDS;
                char time_str[32];
                
                unix_to_string(client_unix_time, time_str, sizeof(time_str));
    
                printf("Event: %s at client time: %s.%03d\n", event.name.c_str(), time_str, event.value2);
            } else {
                printf("Event '%s' at %lu ms (v1=%d, v2=%d, f=%.2f)\n", 
                    event.name.c_str(), 
                    (unsigned long)event.timestamp,
                    event.value1, event.value2, event.fvalue);
            }
        }
        
        now = time_us_32();
        if(now - update_time > 10'000'000) {
            update_time = now;
            printf("Beep.\n");
        }
    }
    
    printf("Shutting down...\n");
    return 0;
}

int main() {
    using namespace sdcard;
    using namespace adc;
    using namespace i2c;
    using namespace i2c::drivers;

    stdio_init_all();
    sleep_ms(3000);

    printf("---------- STARTING FILESYSTEM ----------\n");
    REQUIRE(SDCard::mount(), "Failed to mount SDCard.\n");
    REQUIRE(SDFile<LogFile>::Open(), "Failed to initialize LogFile.\n");
    REQUIRE(SDFile<TelemetryFile>::Open(), "Failed to initialize TelemetryFile.\n");
    REQUIRE(SDFile<SpeedFile>::Open(), "Failed to initialize SpeedFile.\n");
    REQUIRE(SDFile<HX711DataLog>::Open(), "Failed to initialize HX711DataLog.\n");
    printf("---------- FILESYSTEM ONLINE ----------\n");

    printf("---------- STARTING I2C BUS 0 (IMU and BMP) ----------\n");
    REQUIRE(TelemetryBus::start(), "Failed to initialize I2C bus");
    //REQUIRE(TelemetryBus::add_device<MS4525D0>(handle_ms4525d0_data), "Failed to add the MS4525D0\n");
    REQUIRE(TelemetryBus::add_device<BMP581>(handle_bmp581_data), "Failed to add BMP581\n");
    LOG("----------  I2C BUS 0 ONLINE ----------\n");

    HX711 loadcell;
    if (loadcell.init()) {
        int32_t value;
        if (loadcell.read_raw(value)) {
            printf("Load cell initialized.\n");
            sleep_ms(100);
        }
    }

    http::AccessPoint access_point;
    if (!access_point.initialize()) {
        printf("Failed to initialize access point!\n");
        return 1;
    }
    printf("Access point started successfully\n");
    
    uint32_t last_loadcell = 0;
    uint32_t last_save = 0;
    uint32_t save_sequence = 0;

    while(!check_for_exit()) {
        int now = time_us_32();

        if (now - last_loadcell > 20'000 && loadcell.update()) {
            auto data = loadcell.get_data();
            SDFile<HX711DataLog>::Write("Load: %.2f | Tared: %d | Raw: %d\n | Time: %d", data.weight, data.tared_value, data.raw_value, now/1000);
            http::servers::HttpGenerator::force = data.weight; 
            last_loadcell = now;
        }


        while (!SensorChannel::empty()) {
            using namespace i2c::drivers;
            Message* msg = SensorChannel::pop();
            if (msg) {
                switch(msg->type) {
                    case MSG_BMP581_DATA: {
                        const auto* bmp_data = msg->As<bmp581_data>();
                        SDFile<TelemetryFile>::Write("BMP581: Temp=%.2f°C, Pressure=%.1f Pa, Altitude=%.1f m\n",
                        bmp_data->temperature, bmp_data->pressure, bmp_data->altitude);
                        http::servers::HttpGenerator::altitude = bmp_data->altitude;   // Altitude value  
                        break; }
                    case MSG_MS4525_DATA: {
                        const auto* ms4525_data = msg->As<ms4525d0_data>();
                        SDFile<SpeedFile>::Write("Pressure: %.2f Pa, Temp: %.1f°C (16x oversample)\n", 
                                 ms4525_data->pressure_pa, ms4525_data->temperature_c);
                        break; }
                }
                SensorChannel::release(msg);
            }
        }
        if (now - last_save > 1'000'000) {
            if(save_sequence == 0)
                SDFile<TelemetryFile>::Sync();
            else if(save_sequence == 1)
                SDFile<SpeedFile>::Sync();
            else if(save_sequence == 2)
                SDFile<HX711DataLog>::Sync();
            else 
                save_sequence = 0;
            save_sequence += 1;
            last_save = now;
        }

        #if PICO_CYW43_ARCH_POLL
        cyw43_arch_poll();
        cyw43_arch_wait_for_work_until(make_timeout_time_ms(50));
        #else
        sleep_ms(50);
        #endif
        
        http::servers::Event event;
        if (access_point.event_handler().pop_event(event)) {
            if (event.name == "start") {
                static constexpr int ARIZONA_OFFSET_SECONDS = -7 * 3600;  // UTC-7
                uint32_t client_unix_time = event.value1 + ARIZONA_OFFSET_SECONDS;
                char time_str[32];
                
                unix_to_string(client_unix_time, time_str, sizeof(time_str));
    
                SDFile<TelemetryFile>::Write("Started: %s at client time: %s.%03d\n", event.name.c_str(), time_str, event.value2);
                SDFile<HX711DataLog>::Write("Started: %s at client time: %s.%03d\n", event.name.c_str(), time_str, event.value2);
                SDFile<SpeedFile>::Write("Started: %s at client time: %s.%03d\n", event.name.c_str(), time_str, event.value2);
                printf("Started: %s at client time: %s.%03d\n", event.name.c_str(), time_str, event.value2);
                multicore_launch_core1(core1_entry);
            } else if (event.name == "stop") {
                static constexpr int ARIZONA_OFFSET_SECONDS = -7 * 3600;  // UTC-7
                uint32_t client_unix_time = event.value1 + ARIZONA_OFFSET_SECONDS;
                char time_str[32];
                
                unix_to_string(client_unix_time, time_str, sizeof(time_str));

                SDFile<TelemetryFile>::Write("Stopped: %s at client time: %s.%03d\n", event.name.c_str(), time_str, event.value2);
                SDFile<HX711DataLog>::Write("Stopped: %s at client time: %s.%03d\n", event.name.c_str(), time_str, event.value2);
                SDFile<SpeedFile>::Write("Stopped: %s at client time: %s.%03d\n", event.name.c_str(), time_str, event.value2);
                printf("Stopped: %s at client time: %s.%03d\n", event.name.c_str(), time_str, event.value2);
                Message* msg = CommandChannel::acquire();
                if (msg) {
                    msg->type = MSG_CMD_SHUTDOWN;
                    CommandChannel::commit(msg);
                } else {
                    printf("Failed to acquire Cmd buffer!");
                }
                
            } else {
                printf("Event '%s' at %lu ms (v1=%d, v2=%d, f=%.2f)\n", 
                    event.name.c_str(), 
                    (unsigned long)event.timestamp,
                    event.value1, event.value2, event.fvalue);
            }
        }

        sleep_ms(1);
    }
    SDFile<TelemetryFile>::Sync();
    SDFile<HX711DataLog>::Sync();
    printf("Shutting down...\n");
    return 0;
}
