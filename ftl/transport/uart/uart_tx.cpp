#include "internal/uart_tx.h"
#include "internal/uart_dma.h"
#include "core/ftl_api.h"
#include "util/allocator.h"
#include "util/cqueue.h"
#include "util/misc.h"
#include "pico/stdlib.h"
#include <cstring>

namespace ftl {
namespace messages {
    extern MessagePoolType g_message_pool;
}

namespace uart {
namespace internal_tx {

namespace {

CircularQueue<PoolHandle, ftl_config::TX_QUEUE_DEPTH, false> g_tx_queue;
PoolHandle g_current_tx_handle = MessagePoolType::INVALID;

uint8_t g_source_id = 0;

// Statistics
uint32_t g_total_queued = 0;
uint32_t g_total_sent = 0;
uint32_t g_queue_full_drops = 0;
uint32_t g_peak_queue_depth = 0;

bool build_frame(PoolHandle handle, uint8_t* frame_buffer, size_t& frame_size) {
    uint8_t* payload_ptr = messages::g_message_pool.get_ptr<uint8_t>(handle);
    if (!payload_ptr) {
        return false;
    }
    
    uint8_t payload_length = payload_ptr[0];
    
    if (payload_length > ftl_config::MAX_PAYLOAD_SIZE) {
        return false;
    }
    
    size_t idx = 0;
    
    frame_buffer[idx++] = (ftl_config::START_DELIMITER >> 8) & 0xFF;
    frame_buffer[idx++] = ftl_config::START_DELIMITER & 0xFF;
    frame_buffer[idx++] = payload_length;
    frame_buffer[idx++] = payload_ptr[1];
    
    memcpy(&frame_buffer[idx], &payload_ptr[2], payload_length);
    
    uint16_t crc = crc16::calculate(&payload_ptr[2], payload_length);
    idx += payload_length;
    
    frame_buffer[idx++] = (crc >> 8) & 0xFF;
    frame_buffer[idx++] = crc & 0xFF;
    
    frame_buffer[idx++] = (ftl_config::END_DELIMITER >> 8) & 0xFF;
    frame_buffer[idx++] = ftl_config::END_DELIMITER & 0xFF;
    
    frame_size = idx;
    return true;
}

} // anonymous namespace

void initialize(uint8_t source_id) {
    g_source_id = source_id;
    g_tx_queue.clear();
    g_current_tx_handle = MessagePoolType::INVALID;
    g_total_queued = 0;
    g_total_sent = 0;
    g_queue_full_drops = 0;
    g_peak_queue_depth = 0;
}

PoolHandle acquire_and_fill_message(std::span<const uint8_t> payload) {
    if (payload.empty() || payload.size() > ftl_config::MAX_PAYLOAD_SIZE) {
        return MessagePoolType::INVALID;
    }

    PoolHandle handle = messages::g_message_pool.acquire();
    if (handle == MessagePoolType::INVALID) {
        return MessagePoolType::INVALID;
    }
    
    uint8_t* buffer = messages::g_message_pool.get_ptr<uint8_t>(handle);
    if (!buffer) {
        messages::g_message_pool.release(handle);
        return MessagePoolType::INVALID;
    }
    
    buffer[0] = static_cast<uint8_t>(payload.size());
    buffer[1] = g_source_id;
    memcpy(&buffer[2], payload.data(), payload.size());
    
    return handle;
}

bool enqueue_message_on_core0(PoolHandle handle) {
    if (handle == MessagePoolType::INVALID) {
        return false;
    }

    if (!g_tx_queue.enqueue(handle)) {
        messages::g_message_pool.release(handle);
        g_queue_full_drops++;
        return false;
    }
    
    g_total_queued++;
    uint32_t current_depth = g_tx_queue.count();
    if (current_depth > g_peak_queue_depth) {
        g_peak_queue_depth = current_depth;
    }
    
    return true;
}

void process_tx_queue(ftl_internal::DmaController& dma_controller) {
    if (g_current_tx_handle != MessagePoolType::INVALID) {
        if (!dma_controller.is_write_busy()) {
            messages::g_message_pool.release(g_current_tx_handle);
            g_current_tx_handle = MessagePoolType::INVALID;
            g_total_sent++;
        } else {
            return;
        }
    }
    
    if (g_tx_queue.is_empty()) {
        return;
    }
    
    PoolHandle handle;
    if (!g_tx_queue.dequeue(handle)) {
        return;
    }
    
    uint8_t frame_buffer[ftl_config::MAX_MESSAGE_SIZE];
    size_t frame_size;
    
    if (!build_frame(handle, frame_buffer, frame_size)) {
        messages::g_message_pool.release(handle);
        return;
    }
    
    if (dma_controller.write_data(std::span<const uint8_t>{frame_buffer, frame_size})) {
        g_current_tx_handle = handle;
    } else {
        messages::g_message_pool.release(handle);
    }
}

bool is_ready() {
    return !g_tx_queue.is_full();
}

uint8_t get_source_id() {
    return g_source_id;
}

Statistics get_statistics() {
    return Statistics{
        g_total_queued,
        g_total_sent,
        g_queue_full_drops,
        g_tx_queue.count(),
        g_peak_queue_depth
    };
}

bool is_queue_empty() {
    return g_tx_queue.is_empty();
}

uint32_t get_queue_count() {
    return g_tx_queue.count();
}

} // namespace internal_tx
} // namespace uart
} // namespace ftl
