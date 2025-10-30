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
    ftl_internal::DmaController g_dma_controller;
    
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
    
    g_dma_controller.init(uart_inst);
    
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

    g_dma_controller.process_rx_dma();
    internal_rx::process(g_dma_controller);
    internal_multicore::process_fifo_messages();
    internal_tx::process_tx_queue(g_dma_controller);
}

bool send_message(std::span<const uint8_t> payload) {
    if (!g_is_initialized) {
        return false;
    }

    if (payload.empty() || payload.size() > ftl_config::MAX_PAYLOAD_SIZE) {
        return false;
    }

    PoolHandle handle = internal_tx::acquire_and_fill_message(payload);
    if (handle == MessagePoolType::INVALID) {
        return false; // Pool empty
    }

    if (get_core_num() == g_init_core) {
        return internal_tx::enqueue_message_on_core0(handle);
    } else {
        bool success = internal_multicore::send_from_core1(handle, static_cast<uint8_t>(payload.size()));
        if (!success) {
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
} // namespace uart
} // namespace ftl
