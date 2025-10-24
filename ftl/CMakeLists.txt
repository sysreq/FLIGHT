# ==============================================================================
# FTL (Faster Than Light) - Type-Safe Message Passing Library
# ==============================================================================
#
# A reusable CMake library for Raspberry Pi Pico projects providing:
# - DMA-accelerated UART messaging
# - Type-safe message generation from YAML schemas
# - Zero-copy message handling with reference counting
# - Compile-time type safety
#
# Usage in your project:
#   add_subdirectory(ftl)
#   target_link_libraries(your_target ftl)
#
# ==============================================================================

cmake_minimum_required(VERSION 3.13)

# Verify Pico SDK is available
if(NOT DEFINED PICO_SDK_PATH)
    message(FATAL_ERROR "FTL requires Pico SDK. Ensure pico_sdk_init() is called before adding FTL.")
endif()

# ==============================================================================
# Code Generation
# ==============================================================================

find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(FTL_MESSAGES_YAML "${CMAKE_CURRENT_SOURCE_DIR}/messages.yaml"
    CACHE FILEPATH "Path to messages.yaml schema file")
set(FTL_GENERATOR_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/generate_messages.py")
set(FTL_TEMPLATE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/templates")
set(FTL_GENERATED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/generated")

# Main generated files
set(FTL_MESSAGES_DETAIL_H "${FTL_GENERATED_DIR}/messages_detail.h")
set(FTL_MESSAGES_H "${FTL_GENERATED_DIR}/messages.h")
set(FTL_MESSAGES_CPP "${FTL_GENERATED_DIR}/messages.cpp")

add_custom_command(
    OUTPUT
        ${FTL_MESSAGES_DETAIL_H}
        ${FTL_MESSAGES_H}
        ${FTL_MESSAGES_CPP}
    COMMAND
        ${Python3_EXECUTABLE} ${FTL_GENERATOR_SCRIPT}
        ${FTL_MESSAGES_YAML} ${FTL_GENERATED_DIR} ${FTL_TEMPLATE_DIR}
    DEPENDS
        ${FTL_MESSAGES_YAML}
        ${FTL_GENERATOR_SCRIPT}
        ${FTL_TEMPLATE_DIR}/message.h.j2
        ${FTL_TEMPLATE_DIR}/messages_detail.h.j2
        ${FTL_TEMPLATE_DIR}/messages.h.j2
        ${FTL_TEMPLATE_DIR}/messages.cpp.j2
    COMMENT
        "Generating FTL message types from ${FTL_MESSAGES_YAML}"
    VERBATIM
)

add_custom_target(ftl_generate_messages
    DEPENDS
        ${FTL_MESSAGES_DETAIL_H}
        ${FTL_MESSAGES_H}
        ${FTL_MESSAGES_CPP}
)

# ==============================================================================
# FTL UART Transport Target
# ==============================================================================

add_library(ftl_uart STATIC
    transport/uart/uart.cpp
    transport/uart/uart_dma.cpp
    transport/uart/uart_rx.cpp
    transport/uart/uart_tx.cpp
    transport/uart/uart_multicore.cpp
)

target_include_directories(ftl_uart PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include/transport/
)

# Link Pico SDK libraries needed for UART transport
target_link_libraries(ftl_uart PUBLIC
    pico_stdlib
    pico_multicore
    pico_unique_id
    hardware_uart
    hardware_dma
    hardware_gpio
)

# Require C++23
target_compile_features(ftl_uart PUBLIC cxx_std_23)

# ==============================================================================
# FTL Core Library Target
# ==============================================================================

add_library(ftl STATIC
    # Core API
    core/ftl_api.cpp

    # Generated code
    ${FTL_MESSAGES_CPP}
)

add_dependencies(ftl ftl_generate_messages)

# Include directories
target_include_directories(ftl PUBLIC
    # FTL directory itself (for internal includes)
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include/

    # Generated headers
    ${FTL_GENERATED_DIR}
)

# Link transport layers and SDK utilities
target_link_libraries(ftl PUBLIC
    ftl_uart              # UART transport layer
    pico_unique_id        # For device identification
)

# Require C++23
target_compile_features(ftl PUBLIC cxx_std_23)

# ==============================================================================
# Library Information
# ==============================================================================

message(STATUS "FTL Library Configuration:")
message(STATUS "  Message Schema: ${FTL_MESSAGES_YAML}")
message(STATUS "  Generated Output: ${FTL_GENERATED_DIR}")
message(STATUS "  C++ Standard: C++23")
message(STATUS "  Transport Layers: UART")