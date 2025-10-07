#pragma once

#include "pico/stdlib.h"
#include "app/SystemCore.h"
#include "app/Scheduler.h"
#include "sdcard.h"
#include "i2c/i2c_bus.h"
#include "i2c/drivers/ads1115.h"
#include "adc/hx711.h"
#include "network/handlers/shared_state.h"

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
        while(true) {
            if (network::handlers::g_shared_state.session_active.load()) {
                 scheduler_.run();
            }
            sleep_ms(1);
        }
    }
};
