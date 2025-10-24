#include "core/ftl_api.h"
#include "transport/uart/uart_rx.h"
#include "transport/uart/uart_tx.h"
#include "transport/uart/dma_control.h"
#include "util/misc.h"
#include "ftl.settings"
#include "pico/stdlib.h"
#include "pico/printf.h"
#include "hardware/uart.h"

namespace ftl {
namespace messages {
    MessagePoolType g_message_pool;
}
}

namespace ftl {

namespace {

ftl_internal::DmaController g_dma_controller;
bool g_is_initialized = false;
uart_inst_t* g_uart_instance = nullptr;

constexpr uart_inst_t* get_uart_instance(ftl_config::UartId id) {
    return (id == ftl_config::UartId::Uart0) ? uart0 : uart1;
}

} // anonymous namespace

void initialize() {
    if (g_is_initialized) {
        return;
    }

    uint8_t source_id = device_id::get_device_id();
    g_uart_instance = get_uart_instance(ftl_config::UART_ID);

    uart_init(g_uart_instance, ftl_config::BAUD_RATE);

    gpio_set_function(ftl_config::TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(ftl_config::RX_PIN, GPIO_FUNC_UART);

    uart_set_hw_flow(g_uart_instance, false, false);

    uart_set_format(g_uart_instance,
                    ftl_config::DATA_BITS,
                    ftl_config::STOP_BITS,
                    UART_PARITY_NONE);
    
    uart_set_fifo_enabled(g_uart_instance, true);

    while (uart_is_readable(g_uart_instance)) {
        (void)uart_getc(g_uart_instance);
    }

    g_dma_controller.init(g_uart_instance);

    rx::initialize();
    tx::initialize(source_id);

    g_is_initialized = true;
    
    printf("FTL initialized with source ID: 0x%02X\n", source_id);
}

void poll() {
    if (!g_is_initialized) {
        return;
    }
    g_dma_controller.process_rx_dma();
    rx::process(g_dma_controller);
    
    tx::process_tx_queue(g_dma_controller);
}

bool has_msg() {
    if (!g_is_initialized) {
        return false;
    }
    return rx::has_message();
}

MessageHandle get_msg() {
    if (!g_is_initialized) {
        return MessageHandle{};
    }
    return rx::get_message();
}

bool send_msg(std::span<const uint8_t> payload) {
    if (!g_is_initialized || payload.empty()) {
        return false;
    }
    return tx::send_message(g_dma_controller, payload);
}

bool send_msg(std::string_view message) {
    if (!g_is_initialized || message.empty()) {
        return false;
    }
    return tx::send_message(g_dma_controller, message);
}

bool is_tx_ready() {
    if (!g_is_initialized) {
        return true;
    }
    return tx::is_ready();
}

uint8_t get_my_source_id() {
    return tx::get_source_id();
}

void get_stats(uint32_t& total_bytes_rx, uint32_t& total_messages_rx, 
               uint32_t& messages_queued, uint32_t& pool_allocated,
               uint32_t& crc_errors, uint32_t& framing_errors) {
    rx::Statistics stats = rx::get_statistics();
    
    total_bytes_rx = stats.total_bytes_received;
    total_messages_rx = stats.total_messages_received;
    messages_queued = rx::get_queue_count();
    pool_allocated = rx::get_pool_allocated_count();
    crc_errors = stats.crc_errors;
    framing_errors = stats.framing_errors;
}

void get_tx_stats(uint32_t& total_queued, uint32_t& total_sent,
                  uint32_t& queue_full_drops, uint32_t& current_queue_depth,
                  uint32_t& peak_queue_depth) {
    tx::Statistics stats = tx::get_statistics();
    
    total_queued = stats.total_messages_queued;
    total_sent = stats.total_messages_sent;
    queue_full_drops = stats.queue_full_drops;
    current_queue_depth = stats.current_queue_depth;
    peak_queue_depth = stats.peak_queue_depth;
}

uint32_t get_tx_queue_count() {
    return tx::get_queue_count();
}

bool is_tx_queue_empty() {
    return tx::is_queue_empty();
}

} // namespace ftl
