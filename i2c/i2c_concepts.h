#pragma once

#include "i2c_config.h"

namespace i2c {

template<typename T>
concept Device = requires(T device, i2c_inst_t* instance) {
    // Must have device traits defined
    requires requires { DeviceTraits<T>::address; };
    requires requires { DeviceTraits<T>::name; };
    requires requires { typename DeviceTraits<T>::data_type; };
    
    // Must implement the device interface
    { device.init(instance) } -> std::convertible_to<bool>;
    { device.update() } -> std::convertible_to<bool>;
    { device.get_data() } -> std::convertible_to<typename DeviceTraits<T>::data_type>;
};

} // namespace i2c