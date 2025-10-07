#include <cstdio>
#include <pico/stdio.h>
#include <pico/time.h>
#include <pico/sync.h>
#include <hardware/gpio.h>
#include <hardware/i2c.h>

#define ADS1115_ADDR 0x48
#define ADS1115_REG_CONFIG 0x01
#define ADS1115_REG_CONVERSION 0x00

int16_t ads1115_read_differential(i2c_inst_t* i2c) {
    // Config: single-shot, AIN0-AIN1 differential, ±2.048V, 64SPS
    uint16_t config = 0x8000 |  // Start single conversion + single-shot mode
                      0x0000 |  // AIN0-AIN1 differential (000)
                      0x0400 |  // ±2.048V range (010)
                      0x0100 |  // Single-shot mode
                      0x0060 |  // 64 SPS
                      0x0003;   // Disable comparator

    uint8_t config_data[3] = {ADS1115_REG_CONFIG, (uint8_t)(config >> 8), (uint8_t)(config & 0xFF)};

    int result = -1;
    for(int i = 0; i < 3; i++)
    {
        result = i2c_write_blocking(i2c, ADS1115_ADDR, config_data, 3, false);
        if(result >= 0)
            break;

    }

    if (result < 0) {
        printf("I2C write failed: %d\n", result);
        return 0;
    }

    sleep_ms(20);  // Wait for conversion (64 SPS = ~15.6ms)

    uint8_t reg = ADS1115_REG_CONVERSION;
    i2c_write_blocking(i2c, ADS1115_ADDR, &reg, 1, true);

    uint8_t data[2];
    result = i2c_read_blocking(i2c, ADS1115_ADDR, data, 2, false);
    if (result < 0) {
        printf("I2C read failed: %d\n", result);
        return 0;
    }

    return (int16_t)((data[0] << 8) | data[1]);
}

int16_t ads1115_read_a2(i2c_inst_t* i2c) {
    // Config: single-shot, AIN2 single-ended, ±2.048V, 64SPS
    uint16_t config = 0x8000 |  // Start single conversion + single-shot mode
                      0x6000 |  // AIN2 single-ended (110)
                      0x0400 |  // ±2.048V range (010)
                      0x0100 |  // Single-shot mode
                      0x0060 |  // 64 SPS
                      0x0003;   // Disable comparator

    uint8_t config_data[3] = {ADS1115_REG_CONFIG, (uint8_t)(config >>
8), (uint8_t)(config & 0xFF)};
    int result = i2c_write_blocking(i2c, ADS1115_ADDR, config_data, 3, false);
    if (result < 0) {
        printf("I2C write failed: %d\n", result);
        return 0;
    }

    sleep_ms(20);  // Wait for conversion (64 SPS = ~15.6ms)

    uint8_t reg = ADS1115_REG_CONVERSION;
    i2c_write_blocking(i2c, ADS1115_ADDR, &reg, 1, true);

    uint8_t data[2];
    result = i2c_read_blocking(i2c, ADS1115_ADDR, data, 2, false);
    if (result < 0) {
        printf("I2C read failed: %d\n", result);
        return 0;
    }

    return (int16_t)((data[0] << 8) | data[1]);
}

// int main() {
//     stdio_init_all();
//     sleep_ms(2000);  // Wait for serial to initialize
//     printf("Hello.\n");
//     // Setup I2C on GPIO 3 (SDA) and GPIO 4 (SCL)
//     i2c_init(i2c0, 100000);
//     gpio_set_function(4, GPIO_FUNC_I2C);
//     gpio_set_function(5, GPIO_FUNC_I2C);
//     gpio_pull_up(4);
//     gpio_pull_up(5);

//     sleep_ms(1000);

//     // Scan I2C bus
//     printf("Scanning I2C bus...\n");
//     bool found = false;
//     for (uint8_t addr = 0x08; addr < 0x78; addr++) {
//         uint8_t test_data = 0;
//         if (i2c_read_blocking(i2c0, addr, &test_data, 1, false) !=
// PICO_ERROR_GENERIC) {
//             printf("  Found device at 0x%02X\n", addr);
//             found = true;
//         }
//     }
//     if (!found) {
//         printf("No I2C devices found! Check wiring.\n");
//         sleep_ms(500);
//         return 1;
//     }

//     printf("\nADS1115 Ready. Using A0-A1 differential mode with 1.65V ref on A1.\n");

//     int8_t direction = 1;

//     while (true) {
//         int16_t adc_raw = ads1115_read_differential(i2c0);
//         float voltage = (adc_raw * 2.048f) / 32768.0f;
//         voltage += 1.65625f;
//         float current = voltage * 78.30445f;

//         int16_t adc_a2 = ads1115_read_a2(i2c0);
//         float voltage_a2 = (adc_a2 * 2.048f) / 32768.0f;
//         voltage_a2 *= 18.94141f;

//         // Print at 10Hz
//         printf("Amps: %.3fA [ADC: %d (%.3fV)] | Volts: %.3fV (ADC: %d)\n", current, adc_raw, voltage, voltage_a2, adc_a2);
//         sleep_ms(80);  // 10Hz
//     }

//     return 0;
// }
