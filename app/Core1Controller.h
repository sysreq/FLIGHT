#pragma once

#include "pico/stdlib.h"
#include "app/SystemCore.h"
#include "app/Scheduler.h"
#include "hardware/i2c.h"
#include "hx711_s.h"
#include "ads1115_s.h"

class Core1Controller : public SystemCore<Core1Controller, 8> {
private:
    friend class SystemCore<Core1Controller, 8>;

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

    #define QUIT_ON_FAILURE(_FUNC_, _NAME_)                              \
    do {                                                                 \
    if (!_FUNC_) { printf("Core 1: Failed to initialize " _NAME_ "!\n"); \
    return false; } else { printf("Core 1: " _NAME_ " started.\n"); }        \
    } while(false) 

    bool init_impl() {
        printf("Core 1: Initializing...\n");

        QUIT_ON_FAILURE(scale_.init(), "HX711");
        QUIT_ON_FAILURE(ads1115_.init(), "ADS1115");
        QUIT_ON_FAILURE(ads1115_.start_polling([this](const ADS1115Device::Data& data)
            { this->on_ads1115_data(data); }), "ADS1115 polling");

        add_task(&Core1Controller::poll_hx711, 50);

        printf("Core 1: Initialized successfully.\n");
        return true;
    }

    #undef QUIT_ON_FAILURE

    void loop_impl() {
        __nop();
    }

    void shutdown_impl() {
        printf("Core 1: Shutdown command received. Exiting loop.\n");
        ads1115_.shutdown();
        printf("Core 1: Shutdown complete.\n");
        sleep_ms(100);
    }
};
