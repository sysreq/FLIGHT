#include "pico/stdlib.h"
#include "pico/stdio.h"
#include "pico/multicore.h"
#include "pico/bootrom.h"

#include "pico/printf.h"

#include "sdcard.h"

const char* build_timestamp = "012 @ " __DATE__ " " __TIME__;

int main() {
    stdio_init_all();
    sleep_ms(5000);

    printf("System: Initializing Core 0...\n");
    sleep_ms(1000);

    while(!sdcard::SDCard::mount()) { 
        printf("Mount: Failed.\n");
        sleep_ms(1000);
    }
 
    if (!sdcard::SDFile<sdcard::LogFile>::Open()) { 
        printf("Open: Failed.\n");
        return -1;
    }
    
    printf("Core 0: File System Started.\n");
    sdcard::SDFile<sdcard::LogFile>::Write("Device Started: %s.\n", build_timestamp);
    sdcard::SDFile<sdcard::LogFile>::Sync(); 

    sleep_ms(1000);

    while(true)
    {
        printf("Core 0: File System Started.\n");
        sleep_ms(1000);
    }
}