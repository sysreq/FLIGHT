#pragma once

#include "util/allocator.h"
#include "ftl.settings"
#include <cstdint>

namespace ftl_internal {
    class DmaController;
}

namespace ftl {

using MessagePoolType = MessagePool<ftl_config::MAX_MESSAGE_SIZE, ftl_config::MESSAGE_POOL_SIZE>;
using PoolHandle = MessagePoolType::Handle;

class MessageHandle;

namespace uart {
namespace internal_rx {

/**
 * @brief RX statistics
 */
struct Statistics {
    uint32_t total_bytes_received;
    uint32_t total_messages_received;
    uint32_t crc_errors;
    uint32_t framing_errors;
};

/**
 * @brief Initialize RX subsystem
 */
void initialize();

/**
 * @brief Process incoming data from DMA controller
 * 
 * Reads bytes from circular buffer and feeds them through the
 * RX state machine to assemble complete messages.
 * 
 * @param dma_controller DMA controller instance
 */
void process(ftl_internal::DmaController& dma_controller);

/**
 * @brief Check if a fully-formed message is available
 * 
 * @return true if a message is waiting in the RX queue
 */
bool has_message();

/**
 * @brief Get the next available message
 * 
 * @return A valid MessageHandle, or an invalid one if no message
 */
MessageHandle get_message();

/**
 * @brief Get RX statistics
 * 
 * @return Statistics structure
 */
Statistics get_statistics();

/**
 * @brief Get number of allocated pool handles
 * 
 * @return Count of allocated handles
 */
uint32_t get_pool_allocated_count();

/**
 * @brief Get number of messages in RX queue
 * 
 * @return Queue depth
 */
uint32_t get_queue_count();

} // namespace internal_rx
} // namespace uart
} // namespace ftl
