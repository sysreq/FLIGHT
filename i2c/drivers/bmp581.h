#pragma once

#include "i2c/i2c_driver.h"

namespace i2c::drivers {

class BMP581 : public I2CDriverBase<BMP581> {
private:
    static constexpr uint8_t BMP581_REG_CHIP_ID      = 0x01;
    static constexpr uint8_t BMP581_REG_CHIP_STATUS  = 0x11;
    static constexpr uint8_t BMP581_REG_TEMP_DATA    = 0x1D; 
    static constexpr uint8_t BMP581_REG_PRESS_DATA   = 0x20;
    static constexpr uint8_t BMP581_REG_INT_STATUS   = 0x27;
    static constexpr uint8_t BMP581_REG_STATUS       = 0x28;
    static constexpr uint8_t BMP581_REG_PWR_CTRL     = 0x33;
    static constexpr uint8_t BMP581_REG_OSR_CONFIG   = 0x36;
    static constexpr uint8_t BMP581_REG_ODR_CONFIG   = 0x37;
    static constexpr uint8_t BMP581_REG_OSR_EFF      = 0x38;
    static constexpr uint8_t BMP581_REG_CMD          = 0x7E;

    static constexpr uint8_t EXPECTED_CHIP_ID = 0x50; 
    static constexpr uint8_t RESET_COMMAND = 0xB6;
    static constexpr float SEA_LEVEL_PRESSURE = 101325.0f;
    
    bmp581_data data;
    
    float calculate_altitude(float pressure) {
        return 44330.0f * (1.0f - powf(pressure / SEA_LEVEL_PRESSURE, 0.1903f));
    }

public:
    BMP581() : I2CDriverBase() {
        data.valid = false;
    }
    
    bool init(i2c_inst_t* instance) {
        i2c_instance = instance;
        
        if (!device_present()) {
            printf("%s: Device not found at address 0x%02X\n", 
                   Traits::name, Traits::address);
            return false;
        }
        
        if (!verify_chip_id(BMP581_REG_CHIP_ID, EXPECTED_CHIP_ID)) {
            return false;
        }
        
        if (!soft_reset(BMP581_REG_CMD, RESET_COMMAND, 10)) {
            return false;
        }
        
        // OSR register: x2 oversampling for temperature and pressure, enable pressure
        // Bit 6 = pressure enable, Bits [5:3] = temp OSR, [2:0] = press OSR, 001 = x2
        if (!write_register(BMP581_REG_OSR_CONFIG, 0x49)) {  // 0x40 | 0x09
            printf("%s: Failed to configure oversampling\n", Traits::name);
            return false;
        }
        
        // ODR register: 50Hz output data rate (normal/continuous mode)
        // 0xBD = 0x80 | 0x3C | 0x01 (Deep disable + 50Hz + Normal mode)
        if (!write_register(BMP581_REG_ODR_CONFIG, 0xBD)) {
            printf("%s: Failed to configure output data rate\n", Traits::name);
            return false;
        }
        
        sleep_ms(50);
        
        initialized = true;
        data.valid = false;
        
        log_init_success();
        return true;
    }
    
    bool update() {
        if (!initialized) {
            return false;
        }
        
        uint8_t temp_data[3];
        if (!read_registers(BMP581_REG_TEMP_DATA, temp_data, 3)) {
            return false;
        }
        
        uint8_t press_data[3];
        if (!read_registers(BMP581_REG_PRESS_DATA, press_data, 3)) {
            return false;
        }
        
        int32_t raw_temp = utils::merge_bytes<int32_t>(temp_data[2], temp_data[1], temp_data[0]);
        data.temperature = raw_temp / 65536.0f;
        
        uint32_t raw_press = utils::merge_bytes<uint32_t>(press_data[2], press_data[1], press_data[0]);
        data.pressure = raw_press / 64.0f;
        
        data.altitude = calculate_altitude(data.pressure);
        
        data.valid = true;
        
        return true;
    }
    
    bmp581_data get_data() {
        return data;
    }
};

} // namespace i2c::drivers