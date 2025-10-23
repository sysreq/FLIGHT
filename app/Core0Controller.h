#pragma once

#include "pico/stdlib.h"
#include "app/SystemCore.h"
#include "app/Scheduler.h"
#include "pico/printf.h"

#include "ftl/ftl.h"

class Core0Controller : public SystemCore<Core0Controller, 8> {
private:
    friend class SystemCore<Core0Controller, 8>;

    bool init_impl() {
        printf("Core 0: Initializing...\n");
        ftl::initialize();
        printf("Core 0: Initialized successfully.\n");
        return true;
    }

    void loop_impl() {
        ftl::messages::Dispatcher dispatcher;
        __nop();
    }

    void shutdown_impl() {
        printf("Core 0: Shutdown command received. Exiting loop.\n");
        printf("Core 0: Shutdown complete.\n");
        sleep_ms(100);
    }
};
