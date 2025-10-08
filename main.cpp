#include "pico/stdlib.h"
#include "pico/stdio.h"
#include "pico/multicore.h"
#include "pico/bootrom.h"
#include "hardware/resets.h"

#include "app/Core0Controller.h"
#include "app/Core1Controller.h"

// Create the controller instances for each core
static Core0Controller core0_controller;
static Core1Controller core1_controller;

// Core 1's entry point
void core1_entry() {
    if (core1_controller.init()) {
        core1_controller.loop();
    } else {
        printf("FATAL: Core 1 failed to initialize.\n");
        sleep_ms(100);
        return;
    }
}

int main() {
    stdio_init_all();
    sleep_ms(3000);

    printf("System: Initializing Core 0...\n");
    if (core0_controller.init()) {
        printf("System: Launching Core 1...\n");
        multicore_launch_core1(core1_entry);
        sleep_ms(10);
        printf("System: Core 0 entering main loop.\n");
        core0_controller.loop();
    } else {
        printf("FATAL: Core 0 failed to initialize. System halted.\n");
        sleep_ms(100);
    }

    printf("System: Core 0 main loop exited. Shutdown complete.\n");
    sleep_ms(100);
    reset_block_num(RESET_USBCTRL);

    int i = 0;
    while(stdio_usb_connected() && i < 100) {
        sleep_ms(10);
        i++;
        continue;
    }
    
    reset_usb_boot(0, 0);
    return 0;
}