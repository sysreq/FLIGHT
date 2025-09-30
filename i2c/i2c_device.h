#pragma once

#include "i2c_concepts.h"

namespace i2c {

template<Device DeviceType>
class I2CDevice {
private:
    DeviceType device;
    repeating_timer_t timer{};
    std::function<void(const typename DeviceTraits<DeviceType>::data_type&)> callback;
    uint32_t poll_rate_hz = DeviceTraits<DeviceType>::default_poll_rate;
    uint32_t error_count = 0;
    bool initialized = false;
    bool polling_active = false;
    
    static bool timer_callback(repeating_timer_t* rt) {
        auto* self = static_cast<I2CDevice*>(rt->user_data);
        return self->handle_timer();
    }
    
    bool handle_timer() {
        if (device.update()) {
            error_count = 0;
            if (callback) {
                callback(device.get_data());
            }
        } else {
            error_count++;
            if (error_count > MAX_ERRORS) {
                printf("%s: Too many errors (%u), stopping timer\n", 
                       DeviceTraits<DeviceType>::name, error_count);
                polling_active = false;
                return false;
            }
        }
        return true;
    }

public:
    I2CDevice() {
        memset(&timer, 0, sizeof(repeating_timer_t));
    }
    
    ~I2CDevice() {
        stop_polling();
    }
    
    bool init(i2c_inst_t* instance) {
        initialized = device.init(instance);
        return initialized;
    }
    
    void set_callback(std::function<void(const typename DeviceTraits<DeviceType>::data_type&)> cb) {
        callback = std::move(cb);
    }
    
    void set_poll_rate(uint32_t rate_hz) {
        bool was_polling = polling_active;
        if (was_polling) {
            stop_polling();
        }
        
        poll_rate_hz = rate_hz;
        
        if (was_polling) {
            start_polling();
        }
    }
    
    bool start_polling() {
        if (!initialized || polling_active) {
            return polling_active;
        }
        
        int64_t interval_us = -static_cast<int64_t>(1000000 / poll_rate_hz);
        polling_active = add_repeating_timer_us(interval_us, timer_callback, this, &timer);
        
        if (polling_active) {
            printf("%s: Started polling at %u Hz\n", 
                   DeviceTraits<DeviceType>::name, poll_rate_hz);
        }
        
        return polling_active;
    }
    
    void stop_polling() {
        if (polling_active) {
            cancel_repeating_timer(&timer);
            polling_active = false;
            printf("%s: Stopped polling\n", DeviceTraits<DeviceType>::name);
        }
    }
    
    // Direct access to underlying device
    DeviceType& get() { 
        return device; 
    }
    
    // Manual update
    bool update() { 
        return device.update(); 
    }
    
    // Get latest data
    typename DeviceTraits<DeviceType>::data_type get_data() { 
        return device.get_data(); 
    }
    
    // Status
    bool has_callback() const { 
        return callback != nullptr; 
    }
    
    bool is_polling() const { 
        return polling_active; 
    }
    
    uint32_t get_error_count() const { 
        return error_count; 
    }
    
    void reset_error_count() { 
        error_count = 0; 
    }
};

} // namespace i2c