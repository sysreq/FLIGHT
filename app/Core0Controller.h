#pragma once

#include "pico/stdlib.h"
#include "app/SystemCore.h"
#include "app/Scheduler.h"
#include "pico/printf.h"

#include "ftl/ftl.h"

class Core0Controller : public SystemCore<Core0Controller, 8> {
private:
    friend class SystemCore<Core0Controller, 8>;

    uint32_t last_heartbeat = 0;
    uint32_t now = 0;

    ftl::messages::Dispatcher dispatcher;

    bool init_impl() {
        printf("Core 0: Initializing...\n");
        ftl::initialize();
        printf("Core 0: Initialized successfully.\n");
        return true;
    }

    void loop_impl() {
        ftl::poll();

        // Periodically send heartbeat message
        now = to_ms_since_boot(get_absolute_time());
        if (now - last_heartbeat >= 1000 && ftl::is_tx_ready()) {
            dispatcher.send<ftl::messages::MSG_REMOTE_LOG>(now, "Heartbeat from Core 0");
            printf("Core 0: Sent heartbeat at %u ms\n", now);
            last_heartbeat = now;
        }
    }

    void shutdown_impl() {
        printf("Core 0: Shutdown command received. Exiting loop.\n");
        printf("Core 0: Shutdown complete.\n");
        sleep_ms(100);
    }
};
