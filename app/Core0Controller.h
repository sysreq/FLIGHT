#pragma once

#include "pico/stdlib.h"
#include "app/SystemCore.h"
#include "app/Scheduler.h"
#include "pico/printf.h"

class Core0Controller : public SystemCore<Core0Controller> {
private:
    friend class SystemCore<Core0Controller>;

    Scheduler scheduler_;

    bool init_impl() {
        printf("Core 0: Initializing...\n");

        scheduler_.add_task([]() {

        }, 10);

        printf("Core 0: Initialized successfully.\n");
        return true;
    }

    void loop_impl() {
        scheduler_.run();
    }

    void shutdown_impl() {
        printf("Core 0: Shutdown command received. Exiting loop.\n");
        printf("Core 0: Shutdown complete.\n");
        sleep_ms(100);
    }
};
