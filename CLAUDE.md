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

# Build the project (production firmware)
cmake --build build

# Build MOCK_MODULE for testing FTL without hardware
cmake --build build --target MOCK_MODULE

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
- `MOCK_MODULE` - Single-core test executable for FTL integration testing

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

### Inter-Core Communication

**Hardware FIFO for Message Passing**:
Core 1 cannot directly access UART/DMA resources (owned by Core 0). Instead, it uses the RP2350's hardware FIFO:

- **Core 1**: Calls `ftl::multicore::send_from_core1(payload)` - non-blocking, ultra-low latency
- **Core 0**: Processes queued messages via `ftl::multicore::process_core1_messages()` during `ftl::poll()`
- **Implementation**: `ftl/transport/uart/multicore_tx.h/cpp`
- **Benefits**: Single-cycle hardware access, no locks needed, Core 1 never blocks on UART

**Global State Synchronization**:
- `SystemState::globals_[]` provides atomic shared variables (defined in `app/SystemState.h`)
- Enum `GLOBAL_VAR` includes: `ACTIVE`, `HX711_POLLING`
- Example: `SystemCore::Global::system_active_` triggers graceful shutdown across both cores
- Pattern: `SystemState::globals_[GLOBAL_VAR::ACTIVE].store(value)`

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

**Message Pool & Reference Counting**:
- `ftl/util/allocator.h` - Fixed message pool: 32 messages × 256 bytes = 8 KB
- Atomic reference counting with state machine: `STATE_FREE` → `STATE_ALLOCATING` → active ref count
- Max 8 simultaneous references per message with overflow protection
- Zero-copy access via RAII-style `MsgHandle<>` wrapper
- Messages automatically return to pool when handle destroyed

**Utilities**:
- `ftl/util/cqueue.h` - Lock-free circular queue; capacity must be power-of-2
- `ftl/util/misc.h` - Consolidates CRC16 and Device ID functionality:
  - `crc16::calculate()`, `crc16::verify()` - CRC16-CCITT (polynomial 0x1021)
  - `device_id::get_device_id()` - 8-bit unique ID from Pico flash
  - `device_id::get_device_id_32()` - 32-bit variant
  - `device_id::is_device(target_id)` - Device identification

### Device Drivers (`devices/`)

Standalone sensor driver library built as `StandaloneDevices`:

- **HX711** (`hx711_s.cpp/h`): 24-bit ADC for load cells with software bit-banging protocol
- **ADS1115** (`ads1115_s.cpp/h`): 16-bit I2C ADC with polling mode and callback support

Both drivers follow a pattern: `init()`, `update()/start_polling()`, and provide data through getters or callbacks.

**Device Utilities** (`devices/misc/utility.h`):
- Retry mechanisms: `retry_with_error_limit()`, `retry_with_timeout()`
- Byte manipulation: `combine_bytes()`, `split_bytes()`
- Math helpers: `clamp()`, `lerp()`
- Device logging: `log_device(device_name, format, ...)`

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
- **Inter-core messaging**: Core 1 uses `ftl::multicore::send_from_core1()` to queue messages
- **Shared state**: Use `SystemState::globals_[]` atomic variables for coordination
- Call `SystemCore::shutdown()` to signal both cores to exit gracefully
- Core 1 automatically exits when `system_active_` becomes false

### Testing with MOCK_MODULE

Build standalone test executable: `cmake --build build --target MOCK_MODULE`

- Single-core variant for testing FTL messaging without hardware dependencies
- Compiled with `-DMOCK_MODULE` flag, disables multicore code paths
- Runs message dispatcher loop for integration testing
- Conditional compilation in `main.cpp` (lines 6-12, 62-100)

## Hardware Configuration

**Pin Assignments**:
- **UART0**: GP0 (TX), GP1 (RX) @ 230400 baud (configured in `ftl/config.settings`)
- **HX711**: GPIO-based software bit-banging (pins configured in device initialization)
- **ADS1115**: I2C interface (pins configurable in device init)
- **Board**: Raspberry Pi Pico 2 W (RP2350 with WiFi hardware)

## Debugging

**VS Code Configuration**:
- Debug configs in `.vscode/launch.json` support Cortex-Debug with OpenOCD
- Two configurations: built-in OpenOCD or external OpenOCD server
- SVD file enables hardware register inspection during debugging
- Uses CMake Tools + Ninja for builds

## Important Notes

- **STDIO**: USB CDC is enabled (`pico_enable_stdio_usb`), UART stdio is disabled
- **CMake pico_bundle**: A convenience interface library bundles common Pico SDK components (stdlib, hardware interfaces, multicore, DMA, etc.)
- **Linker options**: Includes `--gc-sections` for dead code elimination and `--print-memory-usage`
- **Generated code**: Never manually edit files in `ftl/generated/` - always modify `messages.yaml` and rebuild
- **C++ Standard**: Requires C++23 for FTL library (uses modern features like `std::span`, concepts)
- **Task scheduling**: Use `add_task()` in Core controllers for periodic operations instead of manual timing
- **Shutdown sequence**: `main.cpp` resets USB and enters bootloader mode after clean shutdown
- **Code generation**: Message classes auto-generated from `messages.yaml` via Jinja2 templates; creates type-safe `View` classes for zero-copy payload access

## Git Branch Strategy

- **Main branch**: `main`
- **Current work**: Development happens on feature branches (current: `FORWARD_MODULE`)
- Recent work includes UART DMA control, RX/TX handling, and pin configuration
