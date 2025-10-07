#include "pico/stdlib.h"
#include "pico/stdio.h"
#include "pico/multicore.h"

#include <cstdio>

#include "core.h"
#include "core0.h"
#include "core1.h"

int main() {
    stdio_init_all();
    sleep_ms(3000);

    printf("Start Sequence.\n");

    core0::init();
    core1::init();

    multicore_launch_core1([]() { core1::loop(); });
    
    do {
        core0::loop();
    } while(true);

    sleep_ms(1000);
    return 0;
}
