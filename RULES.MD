# RULES.MD

Development guidelines and best practices for the FLIGHT embedded system running on Raspberry Pi Pico 2 (RP2350) with C++23.

## C++23 Embedded Best Practices

### Memory Management

1. **No Dynamic Allocation After Startup**
   - All heap allocations must occur during initialization
   - Use static allocation and stack-based objects in runtime code
   - Prefer `std::array` over `std::vector`, `std::optional` over pointers
   - Eliminates fragmentation and provides deterministic behavior

2. **Stack Usage**
   - Be mindful of stack depth in recursive functions (avoid if possible)
   - Large buffers (>256 bytes) should be static or heap-allocated at init
   - Each core has limited stack space (~4KB default)

3. **Constexpr Everything**
   - Use `constexpr` for compile-time computation whenever possible
   - Reduces runtime overhead and flash usage
   - Enables template metaprogramming for zero-cost abstractions

### Language Features

1. **Exceptions and RTTI: DISABLED**
   - Never use `throw` or exception handling (disabled in build)
   - RTTI disabled to save flash and reduce overhead
   - Use return codes, `std::optional`, or `std::expected` for error handling

2. **Modern C++ Features (Allowed)**
   - Templates and template metaprogramming (extensively used)
   - `constexpr` and `consteval` for compile-time computation
   - `std::array`, `std::optional`, `std::span`, `std::string_view`
   - Structured bindings: `auto [x, y, z] = tuple;`
   - `if constexpr` for compile-time branching
   - Concepts for template constraints (C++20+)
   - Ranges and views (C++20+) - use judiciously, watch code size

3. **Features to Avoid**
   - `std::string` (use fixed buffers or `std::array<char, N>`)
   - `std::vector`, `std::map`, `std::set` (dynamic allocation)
   - Virtual functions (unless absolutely necessary, vtable overhead)
   - `std::shared_ptr`, `std::unique_ptr` in hot paths (runtime overhead)

### Type Safety

1. **Strong Types**
   - Wrap primitives in type-safe structs (e.g., `struct Celsius { float value; }`)
   - Use `enum class` instead of `#define` constants
   - Leverage concepts to constrain template parameters

2. **Const Correctness**
   - Mark all non-mutating functions as `const`
   - Use `const&` for read-only parameters (except primitives)
   - Prefer `constexpr` over `const` when possible

## RP2350 Specific Guidelines

### Dual-Core Architecture

1. **Core Responsibilities (NEVER Violate)**
   - **Core 0**: WiFi, SD card, load cell, data consumption
   - **Core 1**: I2C sensors, real-time data generation
   - Each core owns its peripherals exclusively
   - Never access another core's hardware directly

2. **Inter-Core Communication**
   - Use `MessageChannel<T>` for all core-to-core messaging
   - Lock-free ring buffers only (no mutexes between cores)
   - Message payloads ≤ 122 bytes
   - Poll channels regularly, messages are fire-and-forget

3. **Synchronization**
   - No mutexes across cores (use lock-free data structures)
   - Use atomic operations for flags/counters
   - Spin locks only for very brief critical sections (<10 cycles)
   - Memory barriers when necessary (`__dmb()`, `__dsb()`)

### Real-Time Constraints

1. **Timing Guarantees**
   - Load cell: Must read every 20ms (50Hz, hard deadline)
   - MS4525D0: 16-sample oversampling, ~8ms per reading
   - File sync: 1 second rotation, non-critical
   - WiFi polling: 50ms when using `CYW43_ARCH_POLL`

2. **ISR Best Practices**
   - Keep interrupt handlers under 10μs
   - No I/O operations in ISRs
   - Set flags, push to queue, defer to main loop
   - Disable interrupts only when absolutely necessary

3. **Polling vs Interrupts**
   - Use DMA for bulk transfers (SPI, UART)
   - I2C sensors: Polled at controlled intervals
   - HX711: Timer-based polling at 50Hz
   - WiFi: Cooperative polling, no blocking

