#pragma once

#include "util/allocator.h"
#include "config.settings"
#include <cstdint>

namespace ftl_internal {
    class DmaController;
}

namespace ftl {

using MessagePoolType = MessagePool<ftl_config::MAX_MESSAGE_SIZE, ftl_config::MESSAGE_POOL_SIZE>;
using PoolHandle = MessagePoolType::Handle;

class MessageHandle;

namespace rx {

struct Statistics {
    uint32_t total_bytes_received;
    uint32_t total_messages_received;
    uint32_t crc_errors;
    uint32_t framing_errors;
};

void initialize();
void process(ftl_internal::DmaController& dma_controller);
bool has_message();
MessageHandle get_message();
Statistics get_statistics();
uint32_t get_pool_allocated_count();
uint32_t get_queue_count();

}} // namespace ftl::rx