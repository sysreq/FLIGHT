# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

FLIGHT is a dual-core flight telemetry and data logging system for the Raspberry Pi Pico 2 W. It implements a real-time data acquisition system for aerospace applications with multi-core processing, sensor fusion, and wireless data access.

## Build System

The project uses CMake with the Pico SDK. Build configuration:
- **C Standard**: C17
- **C++ Standard**: C++23
- **Board**: `pico2_w`
- **SDK Version**: 2.2.0
- **Toolchain**: arm-none-eabi-gcc 14_2_Rel1

### Building

Since CMake is not available in PATH, building must be done through the Pico VS Code extension or by manually configuring the environment to use the Pico SDK toolchain at `~/.pico-sdk/`.

The build system expects:
1. Pico SDK at `~/.pico-sdk/` (configured via `pico-vscode.cmake`)
2. CMake generates to `build/` directory
3. Output: `FLIGHT.uf2` and related binaries

## Architecture

### Dual-Core Design

The system uses both cores of the RP2040/RP2350 processor with distinct responsibilities:

**Core 0** (app/core0.cpp):
- WiFi access point management
- HTTP/DHCP/DNS servers for wireless data access
- SD card file operations and logging
- Load cell (HX711) data acquisition
- Sensor data consumption from Core 1
- File sync and buffering

**Core 1** (app/core1.cpp):
- I2C sensor bus management
- Sensor polling (IMU, barometer, airspeed)
- Real-time data generation
- Command processing from Core 0

### Inter-Core Communication

Lock-free message passing between cores using `MessageChannel<T>` (common/channels.h):
- **SensorChannel**: Core 1 ’ Core 0 (sensor data)
- **CommandChannel**: Core 0 ’ Core 1 (start/stop/shutdown commands)

Template-based type-safe ring buffer with atomic operations. Messages support up to 122 bytes of payload data.

### Main Entry Point

`main.cpp` orchestrates initialization:
1. Initialize stdio (USB serial)
2. Init both cores sequentially
3. Launch Core 1 on separate processor
4. Run dual event loops
5. Coordinated shutdown

### I2C Sensor System

Template-based I2C bus abstraction (i2c/i2c_bus.h):
- **I2CBus<Instance, SDA, SCL, Baudrate>**: Hardware-agnostic bus configuration
- **I2CDevice<DeviceType>**: Per-device polling and callbacks
- **Device traits**: Compile-time device metadata (address, name, data type)

Supported sensors:
- ICM20948: 9-DOF IMU (accelerometer, gyroscope, magnetometer)
- BMP581: High-precision barometric altimeter
- MS4525D0: Differential pressure sensor for airspeed (16x oversampling)

Example bus configuration:
```cpp
using TelemetryBus = i2c::I2CBus<i2c0, 4, 5, 400000>;  // SDA=4, SCL=5, 400kHz
```

### SD Card Storage

Two storage systems are implemented:

**1. SD Card Layer** (sdcard.h, sdcard/):
- Template-based file management with compile-time config
- FatFs filesystem (lib/sdcard/src/ff15/)
- Buffered writes with manual sync control
- File types defined in sdcard/sd_config.h:
  - `LogFile`: System logs (1KB buffer)
  - `TelemetryFile`: IMU/sensor data (2KB buffer)
  - `SpeedFile`: Airspeed measurements
  - `HX711DataLog`: Load cell data

**2. Storage Layer** (storage.h, storage/):
- Binary data logging with double-buffering
- Lock-free buffer swaps between cores
- Structured data with typed headers (DataType enum)
- Direct SPI integration

Usage pattern:
```cpp
sdcard::SDCard::mount();
sdcard::SDFile<LogFile>::Open();
sdcard::SDFile<LogFile>::Write("Message: %s\n", data);
sdcard::SDFile<LogFile>::Sync();  // Periodic flush
```

### HTTP Access Point

WiFi access point (http/access_point.h) provides wireless data access:
- SSID: "FLIGHT-2.0" / Password: "airdevils"
- IP: 192.168.4.1
- Services: HTTP, DNS, DHCP
- Real-time sensor data via HTML interface
- Start/stop controls with timestamp capture

HTTP system:
- **http_server**: TCP connection handling
- **http_router**: URL routing to handlers
- **http_generator**: Dynamic HTML generation with live sensor data
- **http_events**: Event queue for start/stop commands with timestamps

### ADC Subsystem

Load cell interface (adc/hx711.h):
- 24-bit ADC via HX711 chip
- 50Hz update rate (20ms intervals)
- Tare and calibration support

## Configuration Files

- **app/app_config.h**: Timing constants, core contracts, message types
- **sdcard/sd_config.h**: File paths and buffer sizes
- **storage/storage_config.h**: Binary storage format and SPI config
- **http/config/http_config.h**: Network settings
- **http/config/lwipopts.h**: lwIP TCP/IP stack tuning

## Common Development Tasks

### Adding a New I2C Sensor

1. Create device driver in `i2c/drivers/` following the Device concept
2. Define DeviceTraits with address, name, data_type
3. Add to TelemetryBus in app/core1.cpp with callback
4. Handle data in app/core0.cpp sensor processing

### Adding a New Data File

1. Define file config struct in sdcard/sd_config.h
2. Instantiate SDFile<YourFileType> in core0.cpp
3. Open in core0::init()
4. Write with SDFile<YourFileType>::Write()
5. Sync periodically in sync_files()
6. Close in core0::shutdown()

### Modifying Message Types

1. Update enums in app/app_config.h (SensorTypes/CommandTypes)
2. Add handler cases in core processing functions
3. Ensure message size d 122 bytes

## Timing Constraints

- Load cell update: 20ms (50Hz)
- File sync rotation: 1 second per file
- MS4525D0 oversampling: 16 samples before publish
- WiFi polling: 50ms when using CYW43_ARCH_POLL

## Important Notes

- All sensor polling is disabled by default; started via HTTP "start" event
- File writes are buffered; manual sync required for durability
- Core 1 initialization happens on Core 0, then ownership transfers
- GPIO 4/5 reserved for I2C bus
- GPIO configuration for HX711 handled in driver init
