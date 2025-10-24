#include "internal/uart_multicore.h"
#include "internal/uart_tx.h"
#include "core/ftl_api.h"
#include "pico/multicore.h"
#include "pico/printf.h"

namespace ftl {
namespace messages {
    extern MessagePoolType g_message_pool;
}

namespace uart {
namespace internal_multicore {

namespace {

// Statistics
uint32_t g_core1_sent = 0;
uint32_t g_core1_fifo_drops = 0;
uint32_t g_core0_received = 0;
uint32_t g_core0_tx_queue_drops = 0;

bool g_is_initialized = false;

/**
 * @brief FIFO message format
 * 
 * We send two 32-bit words through the FIFO:
 * Word 0: [pool_handle (8 bits)] [payload_length (8 bits)] [reserved (16 bits)]
 * Word 1: [magic (32 bits)] = 0x46544C4D (for validation)
 */
constexpr uint32_t FIFO_MAGIC = 0x46544C;  // "FTL" in ASCII

inline void pack_fifo_message(uint32_t& word, uint8_t handle) {
    word = static_cast<uint32_t>(FIFO_MAGIC << 8 | handle);
}

inline bool unpack_fifo_message(uint32_t word, uint8_t& handle) {
    if ((word >> 8) != FIFO_MAGIC) {
        return false;  // Invalid magic
    }
    handle = static_cast<uint8_t>(word & 0xFF);
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
    g_core0_tx_queue_drops = 0;
    
    g_is_initialized = true;
    
    printf("Multicore TX initialized\n");
}

bool send_from_core1(PoolHandle handle, uint8_t length) {
    if (!g_is_initialized || handle == MessagePoolType::INVALID) {
        return false;
    }
    
    if (length == 0 || length > ftl_config::MAX_PAYLOAD_SIZE) {
        return false;
    }
    
    uint32_t word;
    pack_fifo_message(word, handle);
    
    if (multicore_fifo_wready()) {
        multicore_fifo_push_blocking(word); 
        g_core1_sent++;
        return true;
    } else {
        g_core1_fifo_drops++;
        return false;
    }
}

void process_fifo_messages() {
    if (!g_is_initialized) {
        return;
    }
    
    while (multicore_fifo_rvalid()) {
        uint32_t word = multicore_fifo_pop_blocking();
                       
        uint8_t handle;
        if (!unpack_fifo_message(word, handle)) {
            printf("Multicore FIFO: Invalid magic (0x%08X)\n", word);
            continue;
        }
        
        if (!messages::g_message_pool.is_valid(handle)) {
            printf("Multicore FIFO: Invalid handle %u\n", handle);
            continue;
        }
        
        if (internal_tx::enqueue_message_on_core0(handle)) {
            g_core0_received++;
        } else {
            messages::g_message_pool.release(handle);
            g_core0_tx_queue_drops++;
            printf("Multicore: Core 0 TX queue full, dropping message\n");
        }
    }
}

Statistics get_statistics() {
    return Statistics{
        g_core1_sent,
        g_core1_fifo_drops,
        g_core0_received,
        g_core0_tx_queue_drops
    };
}

bool is_core1_ready() {
    return multicore_fifo_wready();
}

} // namespace internal_multicore
} // namespace uart
} // namespace ftl