### Hardware Abstraction

1. **Template-Based HAL**
   - Use compile-time configuration for hardware instances
   - Example: `I2CBus<i2c0, SDA, SCL, BAUDRATE>`
   - Zero runtime overhead for pin/peripheral selection
   - Device traits pattern for sensor metadata

2. **GPIO Usage**
   - Document reserved pins in central config file
   - Initialize GPIO in single location (avoid scattered init)
   - Use GPIO functions, not direct register access
   - Current reservations: GPIO 4/5 (I2C), per HX711 config

3. **Peripheral Best Practices**
   - One peripheral instance per bus (e.g., single `i2c0` instance)
   - Use RAII for resource management (constructors/destructors)
   - Avoid blocking operations in fast paths
   - Check peripheral busy flags before operations

## Project Architecture Patterns

### Message Passing

1. **Channel Usage**
   - `SensorChannel`: Core 1 → Core 0 (sensor data)
   - `CommandChannel`: Core 0 → Core 1 (control)
   - Always check return values from `push()`/`pop()`
   - Handle full/empty channel conditions gracefully

2. **Message Design**
   - Keep messages small and flat (no pointers)
   - Use tagged unions or `std::variant` for polymorphism
   - Include timestamps and sequence numbers
   - Version message formats for future compatibility

### File System

1. **SD Card Operations**
   - Template-based file types in `sdcard/sd_config.h`
   - All files must be opened in `core0::init()`
   - Manual sync required, buffered writes by default
   - Check return codes on all FatFs operations
   - Close files in `core0::shutdown()`

2. **Buffer Management**
   - Double buffering for SD writes (swap buffers)
   - Sync rotation: 1 file per second, round-robin
   - Flush buffers before shutdown/power loss
   - Buffer sizes: 1KB (logs), 2KB (telemetry)

### I2C Sensor Framework

1. **Adding New Sensors**
   - Create driver in `i2c/drivers/` implementing Device concept
   - Define `DeviceTraits<YourSensor>` with address/name/data_type
   - Add to bus in `core1.cpp` with callback
   - Process data in `core0.cpp` sensor handler

2. **Polling Strategy**
   - All sensors disabled by default
   - Start via HTTP "start" event
   - Poll at sensor-specific intervals
   - Use callbacks for async data handling

### HTTP/WiFi System

1. **Access Point**
   - SSID/password in `http/config/http_config.h`
   - IP: 192.168.4.1 (static)
   - Services: HTTP, DNS, DHCP
   - Cooperative polling model (non-blocking)

2. **Event System**
   - HTTP events queued with timestamps
   - Start/stop commands processed in main loop
   - No blocking in HTTP request handlers
   - Generate HTML dynamically with live data

## Code Style and Organization

### File Structure

1. **Headers**
   - Header guards: `#pragma once` (compiler-supported)
   - Include order: system, SDK, third-party, project
   - Forward declare when possible, include in .cpp
   - Keep public interface minimal

2. **Namespaces**
   - Use namespaces for logical grouping (e.g., `i2c::`, `sdcard::`)
   - Never `using namespace` in headers
   - Prefer explicit qualification in .cpp files

3. **Naming Conventions**
   - Types: `PascalCase` (classes, structs, enums)
   - Functions: `snake_case`
   - Variables: `snake_case`
   - Constants: `SCREAMING_SNAKE_CASE` or `kCamelCase`
   - Templates: `PascalCase` for parameters

### Configuration

1. **Centralized Config**
   - `app/app_config.h`: Timing, core contracts, message types
   - `sdcard/sd_config.h`: File paths, buffer sizes
   - `storage/storage_config.h`: Binary format, SPI config
   - `http/config/`: Network settings, lwIP tuning

2. **Compile-Time Configuration**
   - Use `constexpr` for all constants
   - Template parameters for hardware config
   - `#define` only for conditional compilation
   - Document all magic numbers

