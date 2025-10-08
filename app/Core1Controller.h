#pragma once

#include "pico/stdlib.h"
#include "app/SystemCore.h"
#include "app/Scheduler.h"
#include "sdcard.h"
#include "i2c/i2c_bus.h"
#include "i2c/drivers/ads1115.h"
#include "adc/hx711.h"
#include "network/handlers/shared_state.h"

#include <cctype>

using SensorBus = i2c::I2CBus<i2c0, 4, 5>;
using ADS1115Data = i2c::drivers::ads1115_data;

class Core1Controller : public SystemCore<Core1Controller> {
private:
    friend class SystemCore<Core1Controller>;

    Scheduler scheduler_;
    adc::HX711 scale_;

    void poll_hx711() {
        scale_.update();
        if (scale_.valid()) {
            sdcard::SDFile<sdcard::Force>::Write("(%u) Force: %.2f lbs\n", time_us_32(), scale_.weight());
            network::handlers::g_shared_state.force_value.store(scale_.weight());
        } else {
            int stored_force_value = network::handlers::g_shared_state.force_value.load();
            stored_force_value = (stored_force_value < -1000) ? -1000.0f : (stored_force_value - 1);
            network::handlers::g_shared_state.force_value.store(stored_force_value);
        }
    }

    void flush_sd_cards() {
        sdcard::SDFile<sdcard::Force>::Sync();
        sdcard::SDFile<sdcard::Current>::Sync();
    }

    void on_ads1115_data(const ADS1115Data& data) {
        if (data.valid) {
            float voltage = data.voltage + 1.65625f;
            float current = voltage * 78.30445f;
            sdcard::SDFile<sdcard::Current>::Write("(%u) Amps: %.3fA [ADC: %d (%.3fV)]\n", time_us_32(), current, data.raw, voltage);
            network::handlers::g_shared_state.power.store(current);
        }
    }

    bool init_impl() {
        printf("Core 1: Initializing...\n");

        if (!scale_.init()) {
            printf("Core 1: Failed to initialize HX711!\n");
            return false;
        }
        network::handlers::g_shared_state.force_sensor_ready.store(true);
        printf("Core 1: HX711 Initialized.\n");

        SensorBus::start();
        SensorBus::add_device<i2c::drivers::ADS1115>([this](const ADS1115Data& data) { 
            this->on_ads1115_data(data); 
        });
        
        auto& ads = SensorBus::get_device<i2c::drivers::ADS1115>();
        ads.configure(
            i2c::drivers::ADS1115::Mux::DIFF_0_1, 
            i2c::drivers::ADS1115::Gain::FS_2_048V, 
            i2c::drivers::ADS1115::Rate::SPS_64
        );

        network::handlers::g_shared_state.power_ready.store(true);
        printf("Core 1: ADS1115 Initialized and polling started by I2C Bus Manager.\n");

        scheduler_.add_task([this]() { this->poll_hx711(); }, 50);
        scheduler_.add_task([this]() { this->flush_sd_cards(); }, 1000);

        printf("Core 1: Initialized successfully.\n");
        return true;
    }

    void loop_impl() {
        bool is_scheduler_active = network::handlers::g_shared_state.session_active.load();
        if (is_scheduler_active) {
            scheduler_.run();
        } else {
            int8_t c = stdio_getchar_timeout_us(0);
            if(c != PICO_ERROR_TIMEOUT) {
                switch(std::tolower(c)) {
                    case 'x':{
                        this->shutdown();
                        return;
                    }
                    case 'z': {
                        scale_.zero();
                        printf("Core 1: HX711 Zero'd.\n");
                        break;
                    }
                    case 's': {
                        static constexpr uint32_t DEFAULT_TIMEOUT = 30000000; // 30 seconds
                        uint32_t dynamic_timeout = DEFAULT_TIMEOUT; // 30 seconds
                        char buffer[32];
                        bool input_valid = true;

                        printf("Core 1: Enter calibration weight in lbs (e.g., 5.0): ");
                        buffer[0] = '\0';
                        do {
                            c = stdio_getchar_timeout_us(dynamic_timeout);
                            if(c == PICO_ERROR_TIMEOUT && dynamic_timeout == DEFAULT_TIMEOUT) {
                                printf("\nCore 1: Calibration input timed out.\n");
                                input_valid = false;
                                break;
                            } else if(c == PICO_ERROR_TIMEOUT && dynamic_timeout != DEFAULT_TIMEOUT) {
                                break;
                            } else if(std::isdigit(c) || c == '-' || c == '+' || c == '.') {
                                size_t len = strlen(buffer);
                                if (len < sizeof(buffer) - 1) {
                                    buffer[len] = c;
                                    buffer[len + 1] = '\0';
                                }
                                dynamic_timeout = 1000;
                                input_valid = true;
                            } else {
                                printf("Core 1: Received non-numeric input, ending calibration input.\n");
                                input_valid = false;
                                break;
                            }
                        } while (true);

                        if (!input_valid || strlen(buffer) == 0) {
                            printf("Core 1: No valid calibration input received.\n");
                            break;
                        }

                        float calibration_value = atof(buffer);
                        if (calibration_value != 0 && scale_.get_calibration_sample(calibration_value, 32)) {
                            printf("Core 1: HX711 Calibration sample for %.2f lbs recorded.\n", calibration_value);
                        } else {
                            printf("Core 1: HX711 Failed to record calibration sample for %.2f lbs.\n", calibration_value);
                        }
                        break;
                    }
                    case 't': {
                        bool result = scale_.calibrate_from_samples();
                        if (result) {
                            printf("Core 1: HX711 Calibration successful.\n");
                            printf("\tScale: %.2f | Offset: %d\n", scale_.get_scale(), scale_.get_offset());
                        } else {
                            printf("Core 1: HX711 Calibration failed.\n");
                        }
                        break;
                    }
                    case 'w': {
                        scale_.update(32);
                        if (scale_.valid()) {
                            printf("Core 1: HX711 Current Weight: %.2f lbs (Raw: %d, Tared: %d)\n", 
                                   scale_.weight(), scale_.raw(), scale_.tared());
                        } else {
                            printf("Core 1: HX711 Current Weight: Invalid reading\n");
                        }  
                        break;
                    }

                    default: {
                        printf("Core 1: Unknown command '%c' received via stdin. Current Uptime: %d\n", c, time_us_32());
                        break;
                    }
                }
            }
        }
    }

    void shutdown_impl() {
        printf("Core 1: Shutdown command received. Exiting loop.\n");
        SensorBus::shutdown();
        sdcard::SDFile<sdcard::Force>::Close();
        sdcard::SDFile<sdcard::Current>::Close();
        printf("Core 1: Shutdown complete.\n");
        sleep_ms(100);
    }
};
