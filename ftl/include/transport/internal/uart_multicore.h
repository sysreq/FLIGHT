#pragma once

#include "ftl.settings"
#include "util/allocator.h"
#include <cstdint>

namespace ftl {

using MessagePoolType = MessagePool<ftl_config::MAX_MESSAGE_SIZE, ftl_config::MESSAGE_POOL_SIZE>;
using PoolHandle = MessagePoolType::Handle;

namespace uart {
namespace internal_multicore {

void initialize();
bool send_from_core1(PoolHandle handle, uint8_t length);
void process_fifo_messages();
bool is_core1_ready();

} // namespace internal_multicore
} // namespace uart
} // namespace ftl
