#pragma once

#include "ftl.settings"
#include <span>
#include <string_view>
#include <cstdint>

namespace ftl_internal {
    class DmaController;
}

namespace ftl {

namespace tx {

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
 * @brief Queue a message for transmission (non-blocking)
 * 
 * Message is queued and will be transmitted during next poll() call.
 * Returns immediately without waiting for DMA.
 * 
 * @param dma_controller DMA controller instance
 * @param payload Message payload to send
 * @return true if queued successfully, false if queue full
 */
bool send_message(ftl_internal::DmaController& dma_controller, std::span<const uint8_t> payload);

/**
 * @brief Queue a string message for transmission (non-blocking)
 * 
 * @param dma_controller DMA controller instance
 * @param message String message to send
 * @return true if queued successfully, false if queue full
 */
bool send_message(ftl_internal::DmaController& dma_controller, std::string_view message);

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

} // namespace tx
} // namespace ftl
