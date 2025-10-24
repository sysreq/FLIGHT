#include "uart.h"
#include "internal/uart_dma.h"
#include "internal/uart_tx.h"
#include "internal/uart_rx.h"
#include "internal/uart_multicore.h"
#include "core/ftl_api.h"

#include "pico/stdlib.h"
#include "pico/printf.h"

namespace ftl {
namespace messages {
    extern MessagePoolType g_message_pool;
}

namespace uart {
namespace {
    // The UART module owns the DmaController
    ftl_internal::DmaController g_dma_controller;
    
    // Store initialization state
    uint8_t g_source_id = 0;
    uint8_t g_init_core = 0;
    bool g_is_initialized = false;
}

// --- Public API Implementation ---

void initialize(uart_inst_t* uart_inst, uint8_t source_id) {
    if (g_is_initialized) {
        printf("UART already initialized\n");
        return;
    }

    g_source_id = source_id;
    g_init_core = get_core_num();
    
    // Initialize DMA controller
    g_dma_controller.init(uart_inst);
    
    // Initialize internal subsystems
    internal_rx::initialize();
    internal_tx::initialize(source_id);
    internal_multicore::initialize();
    
    g_is_initialized = true;
    
    printf("UART transport initialized on Core %u (ID: %u)\n", g_init_core, source_id);
}

void poll() {
    if (!g_is_initialized) {
        return;
    }

    // 1. Process RX DMA buffers
    g_dma_controller.process_rx_dma();
    
    // 2. Feed new bytes into the RX state machine
    internal_rx::process(g_dma_controller);
    
    // 3. Process Core 1 -> Core 0 FIFO messages
    //    This directly enqueues handles to the TX queue
    internal_multicore::process_fifo_messages();
    
    // 4. Process Core 0 TX queue
    //    This dequeues handles and starts DMA transfers
    internal_tx::process_tx_queue(g_dma_controller);
}

bool send_message(std::span<const uint8_t> payload) {
    if (!g_is_initialized) {
        return false;
    }

    if (payload.empty() || payload.size() > ftl_config::MAX_PAYLOAD_SIZE) {
        return false;
    }

    // Step 1: Acquire a message pool handle and copy payload
    // This is common to both cores
    PoolHandle handle = internal_tx::acquire_and_fill_message(payload);
    if (handle == MessagePoolType::INVALID) {
        return false; // Pool empty
    }

    if (get_core_num() == g_init_core) {
        // --- Core 0 Path ---
        // Directly enqueue the handle to the TX queue
        return internal_tx::enqueue_message_on_core0(handle);
    } else {
        // --- Core 1 Path ---
        // Send the handle via the FIFO
        bool success = internal_multicore::send_from_core1(handle, 
                                                           static_cast<uint8_t>(payload.size()));
        if (!success) {
            // FIFO was full - release the handle since we couldn't send it
            ftl::messages::g_message_pool.release(handle);
        }
        return success;
    }
}

bool send_message(std::string_view message) {
    return send_message(std::span<const uint8_t>{
        reinterpret_cast<const uint8_t*>(message.data()), 
        message.size()
    });
}

bool has_message() {
    if (!g_is_initialized) {
        return false;
    }
    return internal_rx::has_message();
}

MessageHandle get_message() {
    if (!g_is_initialized) {
        return MessageHandle{};
    }
    return internal_rx::get_message();
}

bool is_tx_ready() {
    if (!g_is_initialized) {
        return false;
    }
    return internal_tx::is_ready();
}

bool is_core1_tx_ready() {
    if (!g_is_initialized) {
        return false;
    }
    return internal_multicore::is_core1_ready();
}

TxStatistics get_tx_statistics() {
    if (!g_is_initialized) {
        return TxStatistics{};
    }
    
    auto internal_stats = internal_tx::get_statistics();
    return TxStatistics{
        internal_stats.total_messages_queued,
        internal_stats.total_messages_sent,
        internal_stats.queue_full_drops,
        internal_stats.current_queue_depth,
        internal_stats.peak_queue_depth
    };
}

RxStatistics get_rx_statistics() {
    if (!g_is_initialized) {
        return RxStatistics{};
    }
    
    auto internal_stats = internal_rx::get_statistics();
    return RxStatistics{
        internal_stats.total_bytes_received,
        internal_stats.total_messages_received,
        internal_stats.crc_errors,
        internal_stats.framing_errors
    };
}

MulticoreStatistics get_multicore_statistics() {
    if (!g_is_initialized) {
        return MulticoreStatistics{};
    }
    
    auto internal_stats = internal_multicore::get_statistics();
    return MulticoreStatistics{
        internal_stats.core1_messages_sent,
        internal_stats.core1_fifo_full_drops,
        internal_stats.core0_messages_received,
        internal_stats.core0_tx_queue_drops
    };
}

} // namespace uart
} // namespace ftl
