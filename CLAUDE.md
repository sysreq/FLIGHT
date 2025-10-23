# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a Raspberry Pi Pico 2 W (RP2350) embedded firmware project called "Forward Module". It's designed to run sensor data collection and processing across both cores of the Pico 2, with communication handled through a custom UART messaging library called FTL (Faster Than Light).

**Target Board**: `pico2_w` (Raspberry Pi Pico 2 W)
**Language**: C++23 with C17 for C sources
**SDK**: Pico SDK 2.2.0
**Toolchain**: ARM GCC 14_2_Rel1

## Build Commands

```bash
# Initial CMake configuration (from project root)
cmake -B build

# Build the project
cmake --build build

# Clean build
cmake --build build --target clean

# Rebuild everything
cmake --build build --clean-first

# Generate messages from YAML schema (happens automatically during build)
cmake --build build --target ftl_generate_messages
```

**Build outputs** are in `build/`:
- `FORWARD_MODULE.uf2` - Main firmware file for flashing
- `FORWARD_MODULE.elf` - ELF binary
- `FORWARD_MODULE.map` - Linker memory map

**Flashing**: Copy the `.uf2` file to the Pico 2 when in BOOTSEL mode.

## Architecture

### Dual-Core Design

The application uses both RP2350 cores with a custom controller pattern:

- **Core 0** (`Core0Controller`): Primary control core, currently minimal implementation
- **Core 1** (`Core1Controller`): Handles sensor devices (HX711 load cell, ADS1115 ADC)

Both cores inherit from `SystemCore<Derived, MaxTasks>`, a CRTP-based template that provides:
- Task scheduling with configurable intervals via `add_task(func_ptr, interval_ms)`
- Lifecycle management: `init_impl()`, `loop_impl()`, `shutdown_impl()`
- Global shutdown coordination through atomic flag `app::SystemCore::Global::system_active_`

**Entry point**: `main.cpp` initializes Core 0, launches Core 1 via `multicore_launch_core1()`, then both enter their respective loops.

### FTL Message Library (`ftl/`)

A reusable, DMA-accelerated UART messaging system with compile-time type safety:

**Key Features**:
- Zero-copy message handling with reference-counted pool allocation
- Protocol: `[0xAACC][LENGTH][SOURCE_ID][PAYLOAD][CRC16][0xDEFA]`
- Max message size: 256 bytes, max payload: 248 bytes
- DMA circular buffer RX (1024 bytes) with 64-byte chunk transfers
- Code generation from YAML schema via `generate_messages.py`

**Configuration**: `ftl/config.settings` contains:
- UART settings (UART0, 230400 baud, GP0/GP1 pins)
- Protocol constants (delimiters, sizes)
- DMA buffer sizes and message pool configuration

**Message Schema**: `ftl/messages.yaml` defines message types. Running the Python generator creates:
- `ftl/generated/messages_detail.h` - Individual message class headers
- `ftl/generated/messages.h` - Unified header with Dispatcher
- `ftl/generated/messages.cpp` - Implementation

**API Pattern**:
```cpp
#include "ftl/ftl.h"

ftl::initialize();
ftl::messages::Dispatcher dispatcher;

// Set handler
dispatcher.set_handler([](const ftl::messages::MSG_SENSOR_DATA_View& msg) {
    printf("Temp: %.1f\n", msg.temperature());
});

// Main loop
ftl::poll();
while (ftl::has_msg()) {
    auto msg = ftl::get_msg();
    dispatcher.dispatch(msg);
}

// Send message
dispatcher.send<ftl::messages::MSG_HEARTBEAT>(counter, status, "Device");
```

**Transport Layer**:
- `ftl/transport/uart/dma_control.cpp` - DMA channel setup and management
- `ftl/transport/uart/uart_rx.cpp` - Receive with frame detection and CRC validation
- `ftl/transport/uart/uart_tx.cpp` - Transmit with framing

