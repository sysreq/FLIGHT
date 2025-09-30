#pragma once

#include "i2c/i2c_driver.h"

namespace i2c::drivers {

class ICM20948 : public I2CDriverBase<ICM20948> {
private:
    static constexpr uint8_t REG_WHO_AM_I = 0x00;
    static constexpr uint8_t REG_USER_CTRL = 0x03;
    static constexpr uint8_t REG_PWR_MGMT_1 = 0x06;
    static constexpr uint8_t REG_PWR_MGMT_2 = 0x07;
    static constexpr uint8_t REG_GYRO_CONFIG_1 = 0x01;
    static constexpr uint8_t REG_ACCEL_CONFIG = 0x14;
    static constexpr uint8_t REG_ACCEL_CONFIG_2 = 0x15;
    static constexpr uint8_t REG_ACCEL_XOUT_H = 0x2D;
    static constexpr uint8_t REG_GYRO_XOUT_H = 0x33;
    static constexpr uint8_t REG_BANK_SEL = 0x7F;

    static constexpr uint8_t EXPECTED_CHIP_ID = 0xEA;
    static constexpr uint8_t ACCEL_RANGE = 2;
    static constexpr uint8_t GYRO_RANGE = 2;
    static constexpr float ACCEL_SCALE = 8.0f * 9.81f / 32768.0f;
    static constexpr float GYRO_SCALE = 1000.0f * 0.01745329f / 32768.0f;

    icm20948_data data;
    uint8_t current_bank;
    
    bool select_bank(uint8_t bank) {
        if (current_bank == bank) {
            return true;
        }
        
        if (write_register(REG_BANK_SEL, (bank & 0x03) << 4)) {
            current_bank = bank;
            return true;
        }
        return false;
    }

public:
    ICM20948() : I2CDriverBase(), current_bank(0xFF) {
        data.valid = false;
    }
    
    bool init(i2c_inst_t* instance) {
        i2c_instance = instance;
        
        // Select bank 0
        if (!select_bank(0)) {
            printf("%s: Failed to select bank 0\n", Traits::name);
            return false;
        }
        
        // Check chip ID
        if (!verify_chip_id(REG_WHO_AM_I, EXPECTED_CHIP_ID)) {
            return false;
        }
        
        // Reset device
        if (!write_register(REG_PWR_MGMT_1, 0x80)) {
            printf("%s: Failed to reset device\n", Traits::name);
            return false;
        }
        sleep_ms(100);
        
        if (!write_register(REG_PWR_MGMT_1, 0x01)) {
            printf("%s: Failed to wake device\n", Traits::name);
            return false;
        }
        sleep_ms(20);
        
        if (!write_register(REG_PWR_MGMT_2, 0x00)) {
            printf("%s: Failed to enable sensors\n", Traits::name);
            return false;
        }
        
        if (!select_bank(2)) {
            printf("%s: Failed to select bank 2\n", Traits::name);
            return false;
        }
        
        if (!write_register(REG_ACCEL_CONFIG, ACCEL_RANGE << 1)) {
            printf("%s: Failed to configure accelerometer\n", Traits::name);
            return false;
        }
        
        if (!write_register(REG_GYRO_CONFIG_1, GYRO_RANGE << 1)) {
            printf("%s: Failed to configure gyroscope\n", Traits::name);
            return false;
        }
        
        if (!select_bank(0)) {
            printf("%s: Failed to return to bank 0\n", Traits::name);
            return false;
        }
        
        initialized = true;
        data.valid = false;
        
        log_init_success();
        return true;
    }
    
    bool update() {
        if (!initialized) {
            return false;
        }
        
        // Only select bank 0 if not already there
        if (current_bank != 0) {
            if (!select_bank(0)) {
                return false;
            }
        }
        
        // Read accel + gyro data (12 bytes)
        uint8_t raw_data[12];
        if (!read_registers(REG_ACCEL_XOUT_H, raw_data, 12)) {
            return false;
        }
        
        // Parse accelerometer data (bytes 0-5)
        int16_t accel_x_raw = utils::merge_bytes<int16_t>(raw_data[0], raw_data[1]);
        int16_t accel_y_raw = utils::merge_bytes<int16_t>(raw_data[2], raw_data[3]);
        int16_t accel_z_raw = utils::merge_bytes<int16_t>(raw_data[4], raw_data[5]);
        
        // Parse gyroscope data (bytes 6-11)
        int16_t gyro_x_raw = utils::merge_bytes<int16_t>(raw_data[6], raw_data[7]);
        int16_t gyro_y_raw = utils::merge_bytes<int16_t>(raw_data[8], raw_data[9]);
        int16_t gyro_z_raw = utils::merge_bytes<int16_t>(raw_data[10], raw_data[11]);
        
        // Convert to SI units using compile-time scale factors
        data.accel_x = accel_x_raw * ACCEL_SCALE;
        data.accel_y = accel_y_raw * ACCEL_SCALE;
        data.accel_z = accel_z_raw * ACCEL_SCALE;
        
        data.gyro_x = gyro_x_raw * GYRO_SCALE;
        data.gyro_y = gyro_y_raw * GYRO_SCALE;
        data.gyro_z = gyro_z_raw * GYRO_SCALE;
        
        data.valid = true;
        
        return true;
    }
    
    icm20948_data get_data() {
        return data;
    }
};

} // namespace i2c::drivers