### Error Handling

1. **Return Value Patterns**
   - `bool`: Success/failure for simple operations
   - `std::optional<T>`: Value or empty for lookup/parse
   - `std::expected<T, E>`: Value or error code (C++23)
   - Negative errno codes for POSIX-style APIs

2. **Assertions**
   - Use SDK `assert()` for invariants and preconditions
   - Debug builds only (stripped in release)
   - Add descriptive messages: `assert(ptr && "Null pointer")`
   - Never put side effects in assertions

3. **Critical Errors**
   - Log to SD card if possible
   - LED indicators for hardware failures
   - Coordinated shutdown between cores
   - Flush all buffers before halt

## Build System and Toolchain

### CMake Configuration

1. **Standards**
   - C17 for `.c` files
   - C++23 for `.cpp` files
   - Board: `pico2_w`
   - SDK: 2.2.0+

2. **Compiler Requirements**
   - arm-none-eabi-gcc 13.3+ (C++23 support)
   - Located at `~/.pico-sdk/`
   - Build via Pico VS Code extension
   - Output: `build/FLIGHT.uf2`

3. **Flags and Optimizations**
   - Release: `-O2` or `-O3` (size vs speed tradeoff)
   - Debug: `-Og -g` for debugger support
   - Warnings: `-Wall -Wextra -Wpedantic`
   - Enable LTO (Link-Time Optimization) for production

### Dependencies

1. **Pico SDK**
   - Core libraries: hardware_*, pico_stdlib
   - WiFi: pico_cyw43_arch, pico_lwip
   - Multicore: pico_multicore
   - Always use SDK abstractions over direct register access

2. **Third-Party**
   - FatFs (lib/sdcard/src/ff15/): SD card filesystem
   - lwIP: TCP/IP stack (configured in lwipopts.h)
   - Minimize external dependencies, vendor when necessary

## AI Assistant Guidelines

### Code Generation

1. **When Adding Features**
   - Read existing similar code first to match patterns
   - Use project's template abstractions (I2CBus, SDFile, etc.)
   - Update config files (`*_config.h`) before implementation
   - Follow established naming and style conventions

2. **When Modifying Code**
   - Preserve compile-time optimizations (constexpr, templates)
   - Maintain core ownership boundaries (Core 0 vs Core 1)
   - Update message types in `app_config.h` if needed
   - Verify timing constraints aren't violated

3. **When Debugging**
   - Check SD card logs first
   - Verify sensor initialization order
   - Inspect channel message flow
   - Review timing with logic analyzer mindset

### Testing Approach

1. **Unit Testing**
   - Test device traits and template instantiations
   - Mock hardware interfaces for business logic
   - Verify message serialization/deserialization
   - Test error handling paths

2. **Integration Testing**
   - Core 0 and Core 1 initialization sequence
   - Message passing under load
   - File system operations (mount, write, sync, unmount)
   - WiFi connectivity and HTTP serving

3. **Hardware Testing**
   - Sensor communication (I2C bus scan)
   - SD card read/write performance
   - Load cell calibration and tare
   - Timing validation (scope/logic analyzer)

### Documentation

1. **Code Comments**
   - Explain *why*, not *what* (code is self-documenting)
   - Document hardware dependencies and pin usage
   - Note timing constraints and performance requirements
   - Add references to datasheets for hardware details

2. **File Headers**
   - Brief description of module purpose
   - Core ownership (if applicable)
   - Key dependencies and assumptions
   - Author/date not required (use git history)

3. **README and CLAUDE.md**
   - Keep CLAUDE.md updated with architecture changes
   - README.md for external users (build instructions)
   - RULES.MD for development practices (this file)
   - Avoid duplication between documentation files

## Performance Optimization

### Flash Usage

1. **Code Size**
   - Template instantiation can bloat flash (watch build size)
   - Use `if constexpr` to eliminate dead code paths
   - Enable LTO to deduplicate template code
   - Profile flash usage: `arm-none-eabi-size FLIGHT.elf`

