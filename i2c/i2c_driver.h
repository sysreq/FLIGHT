#pragma once

#include "i2c_config.h"

namespace i2c::drivers {

// Utility functions for byte merging
namespace utils {
    // Merge 2 bytes (MSB first)
    template<typename T>
    inline T merge_bytes(uint8_t high, uint8_t low) {
        return static_cast<T>((static_cast<uint16_t>(high) << 8) | low);
    }
    
    // Merge 3 bytes (MSB first)
    template<typename T>
    inline T merge_bytes(uint8_t b2, uint8_t b1, uint8_t b0) {
        return static_cast<T>((static_cast<uint32_t>(b2) << 16) | 
                             (static_cast<uint32_t>(b1) << 8) | 
                             static_cast<uint32_t>(b0));
    }
    
    // Merge 4 bytes (MSB first)
    template<typename T>
    inline T merge_bytes(uint8_t b3, uint8_t b2, uint8_t b1, uint8_t b0) {
        return static_cast<T>((static_cast<uint32_t>(b3) << 24) |
                             (static_cast<uint32_t>(b2) << 16) | 
                             (static_cast<uint32_t>(b1) << 8) | 
                             static_cast<uint32_t>(b0));
    }
}

// Base class template using CRTP for static polymorphism
template<typename Derived>
class I2CDriverBase {
protected:
    // Use DeviceTraits for address and name
    using Traits = i2c::DeviceTraits<Derived>;
    
    i2c_inst_t* i2c_instance;
    bool initialized;
    
    // Common I2C operations
    bool write_register(uint8_t reg, uint8_t value) {
        uint8_t buffer[2] = {reg, value};
        int result = i2c_write_blocking(i2c_instance, Traits::address, buffer, 2, false);
        return result == 2;
    }
    
    bool write_registers(uint8_t reg, const uint8_t* data, size_t len) {
        // Create buffer with register address followed by data
        uint8_t buffer[len + 1];
        buffer[0] = reg;
        memcpy(&buffer[1], data, len);
        int result = i2c_write_blocking(i2c_instance, Traits::address, buffer, len + 1, false);
        return result == static_cast<int>(len + 1);
    }
    
    bool read_register(uint8_t reg, uint8_t* value) {
        return read_registers(reg, value, 1);
    }
    
    bool read_registers(uint8_t reg, uint8_t* buffer, size_t len) {
        int result = i2c_write_blocking(i2c_instance, Traits::address, &reg, 1, true);
        if (result != 1) return false;
        
        result = i2c_read_blocking(i2c_instance, Traits::address, buffer, len, false);
        return result == static_cast<int>(len);
    }
    
    // Check if device is present on the bus
    bool device_present() {
        uint8_t dummy;
        int result = i2c_read_blocking(i2c_instance, Traits::address, &dummy, 1, false);
        return result >= 0;
    }
    
    // Verify chip ID helper
    bool verify_chip_id(uint8_t id_register, uint8_t expected_id, const char* error_context = "chip ID") {
        uint8_t chip_id;
        if (!read_register(id_register, &chip_id)) {
            printf("%s: Failed to read %s\n", Traits::name, error_context);
            return false;
        }
        
        if (chip_id != expected_id) {
            printf("%s: Wrong %s: 0x%02X (expected 0x%02X)\n", 
                   Traits::name, error_context, chip_id, expected_id);
            return false;
        }
        return true;
    }
    
    // Soft reset helper (many sensors use 0xB6 as reset command)
    bool soft_reset(uint8_t cmd_register, uint8_t reset_cmd = 0xB6, uint32_t delay_ms = 10) {
        if (!write_register(cmd_register, reset_cmd)) {
            printf("%s: Failed to send reset command\n", Traits::name);
            return false;
        }
        sleep_ms(delay_ms);
        return true;
    }
    
    // Log successful initialization
    void log_init_success() {
        printf("%s: Initialized successfully (address: 0x%02X)\n", 
               Traits::name, Traits::address);
    }

public:
    I2CDriverBase() : i2c_instance(nullptr), initialized(false) {}
    
    bool is_initialized() const { return initialized; }
};

} // namespace i2c::drivers