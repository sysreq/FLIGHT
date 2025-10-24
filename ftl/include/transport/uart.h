#pragma once

/**
 * @file uart.h
 * @brief Unified UART Transport Layer API
 * 
 * This is the ONLY header other subsystems need to include for UART transport.
 * It provides a clean, core-safe interface that encapsulates all internal
 * complexity including DMA, RX state machine, TX queue, and multicore FIFO.
 * 
 * Architecture:
 * - Core 0: Initializes, polls, and handles all UART/DMA operations
 * - Core 1: Can send messages which are automatically routed through FIFO
 * - Internal modules handle framing, CRC, queuing, and synchronization
 * 
 * Usage:
 * ```cpp
 * // On Core 0 initialization:
 * ftl::uart::initialize(uart0, MY_DEVICE_ID);
 * 
 * // In Core 0 main loop:
 * ftl::uart::poll();
 * 
 * // From either core:
 * ftl::uart::send_message(payload);  // Automatically routed
 * 
 * // From Core 0 to receive:
 * if (ftl::uart::has_message()) {
 *     auto msg = ftl::uart::get_message();
 *     // Process message...
 * }
 * ```
 */

#include "ftl.settings"
#include <cstdint>
#include <span>
#include <string_view>

// Forward declare hardware types
struct uart_inst;
typedef struct uart_inst uart_inst_t;

namespace ftl {

// Forward declare MessageHandle from public API
class MessageHandle;

namespace uart {

/**
 * @brief TX statistics
 */
struct TxStatistics {
    uint32_t total_messages_queued;
    uint32_t total_messages_sent;
    uint32_t queue_full_drops;
    uint32_t current_queue_depth;
    uint32_t peak_queue_depth;
};

/**
 * @brief RX statistics
 */
struct RxStatistics {
    uint32_t total_bytes_received;
    uint32_t total_messages_received;
    uint32_t crc_errors;
    uint32_t framing_errors;
};

/**
 * @brief Multicore statistics
 */
struct MulticoreStatistics {
    uint32_t core1_messages_sent;      // Messages sent from Core 1
    uint32_t core1_fifo_full_drops;    // Drops due to FIFO full
    uint32_t core0_messages_received;  // Messages processed on Core 0
    uint32_t core0_tx_queue_drops;     // Drops due to Core 0 TX queue full
};

/**
 * @brief Initialize the UART transport layer
 * 
 * Must be called exactly once on Core 0 before any other UART operations.
 * Claims DMA channels, configures UART hardware, and initializes internal
 * RX/TX/Multicore queues.
 * 
 * @pre Called on Core 0 only
 * @pre Called before Core 1 is started
 * @pre Message pool must be initialized
 * 
 * @param uart_inst UART instance (e.g., uart0, uart1)
 * @param source_id This device's source ID for outgoing messages
 */
void initialize(uart_inst_t* uart_inst, uint8_t source_id);

/**
 * @brief Poll the UART transport layer
 * 
 * Must be called repeatedly from Core 0's main loop.
 * This function:
 * - Processes incoming DMA data through the RX state machine
 * - Processes messages from Core 1's FIFO
 * - Processes the Core 0 TX queue and starts new DMA sends
 * 
 * @pre Must be called from Core 0 only
 */
void poll();

/**
 * @brief Send a message (Core-safe)
 * 
 * This is the unified send function that works from any core.
 * - If called from Core 0: Enqueues the message to the TX queue
 * - If called from Core 1: Pushes the message to the inter-core FIFO
 * 
 * Non-blocking. Returns immediately with success/failure status.
 * 
 * @param payload Message payload (max ftl_config::MAX_PAYLOAD_SIZE bytes)
 * @return true if queued, false if queue/FIFO is full or payload invalid
 */
bool send_message(std::span<const uint8_t> payload);

/**
 * @brief Send a string message (Core-safe)
 * 
 * Convenience wrapper for text messages.
 * 
 * @param message String message to send
 * @return true if queued, false if queue/FIFO is full
 */
bool send_message(std::string_view message);

/**
 * @brief Check if a fully-formed message is available
 * 
 * @return true if a message is waiting in the RX queue
 */
bool has_message();

/**
 * @brief Get the next available message
 * 
 * Removes and returns the oldest message from the RX queue.
 * If no message is available, returns an invalid MessageHandle.
 * 
 * @return A valid MessageHandle, or an invalid one if no message
 */
MessageHandle get_message();

/**
 * @brief Check if the Core 0 TX queue is ready
 * 
 * Useful for flow control from Core 0.
 * 
 * @return true if the TX queue has space
 */
bool is_tx_ready();

/**
 * @brief Check if the Core 1 FIFO is ready
 * 
 * Useful for flow control from Core 1.
 * 
 * @return true if the inter-core FIFO has space
 */
bool is_core1_tx_ready();

/**
 * @brief Get TX statistics
 * 
 * @return TX statistics structure
 */
TxStatistics get_tx_statistics();

/**
 * @brief Get RX statistics
 * 
 * @return RX statistics structure
 */
RxStatistics get_rx_statistics();

/**
 * @brief Get multicore statistics
 * 
 * @return Multicore statistics structure
 */
MulticoreStatistics get_multicore_statistics();

} // namespace uart
} // namespace ftl
