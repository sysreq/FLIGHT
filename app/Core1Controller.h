#pragma once

#include "pico/stdlib.h"
#include "app/SystemCore.h"
#include "app/Scheduler.h"
#include "i2c/i2c_bus.h"
#include "i2c/drivers/ads1115.h"
#include "adc/hx711.h"

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
            printf("Scale Data: %.2f [Raw: %d]\n", scale_.weight(), scale_.raw());
        }
    }

    void on_ads1115_data(const ADS1115Data& data) {
        if (data.valid) {
            float voltage = data.voltage + 1.65625f;
            float current = voltage * 78.30445f;
            //TODO: Transmit sensor data back to host from remote
        }
    }

    bool init_impl() {
        printf("Core 1: Initializing...\n");

        if (!scale_.init()) {
            printf("Core 1: Failed to initialize HX711!\n");
            return false;
        }
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

        printf("Core 1: ADS1115 Initialized and polling started by I2C Bus Manager.\n");

        scheduler_.add_task([this]() { this->poll_hx711(); }, 50);

        printf("Core 1: Initialized successfully.\n");
        return true;
    }

    void loop_impl() {
        scheduler_.run();
    }

    void shutdown_impl() {
        printf("Core 1: Shutdown command received. Exiting loop.\n");
        SensorBus::shutdown();
        printf("Core 1: Shutdown complete.\n");
        sleep_ms(100);
    }
};
