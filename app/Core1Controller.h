#pragma once

#include "pico/stdlib.h"
#include "app/SystemCore.h"
#include "app/Scheduler.h"
#include "hardware/i2c.h"
#include "hx711_s.h"
#include "ads1115_s.h"

class Core1Controller : public SystemCore<Core1Controller> {
private:
    friend class SystemCore<Core1Controller>;

    Scheduler scheduler_;
    HX711Device scale_;
    ADS1115Device ads1115_;
    
    void poll_hx711() {
        scale_.update();
        if (scale_.valid()) {
            printf("Scale Data: %.2f [Raw: %d, Tared: %d]\n", scale_.weight(), scale_.raw(), scale_.tared());
        }
    }

    void on_ads1115_data(const ADS1115Device::Data& data) {
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

        // Initialize ADS1115 with I2C instance
        if (!ads1115_.init()) {
            printf("Core 1: Failed to initialize ADS1115!\n");
            return false;
        }
        printf("Core 1: ADS1115 Initialized.\n");

        // Start polling for ADS1115 data
        if (!ads1115_.start_polling([this](const ADS1115Device::Data& data) {
            this->on_ads1115_data(data);
        })) {
            printf("Core 1: Failed to start ADS1115 polling!\n");
            return false;
        }
        printf("Core 1: ADS1115 polling started.\n");

        scheduler_.add_task([this]() { this->poll_hx711(); }, 50);

        printf("Core 1: Initialized successfully.\n");
        return true;
    }

    void loop_impl() {
        scheduler_.run();
    }

    void shutdown_impl() {
        printf("Core 1: Shutdown command received. Exiting loop.\n");
        ads1115_.shutdown();
        printf("Core 1: Shutdown complete.\n");
        sleep_ms(100);
    }
};
