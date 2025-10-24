#pragma once

/**
 * @file ftl.h
 * @brief FTL (Faster Than Light) - Main header for the message-oriented UART library
 *
 * This is the single-include header for using the FTL library in your project.
 *
 * Quick Start:
 * ```cpp
 * #include "ftl/ftl.h"
 *
 * int main() {
 *     ftl::initialize();
 *     ftl::messages::Dispatcher dispatcher;
 *
 *     // Set up message handlers
 *     dispatcher.set_handler([](const ftl::messages::MSG_SENSOR_DATA_View& msg) {
 *         printf("Temp: %.1f\n", msg.temperature());
 *     });
 *
 *     // Main loop
 *     while (true) {
 *         ftl::poll();
 *         while (ftl::has_msg()) {
 *             auto msg = ftl::get_msg();
 *             dispatcher.dispatch(msg);
 *         }
 *
 *         // Send messages
 *         dispatcher.send<ftl::messages::MSG_HEARTBEAT>(counter++, 100, "Device");
 *     }
 * }
 * ```
 *
 * CMake Integration:
 * ```cmake
 * add_subdirectory(ftl)
 * target_link_libraries(your_target ftl)
 * ```
 */

// Core FTL API
#include "core/ftl_api.h"

// Configuration (users may want to reference ftl_config constants)
#include "ftl.settings"

// Generated message types and dispatcher
#include "generated/messages.h"
