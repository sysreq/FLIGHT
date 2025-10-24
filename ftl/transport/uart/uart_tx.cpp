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

// TX queue for outbound messages
CircularQueue<PoolHandle, ftl_config::TX_QUEUE_DEPTH, false> g_tx_queue;

// Currently transmitting handle (invalid if no transmission in progress)
PoolHandle g_current_tx_handle = MessagePoolType::INVALID;

// Device source ID
uint8_t g_source_id = 0;

// Statistics
uint32_t g_total_queued = 0;
uint32_t g_total_sent = 0;
uint32_t g_queue_full_drops = 0;
uint32_t g_peak_queue_depth = 0;

/**
 * @brief Build complete frame from payload in pool handle
 * 
 * Constructs framed message in a temporary buffer ready for DMA.
 * Frame format: [START] [LENGTH] [SOURCE] [PAYLOAD] [CRC16] [END]
 * 
 * @param handle Pool handle containing payload
 * @param frame_buffer Output buffer for complete frame
 * @param frame_size Output parameter for frame size
 * @return true if frame built successfully
 */
bool build_frame(PoolHandle handle, uint8_t* frame_buffer, size_t& frame_size) {
    uint8_t* payload_ptr = messages::g_message_pool.get_ptr<uint8_t>(handle);
    if (!payload_ptr) {
        return false;
    }
    
    // Payload layout in pool: [LENGTH][SOURCE][PAYLOAD_DATA...]
    uint8_t payload_length = payload_ptr[0];
    
    if (payload_length > ftl_config::MAX_PAYLOAD_SIZE) {
        return false;
    }
    
    // Build frame
    size_t idx = 0;
    
    // Start delimiter
    frame_buffer[idx++] = (ftl_config::START_DELIMITER >> 8) & 0xFF;
    frame_buffer[idx++] = ftl_config::START_DELIMITER & 0xFF;
    
    // Length
    frame_buffer[idx++] = payload_length;
    
    // Source ID (use the one from the handle, which was set during acquire)
    frame_buffer[idx++] = payload_ptr[1];
    
    // Payload (skip the LENGTH and SOURCE bytes from pool buffer)
    memcpy(&frame_buffer[idx], &payload_ptr[2], payload_length);
    
    // Calculate CRC over payload only
    uint16_t crc = crc16::calculate(&payload_ptr[2], payload_length);
    idx += payload_length;
    
    // CRC16
    frame_buffer[idx++] = (crc >> 8) & 0xFF;
    frame_buffer[idx++] = crc & 0xFF;
    
    // End delimiter
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

    // Acquire handle from pool
    PoolHandle handle = messages::g_message_pool.acquire();
    if (handle == MessagePoolType::INVALID) {
        return MessagePoolType::INVALID;
    }
    
    // Get buffer pointer
    uint8_t* buffer = messages::g_message_pool.get_ptr<uint8_t>(handle);
    if (!buffer) {
        messages::g_message_pool.release(handle);
        return MessagePoolType::INVALID;
    }
    
    // Store payload in pool buffer: [LENGTH][SOURCE][PAYLOAD_DATA...]
    buffer[0] = static_cast<uint8_t>(payload.size());
    buffer[1] = g_source_id;  // Fill in source ID
    memcpy(&buffer[2], payload.data(), payload.size());
    
    return handle;
}

bool enqueue_message_on_core0(PoolHandle handle) {
    if (handle == MessagePoolType::INVALID) {
        return false;
    }

    // Try to enqueue
    if (!g_tx_queue.enqueue(handle)) {
        // Queue full - release handle and increment drop counter
        messages::g_message_pool.release(handle);
        g_queue_full_drops++;
        return false;
    }
    
    // Successfully queued
    g_total_queued++;
    uint32_t current_depth = g_tx_queue.count();
    if (current_depth > g_peak_queue_depth) {
        g_peak_queue_depth = current_depth;
    }
    
    return true;
}

void process_tx_queue(ftl_internal::DmaController& dma_controller) {
    // Check if current transmission is complete
    if (g_current_tx_handle != MessagePoolType::INVALID) {
        if (!dma_controller.is_write_busy()) {
            // Transmission complete - release handle
            messages::g_message_pool.release(g_current_tx_handle);
            g_current_tx_handle = MessagePoolType::INVALID;
            g_total_sent++;
        } else {
            // Still transmitting - nothing to do
            return;
        }
    }
    
    // DMA is idle - check if we have queued messages
    if (g_tx_queue.is_empty()) {
        return;
    }
    
    // Dequeue next message
    PoolHandle handle;
    if (!g_tx_queue.dequeue(handle)) {
        return;
    }
    
    // Build frame
    uint8_t frame_buffer[ftl_config::MAX_MESSAGE_SIZE];
    size_t frame_size;
    
    if (!build_frame(handle, frame_buffer, frame_size)) {
        // Failed to build frame - release handle
        messages::g_message_pool.release(handle);
        return;
    }
    
    // Start DMA transfer
    if (dma_controller.write_data(std::span<const uint8_t>{frame_buffer, frame_size})) {
        // DMA started successfully - keep handle until transmission completes
        g_current_tx_handle = handle;
    } else {
        // Failed to start DMA - release handle
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
