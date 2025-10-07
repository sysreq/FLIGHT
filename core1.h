#include "pico/stdlib.h"

#include "core.h"
#include "sdcard.h"

#include "adc/hx711.h"
#include "adc/acs770.h"

#include "network/handlers/shared_state.h"

namespace core1 {

static inline adc::HX711 scale;
       
bool start_hx711() {
    using namespace sdcard;
    printf("Starting HX711...\n");
    REQUIRE(scale.init(),"Failed to initialize HX711!\n");
    printf("\nTesting single reading...\n");

    scale.update();
    if (scale.valid()) {
        printf("Raw: %ld, Tared: %ld, Weight: %.2f\n",
               scale.raw(), scale.tared(), scale.weight());
         printf("HX711 initialized successfully\n");
    } else {
        printf("Failed to read data\n");
        SDFile<LogFile>::Write("Failed to start HX711. Ending.\n");
        sleep_ms(50);
        SDFile<LogFile>::Sync();
        return false;
    }

    network::handlers::g_shared_state.force_sensor_ready.store(true);
    SDFile<LogFile>::Write("HX711 started successfully.\n");
    sleep_ms(50);
    SDFile<LogFile>::Sync();
    return true;
}

bool start_acs700() {
    using namespace sdcard;

    i2c_init(i2c0, 100000);
    gpio_set_function(4, GPIO_FUNC_I2C);
    gpio_set_function(5, GPIO_FUNC_I2C);
    gpio_pull_up(4);
    gpio_pull_up(5);

    sleep_ms(100);

    // Scan I2C bus
    printf("Scanning I2C bus...\n");
    bool found = false;
    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        uint8_t test_data = 0;
        if (i2c_read_blocking(i2c0, addr, &test_data, 1, false) !=
PICO_ERROR_GENERIC) {
            printf("  Found device at 0x%02X\n", addr);
            found = true;
        }
    }
    if (!found) {
        printf("No I2C devices found! Check wiring.\n");
        sleep_ms(500);
        return 1;
    }

    printf("\nADS1115 Ready. Using A0-A1 differential mode with 1.65V ref on A1.\n");
    SDFile<Current>::Write("ADS1115 Ready. Using A0-A1 differential mode with 1.65V ref on A1.\n");
    SDFile<Current>::Sync();
    network::handlers::g_shared_state.power_ready.store(true);
    sleep_ms(10);

    return true;
}



bool init() {
    using namespace sdcard;
    REQUIRE(start_hx711(), "Failed to start HX711.\n");
    REQUIRE(start_acs700(), "Failed to start ACS770.\n");
    sleep_ms(50);
    printf("Core 1 Started.\n");
    SDFile<LogFile>::Write("Core 1 Started.\n");
    SDFile<LogFile>::Sync();
    sleep_ms(50);
    return true;
}

static constexpr uint32_t HX711_POLL_RATE = 50;
static constexpr uint32_t POWER_POLL_RATE = 35;
static constexpr uint32_t SAVE_RATE = 1000;

bool loop() {
    using namespace sdcard;
    auto *shared_state = &network::handlers::g_shared_state;

    uint32_t now = time_us_32();
    uint32_t last_data_flush = now;
    uint32_t last_hx711_update = now;
    uint32_t last_power_update = now;

    printf("\nStarting polling loop at 10 Hz...\n");
    SDFile<Force>::Write("HX711 started.\n");
    int last_state = false;

    while(true) {
        if(last_state != shared_state->session_active.load())
        {
            last_state = shared_state->session_active;

            if(last_state == true) {
                printf("Starting to log @ %llu\n", shared_state->session_start_time.load());
            }
        }

        now = time_us_32();
        if(last_state) {
            if(last_hx711_update - now >= HX711_POLL_RATE * 1000) {
                last_hx711_update = now;
                scale.update();
                if (scale.valid()) {
                    SDFile<Force>::Write("(%u) Force: %.2f lbs\n", now,  scale.weight());
                    shared_state->force_value.store(scale.weight());
                } else {
                    int stored_force_value = shared_state->force_value.load();
                    stored_force_value = (stored_force_value < -1000) ? -1000.0f : (stored_force_value - 1);
                    shared_state->force_value.store(stored_force_value);
                }
            }

            if(last_power_update - now >= POWER_POLL_RATE * 1000) {
                last_power_update = now;
                int16_t adc_raw = ads1115_read_differential(i2c0);
                float voltage = (adc_raw * 2.048f) / 32768.0f;
                voltage += 1.65625f;
                float current = voltage * 78.30445f;
                SDFile<Current>::Write("(%u) Amps: %.3fA [ADC: %d (%.3fV)]\n", now, current, adc_raw, voltage);
                shared_state->power.store(current);

            }

            if(last_data_flush - now >= SAVE_RATE * 1000){
                last_data_flush = now;
                SDFile<Force>::Sync();
                SDFile<Current>::Sync();
            }
        }
        


        sleep_ms(1);
    }
}

void shutdown() {

}

} // namespace core0