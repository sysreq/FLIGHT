#pragma once

#include "ftl.settings"
#include "util/allocator.h"
#include <cstdint>

namespace ftl {

using MessagePoolType = MessagePool<ftl_config::MAX_MESSAGE_SIZE, ftl_config::MESSAGE_POOL_SIZE>;
using PoolHandle = MessagePoolType::Handle;

namespace uart {
namespace internal_multicore {

struct Statistics {
    uint32_t core1_messages_sent;      // Messages sent from Core 1
    uint32_t core1_fifo_full_drops;    // Drops due to FIFO full
    uint32_t core0_messages_received;  // Messages processed on Core 0
    uint32_t core0_tx_queue_drops;     // Drops due to Core 0 TX queue full
};

void initialize();
bool send_from_core1(PoolHandle handle, uint8_t length);
void process_fifo_messages();
Statistics get_statistics();
bool is_core1_ready();

} // namespace internal_multicore
} // namespace uart
} // namespace ftl
