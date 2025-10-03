#include <cstdio>

#include <pico/stdio.h>
#include <pico/time.h>
// ============================================
int main() {
    stdio_init_all();
    sleep_ms(3000);
    printf("Hello World!");
    sleep_ms(500);
    return 0;
}