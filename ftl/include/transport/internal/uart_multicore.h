#pragma once

/**
 * @file uart_multicore_internal.h
 * @brief Inter-core message transmission using hardware FIFO (Internal)
 * 
 * Allows Core 1 to queue messages for transmission on Core 0 using the
 * RP2040/RP2350's hardware FIFO with minimal overhead.
 * 
 * Architecture:
 * - Core 1: Calls send_from_core1(handle) → pushes to hardware FIFO
 * - Core 0: Calls process_fifo_messages() → moves handles to TX queue
 * 
 * Benefits:
 * - Ultra-low latency: Hardware FIFO is single-cycle access
 * - No locks needed: Hardware handles synchronization
 * - Only 8 bytes per message (handle + length + magic)
 * - Direct handle passing eliminates re-serialization
 */

#include "ftl.settings"
#include "util/allocator.h"
#include <cstdint>

namespace ftl {

using MessagePoolType = MessagePool<ftl_config::MAX_MESSAGE_SIZE, ftl_config::MESSAGE_POOL_SIZE>;
using PoolHandle = MessagePoolType::Handle;

namespace uart {
namespace internal_multicore {

/**
 * @brief Multicore TX statistics
 */
struct Statistics {
    uint32_t core1_messages_sent;      // Messages sent from Core 1
    uint32_t core1_fifo_full_drops;    // Drops due to FIFO full
    uint32_t core0_messages_received;  // Messages processed on Core 0
    uint32_t core0_tx_queue_drops;     // Drops due to Core 0 TX queue full
};

/**
 * @brief Initialize multicore TX system
 * 
 * Must be called on Core 0 after message pool initialization.
 * Does not require Core 1 to be running yet.
 */
void initialize();

/**
 * @brief Send a pre-filled handle from Core 1
 * 
 * Non-blocking. Pushes handle to hardware FIFO for Core 0 to process.
 * Can only be called from Core 1.
 * 
 * The handle must already be acquired and filled with payload data.
 * If this function returns false, the caller is responsible for
 * releasing the handle.
 * 
 * @param handle Pre-filled pool handle to send
 * @param length Payload length (for validation)
 * @return true if queued to FIFO, false if FIFO full
 */
bool send_from_core1(PoolHandle handle, uint8_t length);

/**
 * @brief Process messages from Core 1 (call from Core 0 during poll)
 * 
 * Reads handles from hardware FIFO and directly enqueues them to the
 * Core 0 TX queue. No re-serialization needed.
 * Must be called from Core 0 only.
 * 
 * Typically called from uart::poll() so Core 0 handles all UART/DMA.
 */
void process_fifo_messages();

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

} // namespace internal_multicore
} // namespace uart
} // namespace ftl
