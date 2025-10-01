#include "app_config.h"
// ============================================
int main() {
    stdio_init_all();
    sleep_ms(3000);

    PRINT_HEADER("FLIGHT System v2.0");
    REQUIREN(core0::init(), "\t\tERROR: Core 0 initialization failed\n");
    REQUIREN(core1::init(), "\t\tERROR: Core 1 initialization failed\n");
    sleep_ms(100);
    PRINT_BANNER(0, "STARTING MAIN LOOP");
    multicore_launch_core1([]{ while (core1::loop()); });
    while (core0::loop());
    PRINT_BANNER(0, "STOPPED MAIN LOOP");

    core0::shutdown();
    core1::shutdown();
    sleep_ms(500);
    PRINT_HEADER("SHUTDOWN COMPLETE");
    sleep_ms(500);
    return 0;
}