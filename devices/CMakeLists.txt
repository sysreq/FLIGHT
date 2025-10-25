cmake_minimum_required(VERSION 3.13)
set(CURRENT_MODULE_NAME StandaloneDevices)
# =========================================================================================
add_library(${CURRENT_MODULE_NAME}
        hx711_s.cpp
        ads1115_s.cpp
)

target_include_directories(${CURRENT_MODULE_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/misc>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(${CURRENT_MODULE_NAME}
    PUBLIC
    
    PRIVATE
        pico_stdlib
        pico_cxx_options
        pico_multicore

        hardware_gpio
        hardware_i2c
        hardware_timer
        hardware_sync
        hardware_flash
)