2. **Data Storage**
   - Const data in flash: `static constexpr`
   - Lookup tables in flash: `const` arrays
   - String literals: Use `PSTR()` macro if available
   - Reduce duplication of error/log strings

### RAM Usage

1. **Static Allocation**
   - Minimize global variables (namespaces help organization)
   - Use `static` locals for persistent state
   - Pool allocations at startup for known maximums
   - Monitor stack usage per core

2. **Buffer Optimization**
   - Reuse buffers across operations
   - Size buffers to actual needs, not worst-case
   - Use `std::span` for views into existing buffers
   - Align buffers for DMA (16-byte for RP2350)

### Execution Speed

1. **Hot Paths**
   - Inline small, frequently called functions
   - Loop unrolling for fixed iterations
   - Branch prediction hints if available
   - Minimize cache misses (locality of reference)

2. **Peripheral Access**
   - Batch register writes when possible
   - Use DMA for block transfers
   - Avoid polling busy-wait loops (use interrupts/events)
   - Read-modify-write for GPIO is expensive (use set/clear)

## Security and Safety

### Data Integrity

1. **Checksums**
   - CRC for critical data structures
   - Verify SD card writes periodically
   - Validate sensor data ranges
   - Message sequence numbers to detect drops

2. **Watchdog**
   - Enable hardware watchdog for production
   - Pet watchdog from main loops only
   - Timeout: 8 seconds (allow for WiFi delays)
   - Reset if either core hangs

### Power Management

1. **Brownout**
   - Monitor supply voltage if possible
   - Flush buffers on low voltage detect
   - Graceful shutdown sequence
   - Test power-cycle recovery

2. **Low Power**
   - Use `sleep_ms()` when possible (not tight loops)
   - Clock gating for unused peripherals
   - WiFi sleep modes when inactive
   - Consider dormant mode for long idle

## Common Pitfalls to Avoid

1. **Cross-Core Access**
   - Never access GPIO/peripherals from wrong core
   - Use message passing, not shared memory (without atomics)
   - Core 1 launch happens on Core 0, then ownership transfers

2. **Timing Violations**
   - Don't block in 50Hz load cell loop
   - WiFi operations can take 10s of milliseconds
   - SD card writes can stall if card is slow
   - I2C can stretch clock indefinitely (timeout!)

3. **Resource Exhaustion**
   - Message channels can fill (check push() return)
   - SD buffers can overflow (sync regularly)
   - Stack overflow crashes silently (monitor usage)
   - FatFs has file handle limits (close unused)

4. **Undefined Behavior**
   - Uninitialized variables (use `-Wuninitialized`)
   - Integer overflow (use safe math or check)
   - Array out-of-bounds (use `std::array` with `.at()`)
   - Type punning (use `std::bit_cast` or unions)

---

## Quick Reference: Development Checklist

### Adding a New Feature
- [ ] Update relevant `*_config.h` file(s)
- [ ] Respect core ownership (Core 0 vs Core 1)
- [ ] Use project template patterns (I2CBus, SDFile, etc.)
- [ ] No dynamic allocation in runtime code
- [ ] Add to initialization sequence (init functions)
- [ ] Add to shutdown sequence (cleanup)
- [ ] Update CLAUDE.md if architecture changes
- [ ] Test timing constraints with representative load

### Code Review Checklist
- [ ] Const correctness (`const`, `constexpr`)
- [ ] No exceptions or RTTI
- [ ] Bounded buffers and arrays
- [ ] Error handling on all I/O operations
- [ ] No blocking in hot paths
- [ ] Memory barriers if sharing data across cores
- [ ] Documentation explains *why*, not *what*
- [ ] Flash/RAM budget acceptable

---

*Last Updated: 2025-10-01*
*Toolchain: arm-none-eabi-gcc 14_2_Rel1, Pico SDK 2.2.0, C++23*
