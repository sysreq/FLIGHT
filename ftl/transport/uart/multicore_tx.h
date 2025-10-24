#pragma once

/**
 * @file multicore_tx.h
 * @brief Inter-core message transmission using hardware FIFO
 * 
 * Allows Core 1 to queue messages for transmission on Core 0 using the
 * RP2040/RP2350's hardware FIFO with minimal overhead.
 * 
 * Architecture:
 * - Core 1: Calls ftl::multicore::send_from_core1() → pushes to hardware FIFO
 * - Core 0: Calls ftl::multicore::process_core1_messages() during poll()
 * 
 * Benefits:
 * - Ultra-low latency: Hardware FIFO is single-cycle access
 * - No locks needed: Hardware handles synchronization
 * - Only 8 bytes per message (handle + length)
 * - Core 1 never blocks on UART/DMA
 */

#include "ftl.settings"
#include <span>
#include <cstdint>
#include <string_view>

namespace ftl {
namespace multicore {

/**
 * @brief Multicore TX statistics
 */
struct Statistics {
    uint32_t core1_messages_sent;      // Messages sent from Core 1
    uint32_t core1_fifo_full_drops;    // Drops due to FIFO full
    uint32_t core0_messages_received;  // Messages processed on Core 0
};

/**
 * @brief Initialize multicore TX system
 * 
 * Must be called on Core 0 after ftl::initialize().
 * Does not require Core 1 to be running yet.
 */
void initialize();

/**
 * @brief Queue message for transmission from Core 1
 * 
 * Non-blocking. Pushes message to hardware FIFO for Core 0 to process.
 * Can only be called from Core 1.
 * 
 * @param payload Message payload to send
 * @return true if queued to FIFO, false if FIFO full
 */
bool send_from_core1(std::span<const uint8_t> payload);

/**
 * @brief Queue string message for transmission from Core 1
 * 
 * @param message String message to send
 * @return true if queued to FIFO, false if FIFO full
 */
bool send_from_core1(std::string_view message);

/**
 * @brief Process messages from Core 1 (call from Core 0 during poll)
 * 
 * Reads messages from hardware FIFO and queues them for transmission.
 * Must be called from Core 0 only.
 * 
 * Typically called from ftl::poll() so Core 0 handles all UART/DMA.
 */
void process_core1_messages();

/**
 * @brief Get multicore TX statistics
 * 
 * @return Statistics structure
 */
Statistics get_statistics();

/**
 * @brief Check if ready to accept messages from Core 1
 * 
 * @return true if FIFO has space
 */
bool is_core1_ready();

} // namespace multicore
} // namespace ftl
