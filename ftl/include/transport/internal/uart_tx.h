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

struct Statistics {
    uint32_t total_messages_queued;
    uint32_t total_messages_sent;
    uint32_t queue_full_drops;
    uint32_t current_queue_depth;
    uint32_t peak_queue_depth;
};

void initialize(uint8_t source_id);

PoolHandle acquire_and_fill_message(std::span<const uint8_t> payload);

bool enqueue_message_on_core0(PoolHandle handle);
void process_tx_queue(ftl_internal::DmaController& dma_controller);

bool is_ready();
uint8_t get_source_id();

Statistics get_statistics();
bool is_queue_empty();
uint32_t get_queue_count();

} // namespace internal_tx
} // namespace uart
} // namespace ftl