**Utilities**:
- `ftl/util/allocator.h` - Fixed-size message pool with reference counting
- `ftl/util/cqueue.h` - Circular queue for received messages
- `ftl/util/crc16.h` - CRC16 calculation
- `ftl/util/device_id.h` - Unique device ID from Pico flash

### Device Drivers (`devices/`)

Standalone sensor driver library built as `StandaloneDevices`:

- **HX711** (`hx711_s.cpp/h`): 24-bit ADC for load cells with software bit-banging protocol
- **ADS1115** (`ads1115_s.cpp/h`): 16-bit I2C ADC with polling mode and callback support

Both drivers follow a pattern: `init()`, `update()/start_polling()`, and provide data through getters or callbacks.

### Application Layer (`app/`)

- **SystemCore.h**: CRTP base template for core controllers with task scheduling
- **Scheduler.h**: Supplementary scheduler utilities (if needed beyond SystemCore)
- **Core0Controller.h**: Core 0 implementation (currently minimal)
- **Core1Controller.h**: Core 1 implementation - manages HX711 and ADS1115, polls HX711 every 50ms

## Code Organization

```
ForwardModule/
├── main.cpp              # Entry point, dual-core startup
├── CMakeLists.txt        # Root build config, pico_bundle library
├── app/                  # Application controllers
├── devices/              # Sensor drivers (StandaloneDevices lib)
│   ├── CMakeLists.txt
│   ├── include/          # Public headers
│   └── misc/             # Utility headers
├── ftl/                  # FTL messaging library
│   ├── CMakeLists.txt    # Code generation + library build
│   ├── config.settings   # UART/protocol configuration
│   ├── messages.yaml     # Message schema (edit this!)
│   ├── generate_messages.py # Code generator script
│   ├── templates/        # Jinja2 templates for codegen
│   ├── generated/        # AUTO-GENERATED (don't edit directly)
│   ├── core/            # ftl_api.h/cpp
│   ├── transport/uart/  # DMA UART implementation
│   └── util/            # Allocator, CRC, queue, device ID
└── build/               # Build artifacts (gitignored)
```

## Development Workflow

### Adding New Message Types

1. Edit `ftl/messages.yaml` to add message definition
2. Build project - CMake automatically runs `generate_messages.py`
3. Include `"ftl/ftl.h"` and use new message types in dispatcher

### Adding New Sensors/Devices

1. Create driver in `devices/` (`.cpp` and header in `include/`)
2. Add source file to `devices/CMakeLists.txt` in `add_library(StandaloneDevices ...)`
3. Instantiate device in appropriate Core controller
4. Initialize in `init_impl()`, poll in `loop_impl()` or via `add_task()`

### Working with Dual Cores

- Core 0 and Core 1 run independently after launch
- Use FTL messages or shared memory (with proper synchronization) for inter-core communication
- Call `SystemCore::shutdown()` to signal both cores to exit gracefully
- Core 1 automatically exits when `system_active_` becomes false

## Important Notes

- **STDIO**: USB CDC is enabled (`pico_enable_stdio_usb`), UART stdio is disabled
- **CMake pico_bundle**: A convenience interface library bundles common Pico SDK components (stdlib, hardware interfaces, multicore, DMA, etc.)
- **Linker options**: Includes `--gc-sections` for dead code elimination and `--print-memory-usage`
- **Generated code**: Never manually edit files in `ftl/generated/` - always modify `messages.yaml` and rebuild
- **C++ Standard**: Requires C++23 for FTL library (uses modern features like `std::span`, concepts)
- **Task scheduling**: Use `add_task()` in Core controllers for periodic operations instead of manual timing
- **Shutdown sequence**: `main.cpp` resets USB and enters bootloader mode after clean shutdown

## Git Branch Strategy

- **Main branch**: `main`
- **Current work**: Development happens on feature branches (current: `FORWARD_MODULE`)
- Recent work includes UART DMA control, RX/TX handling, and pin configuration
