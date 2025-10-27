# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_3_Rel1)
set(picotoolVersion 2.2.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2_w CACHE STRING "Board type")
include(pico_sdk_import.cmake)

project(FLIGHT C CXX ASM)

pico_sdk_init()

add_executable(FLIGHT main.cpp)

pico_set_program_name(FLIGHT "FLIGHT")
pico_set_program_version(FLIGHT "0.1")

pico_enable_stdio_uart(FLIGHT 0)
pico_enable_stdio_usb(FLIGHT 1)

add_subdirectory(lib)

# Create a bundled interface library for all Pico SDK components
add_library(pico_bundle INTERFACE)

target_link_libraries(pico_bundle INTERFACE
    # Core Pico libraries
    pico_stdlib
    pico_multicore
    pico_bootrom
    pico_sync
        
    # Hardware interfaces
    hardware_resets
    hardware_clocks
    hardware_spi
    hardware_i2c
    hardware_gpio
    hardware_uart
    hardware_dma
    hardware_irq
    hardware_timer
    hardware_pwm
    hardware_pio
    hardware_sync
    
    # C++ support
    pico_cxx_options
)

target_link_libraries(FLIGHT
    pico_bundle
    third_party
)

# Add the standard include files to the build
target_include_directories(FLIGHT PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/sdcard
        ${CMAKE_CURRENT_LIST_DIR}/lib
)

# Linker options
target_link_options(${PROJECT_NAME} PRIVATE
    -Wl,--gc-sections
    -Wl,--print-memory-usage
    -Wl,--no-wchar-size-warning
    -Wl,--cref
    -Wl,-Map=${PROJECT_NAME}.map
)

pico_add_extra_outputs(FLIGHT)

