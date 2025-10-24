#pragma once

#include "ftl.settings"
#include "util/allocator.h"
#include <span>
#include <string_view>
#include <cstdint>

namespace ftl_internal {
    class DmaController;
}

namespace ftl {

using MessagePoolType = MessagePool<ftl_config::MAX_MESSAGE_SIZE, ftl_config::MESSAGE_POOL_SIZE>;
using PoolHandle = MessagePoolType::Handle;

namespace uart {
namespace internal_tx {

/**
 * @brief TX queue statistics
 */
struct Statistics {
    uint32_t total_messages_queued;
    uint32_t total_messages_sent;
    uint32_t queue_full_drops;
    uint32_t current_queue_depth;
    uint32_t peak_queue_depth;
};

/**
 * @brief Initialize TX subsystem
 * 
 * @param source_id Device source ID for outgoing messages
 */
void initialize(uint8_t source_id);

/**
 * @brief Acquire a pool handle and fill it with payload
 * 
 * This is the first step of sending a message - it allocates a handle
 * from the message pool and copies the payload into it.
 * Used by both Core 0 and Core 1 paths.
 * 
 * @param payload Message payload to copy
 * @return PoolHandle if successful, INVALID if pool exhausted
 */
PoolHandle acquire_and_fill_message(std::span<const uint8_t> payload);

/**
 * @brief Enqueue a pre-filled handle to Core 0's TX queue
 * 
 * This is the second step - it takes an already-filled handle and
 * queues it for transmission. Called from Core 0's send path and
 * from the multicore FIFO processor.
 * 
 * @param handle Pre-filled pool handle
 * @return true if queued successfully, false if queue full
 */
bool enqueue_message_on_core0(PoolHandle handle);

/**
 * @brief Process TX queue and start DMA transfers when ready
 * 
 * Should be called regularly from poll(). Checks if DMA is idle,
 * and if so, dequeues next message and starts transmission.
 * 
 * @param dma_controller DMA controller instance
 */
void process_tx_queue(ftl_internal::DmaController& dma_controller);

/**
 * @brief Check if TX is ready to accept more messages
 * 
 * @return true if queue has space available
 */
bool is_ready();

/**
 * @brief Get device source ID
 * 
 * @return Source ID configured during initialization
 */
uint8_t get_source_id();

/**
 * @brief Get TX statistics
 * 
 * @return Statistics structure with queue metrics
 */
Statistics get_statistics();

/**
 * @brief Check if TX queue is empty
 * 
 * @return true if no messages waiting to send
 */
bool is_queue_empty();

/**
 * @brief Get current number of messages in TX queue
 * 
 * @return Number of queued messages
 */
uint32_t get_queue_count();

} // namespace internal_tx
} // namespace uart
} // namespace ftl
