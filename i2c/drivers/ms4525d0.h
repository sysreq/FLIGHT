#pragma once

#include "i2c/i2c_driver.h"

namespace i2c::drivers {

class MS4525D0 : public I2CDriverBase<MS4525D0> {
private:
    // MS4525DO-DS5A1 (±5 inches H2O variant)
    static constexpr float P_MIN = -5.0f;              // -5 inH2O
    static constexpr float P_MAX = 5.0f;               // +5 inH2O
    static constexpr float INH2O_TO_PA = 249.089f;     // Conversion factor
    
    // Transfer function constants (14-bit ADC)
    static constexpr float OUTPUT_MIN = 0.1f * 16383.0f;   // 10% of 2^14-1
    static constexpr float OUTPUT_SPAN = 0.8f * 16383.0f;  // 80% of 2^14-1
    
    // Temperature constants (11-bit)
    static constexpr float TEMP_SCALE = 200.0f / 2047.0f;
    static constexpr float TEMP_OFFSET = -50.0f;
    
    // Status bits in first byte
    static constexpr uint8_t STATUS_MASK = 0xC0;
    static constexpr uint8_t STATUS_NORMAL = 0x00;
    static constexpr uint8_t STATUS_STALE = 0x80;
    static constexpr uint8_t STATUS_FAULT = 0xC0;
    
    ms4525d0_data data;
    
    float calculate_pressure(uint16_t raw) {
        float diff_press_inH2O = ((raw - OUTPUT_MIN) / OUTPUT_SPAN) * (P_MAX - P_MIN) + P_MIN;
        return diff_press_inH2O * INH2O_TO_PA;
    }
    
    float calculate_temperature(uint16_t raw) {
        uint16_t temp_raw = raw >> 5;
        return (temp_raw * TEMP_SCALE) + TEMP_OFFSET;
    }

public:
    MS4525D0() : I2CDriverBase() {
        data.valid = false;
    }
    
    bool init(i2c_inst_t* instance) {
        i2c_instance = instance;
        
        if (!device_present()) {
            printf("%s: Device not found at address 0x%02X\n", 
                   Traits::name, Traits::address);
            return false;
        }
        
        uint8_t test_data[4];
        if (!read_measurement(test_data)) {
            printf("%s: Failed to read initial measurement\n", Traits::name);
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
        
        uint8_t raw_data[4];
        if (!read_measurement(raw_data)) {
            data.valid = false;
            return false;
        }
        
        // Check status bits
        uint8_t status = raw_data[0] & STATUS_MASK;
        if (status == STATUS_FAULT) {
            data.valid = false;
            return false;
        }
        
        // Extract 14-bit pressure value (first byte status bits masked, plus second byte)
        uint16_t pressure_raw = ((raw_data[0] & 0x3F) << 8) | raw_data[1];
        
        // Extract 11-bit temperature value (last 5 bits of byte 2, plus byte 3)
        uint16_t temp_raw = (raw_data[2] << 8) | raw_data[3];
        
        // Calculate values
        data.pressure_pa = calculate_pressure(pressure_raw);
        data.temperature_c = calculate_temperature(temp_raw);
        data.valid = true;
        
        return true;
    }
    
    ms4525d0_data get_data() {
        return data;
    }
    
private:
    bool read_measurement(uint8_t* buffer) {
        int result = i2c_read_blocking(i2c_instance, Traits::address, buffer, 4, false);
        return result == 4;
    }
};

} // namespace i2c::drivers