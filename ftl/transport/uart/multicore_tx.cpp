#include "transport/uart/multicore_tx.h"
#include "core/ftl_api.h"
#include "util/allocator.h"
#include "pico/multicore.h"
#include "pico/printf.h"
#include <cstring>

namespace ftl {
namespace messages {
    extern MessagePoolType g_message_pool;
}

namespace multicore {

namespace {

// Statistics
uint32_t g_core1_sent = 0;
uint32_t g_core1_fifo_drops = 0;
uint32_t g_core0_received = 0;

bool g_is_initialized = false;

/**
 * @brief FIFO message format
 * 
 * We send two 32-bit words through the FIFO:
 * Word 0: [pool_handle (8 bits)] [payload_length (8 bits)] [reserved (16 bits)]
 * Word 1: [magic (32 bits)] = 0xFTL_MSG (for validation)
 */
constexpr uint32_t FIFO_MAGIC = 0x46544C4D;  // "FTLM" in ASCII

struct FifoMessage {
    uint8_t handle;
    uint8_t length;
    uint16_t reserved;
};

/**
 * @brief Pack FIFO message into two 32-bit words
 */
inline void pack_fifo_message(uint32_t& word0, uint32_t& word1, 
                               uint8_t handle, uint8_t length) {
    word0 = (static_cast<uint32_t>(handle) << 0) |
            (static_cast<uint32_t>(length) << 8);
    word1 = FIFO_MAGIC;
}

/**
 * @brief Unpack FIFO message from two 32-bit words
 */
inline bool unpack_fifo_message(uint32_t word0, uint32_t word1,
                                 uint8_t& handle, uint8_t& length) {
    if (word1 != FIFO_MAGIC) {
        return false;  // Invalid magic
    }
    
    handle = static_cast<uint8_t>(word0 & 0xFF);
    length = static_cast<uint8_t>((word0 >> 8) & 0xFF);
    return true;
}

} // anonymous namespace

void initialize() {
    if (g_is_initialized) {
        return;
    }
    
    // Nothing special needed - Pico SDK initializes FIFO automatically
    // Just clear statistics
    g_core1_sent = 0;
    g_core1_fifo_drops = 0;
    g_core0_received = 0;
    
    g_is_initialized = true;
    
    printf("Multicore TX initialized\n");
}

bool send_from_core1(std::span<const uint8_t> payload) {
    if (!g_is_initialized) {
        return false;
    }
    
    if (payload.empty() || payload.size() > ftl_config::MAX_PAYLOAD_SIZE) {
        return false;
    }
    
    // Acquire handle from pool
    PoolHandle handle = messages::g_message_pool.acquire();
    if (handle == MessagePoolType::INVALID) {
        return false;
    }
    
    // Get buffer pointer
    uint8_t* buffer = messages::g_message_pool.get_ptr<uint8_t>(handle);
    if (!buffer) {
        messages::g_message_pool.release(handle);
        return false;
    }
    
    // Store payload in pool buffer: [LENGTH][SOURCE][PAYLOAD_DATA...]
    // Note: SOURCE will be filled in by Core 0
    buffer[0] = static_cast<uint8_t>(payload.size());
    buffer[1] = 0;  // Source ID placeholder
    memcpy(&buffer[2], payload.data(), payload.size());
    
    // Try to push to FIFO (non-blocking)
    uint32_t word0, word1;
    pack_fifo_message(word0, word1, handle, static_cast<uint8_t>(payload.size()));
    
    if (multicore_fifo_wready()) {
        // FIFO has space - push both words
        multicore_fifo_push_blocking(word0);  // First word
        multicore_fifo_push_blocking(word1);  // Second word (magic)
        
        g_core1_sent++;
        return true;
    } else {
        // FIFO full - release handle and drop
        messages::g_message_pool.release(handle);
        g_core1_fifo_drops++;
        return false;
    }
}

bool send_from_core1(std::string_view message) {
    return send_from_core1(std::span<const uint8_t>{
        reinterpret_cast<const uint8_t*>(message.data()),
        message.size()
    });
}

void process_core1_messages() {
    if (!g_is_initialized) {
        return;
    }
    
    // Process all available messages from FIFO
    while (multicore_fifo_rvalid()) {
        // Read two words from FIFO
        uint32_t word0 = multicore_fifo_pop_blocking();
        
        if (!multicore_fifo_rvalid()) {
            printf("Multicore FIFO: Incomplete message (missing word 1)\n");
            break;
        }
        
        uint32_t word1 = multicore_fifo_pop_blocking();
        
        // Unpack and validate
        uint8_t handle;
        uint8_t length;
        
        if (!unpack_fifo_message(word0, word1, handle, length)) {
            printf("Multicore FIFO: Invalid magic (0x%08X)\n", word1);
            continue;
        }
        
        // Validate handle
        if (!messages::g_message_pool.is_valid(handle)) {
            printf("Multicore FIFO: Invalid handle %u\n", handle);
            continue;
        }
        
        // Get buffer and queue for transmission
        uint8_t* buffer = messages::g_message_pool.get_ptr<uint8_t>(handle);
        if (buffer) {
            // Buffer format: [LENGTH][SOURCE][PAYLOAD...]
            // Create span of the payload portion
            std::span<const uint8_t> payload(&buffer[2], length);
            
            // Queue for transmission on Core 0
            if (ftl::send_msg(payload)) {
                g_core0_received++;
            } else {
                // Failed to queue - release handle
                printf("Multicore: Failed to queue message from Core 1\n");
                messages::g_message_pool.release(handle);
            }
        } else {
            // Invalid buffer - release handle
            messages::g_message_pool.release(handle);
        }
    }
}

Statistics get_statistics() {
    return Statistics{
        g_core1_sent,
        g_core1_fifo_drops,
        g_core0_received
    };
}

bool is_core1_ready() {
    return multicore_fifo_wready();
}

} // namespace multicore
} // namespace ftl
