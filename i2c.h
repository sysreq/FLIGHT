#pragma once

/**
 * @file i2c.h
 * @brief Complete I2C system with automatic polling and timer management
 * 
 * This is the single include file that provides the complete I2C abstraction
 * system for Pi Pico 2-based telemetry systems.
 * 
 * Key Features:
 * - Template-based device management with compile-time safety
 * - Automatic polling with hardware timers and callbacks
 * - Manual device access for on-demand operations
 * - Clean separation of concerns between bus and devices
 * 
 * Example Usage:
 * 
 *   // Define your bus type
 *   using MyI2CBus = i2c::I2CBus<i2c0, 4, 5>; // SDA=4, SCL=5
 * 
 *   // Initialize and configure
 *   MyI2CBus::start();
 *   MyI2CBus::add_device<drivers::ICM20948>(handle_imu_data);  // Auto-polls at 100Hz when enabled
 *   MyI2CBus::add_device<drivers::BME280>(handle_env_data);    // Auto-polls at 1Hz when enabled
 *   MyI2CBus::add_device<drivers::ADS1115>();                   // Manual only (no handler)
 *   
 *   // Enable bus - devices with handlers automatically start polling at default rates!
 *   MyI2CBus::enable(); // ICM20948 @ 100Hz and BME280 @ 1Hz start automatically
 * 
 *   // Manual device access in main loop
 *   auto& adc = MyI2CBus::get_device<drivers::ADS1115>();
 *   if (adc.update()) {
 *       // Process data manually
 *   }
 */

#include "hardware/i2c.h"
#include "pico/time.h"
#include "hardware/timer.h"
#include "hardware/gpio.h"

#include "i2c/i2c_config.h"
#include "i2c/i2c_concepts.h" 
#include "i2c/i2c_device.h"
#include "i2c/i2c_bus.h"

#include "i2c/drivers/icm20948.h"
#include "i2c/drivers/bmp581.h"
#include "i2c/drivers/ms4525d0.h"