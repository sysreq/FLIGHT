#pragma once

#include <stdint.h>
#include "pico/stdlib.h"
#include "RP2350.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline uint32_t millis() {
    __COMPILER_BARRIER();
    return time_us_64() / 1000;
    __COMPILER_BARRIER();
}

static inline void delay_ms(uint32_t ulTime_ms) {
    sleep_ms(ulTime_ms);
}

static inline uint64_t micros() {
    __COMPILER_BARRIER();
    return to_us_since_boot(get_absolute_time());
    __COMPILER_BARRIER();
}

#ifdef __cplusplus
}
#endif
