#pragma once

#include "i2c_config.h"
#include "i2c_device.h"

namespace i2c {

template<i2c_inst_t* Instance, uint SDA, uint SCL, uint32_t Baudrate = DEFAULT_BUS_SPEED>
class I2CBus {
private:
    static inline std::array<uint8_t, MAX_DEVICES> registered_addresses{};
    static inline std::array<std::function<void()>, MAX_DEVICES> start_functions{};
    static inline std::array<std::function<void()>, MAX_DEVICES> stop_functions{};
    static inline size_t device_count = 0;
    static inline bool initialized = false;
    static inline bool enabled = false;

    static constexpr uint32_t SDA_PIN = SDA;
    static constexpr uint32_t SCL_PIN = SCL;
    static constexpr uint32_t BUS_BAUDRATE = Baudrate;
    
    // Static storage for device instances
    template<Device DeviceType>
    static I2CDevice<DeviceType>& get_device_instance() {
        static I2CDevice<DeviceType> instance;
        return instance;
    }

    static bool bus_scan() {
        printf("\nI2C Bus Scan\n");
            printf("   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");

            for (int addr = 0x20; addr < (1 << 7); ++addr) {
                if (addr % 16 == 0) {
                    printf("%02x ", addr);
                }

                int ret;
                uint8_t rxdata;
                ret = i2c_read_blocking(Instance, addr, &rxdata, 1, false);

                printf(ret < 0 ? "." : "@");
                printf(addr % 16 == 15 ? "\n" : "  ");
            }
            printf("Done.\n");

            return true;
    }
    
    static bool is_address_conflict(uint8_t addr) {
        for (size_t i = 0; i < device_count; ++i) {
            if (registered_addresses[i] == addr) {
                return true;
            }
        }
        return false;
    }
    
    template<Device DeviceType>
    static void register_device_functions() {
        size_t index = device_count;
        
        start_functions[index] = []() {
            auto& device = get_device_instance<DeviceType>();
            if (device.has_callback()) {
                device.start_polling();
            }
        };
        
        stop_functions[index] = []() {
            get_device_instance<DeviceType>().stop_polling();
        };
    }

public:
    static bool start() {
        if (initialized) {
            return true;
        }
        
        // Initialize I2C hardware
        i2c_init(Instance, Baudrate);
        
        // Configure GPIO pins
        gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
        gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);
        gpio_pull_up(SDA_PIN);
        gpio_pull_up(SCL_PIN);

        // Initialize tracking arrays
        registered_addresses.fill(0xFF);
        start_functions.fill(nullptr);
        stop_functions.fill(nullptr);
        device_count = 0;
        
        initialized = true;
        printf("I2C Bus: Initialized on pins SDA=%u, SCL=%u at %u Hz\n", 
               SDA_PIN, SCL, Baudrate);

        bus_scan();
        return true;
    }

    static void shutdown() {
        if (initialized) {
            disable();
            
            i2c_deinit(Instance);
            
            device_count = 0;
            registered_addresses.fill(0xFF);
            start_functions.fill(nullptr);
            stop_functions.fill(nullptr);
            initialized = false;
            
            printf("I2C Bus: Shutdown complete\n");
        }
    }
    
    template<Device DeviceType>
    static bool add_device(std::function<void(const typename DeviceTraits<DeviceType>::data_type&)> handler) {
        constexpr uint8_t addr = DeviceTraits<DeviceType>::address;
        constexpr const char* name = DeviceTraits<DeviceType>::name;
        
        if (is_address_conflict(addr)) {
            printf("I2C Bus: Address conflict! Device %s cannot use address 0x%02X\n", 
                   name, addr);
            return false;
        }
        
        if (device_count >= MAX_DEVICES) {
            printf("I2C Bus: Maximum device limit (%zu) reached\n", MAX_DEVICES);
            return false;
        }
        
        auto& device = get_device_instance<DeviceType>();
        
        if (!device.init(Instance)) {
            printf("I2C Bus: Failed to initialize device %s\n", name);
            return false;
        }
        
        device.set_callback(std::move(handler));
        
        register_device_functions<DeviceType>();
        registered_addresses[device_count] = addr;
        device_count++;
        
        if (enabled) {
            device.start_polling();
        }
        
        printf("I2C Bus: Added device %s at address 0x%02X\n", name, addr);
        return true;
    }
    
    template<Device DeviceType>
    static bool add_device() {
        constexpr uint8_t addr = DeviceTraits<DeviceType>::address;
        constexpr const char* name = DeviceTraits<DeviceType>::name;
        
        if (is_address_conflict(addr)) {
            printf("I2C Bus: Address conflict! Device %s cannot use address 0x%02X\n", 
                   name, addr);
            return false;
        }
        
        if (device_count >= MAX_DEVICES) {
            printf("I2C Bus: Maximum device limit (%zu) reached\n", MAX_DEVICES);
            return false;
        }
        
        auto& device = get_device_instance<DeviceType>();
        
        if (!device.init(Instance)) {
            printf("I2C Bus: Failed to initialize device %s\n", name);
            return false;
        }
        
        register_device_functions<DeviceType>();
        registered_addresses[device_count] = addr;
        device_count++;
        
        printf("I2C Bus: Added device %s at address 0x%02X (manual mode)\n", name, addr);
        return true;
    }
    
    static void enable() {
        enabled = true;
        
        for (size_t i = 0; i < device_count; ++i) {
            if (start_functions[i]) {
                start_functions[i]();
                sleep_ms(1);
            }
        }
        
        printf("I2C Bus: Enabled (%zu devices registered)\n", device_count);
    }
    
    static void disable() {
        enabled = false;
        
        for (size_t i = 0; i < device_count; ++i) {
            if (stop_functions[i]) {
                stop_functions[i]();
            }
        }
        
        printf("I2C Bus: Disabled\n");
    }
    
    template<Device DeviceType>
    static DeviceType& get_device() {
        return get_device_instance<DeviceType>().get();
    }
    
    template<Device DeviceType>
    static bool poll_default_rate() {
        if (!enabled) {
            printf("I2C Bus: Bus not enabled, call enable() first\n");
            return false;
        }
        return get_device_instance<DeviceType>().start_polling();
    }
    
    template<Device DeviceType>
    static bool poll_rate(uint32_t rate_hz) {
        if (!enabled) {
            printf("I2C Bus: Bus not enabled, call enable() first\n");
            return false;
        }
        auto& device = get_device_instance<DeviceType>();
        device.set_poll_rate(rate_hz);
        return device.start_polling();
    }
    
    template<Device DeviceType>
    static void stop_polling() {
        get_device_instance<DeviceType>().stop_polling();
    }
    
    template<Device DeviceType>
    static void set_handler(std::function<void(const typename DeviceTraits<DeviceType>::data_type&)> handler) {
        get_device_instance<DeviceType>().set_callback(std::move(handler));
    }
    
    template<Device DeviceType>
    static uint32_t get_error_count() {
        return get_device_instance<DeviceType>().get_error_count();
    }
    
    template<Device DeviceType>
    static void reset_error_count() {
        get_device_instance<DeviceType>().reset_error_count();
    }
    
    template<Device DeviceType>
    static bool is_polling() {
        return get_device_instance<DeviceType>().is_polling();
    }
    
    static bool is_initialized() { return initialized; }
    static bool is_enabled() { return enabled; }
    static size_t get_device_count() { return device_count; }

private:
    I2CBus() = delete;
    I2CBus(const I2CBus&) = delete;
    I2CBus& operator=(const I2CBus&) = delete;
};

} // namespace i2c