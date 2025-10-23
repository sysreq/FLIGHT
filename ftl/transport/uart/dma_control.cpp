#include "transport/uart/dma_control.h"

#include "pico/printf.h"
#include "hardware/uart.h"
#include "hardware/dma.h"
#include <algorithm>
#include <cstring>

namespace ftl_internal {

void DmaController::init(uart_inst_t* uart_inst) {
    uart_instance_ = uart_inst;
    uart_dreq_tx_ = uart_get_dreq(uart_instance_, true);   // TX DREQ
    uart_dreq_rx_ = uart_get_dreq(uart_instance_, false);  // RX DREQ

    dma_rx_chan_a_ = dma_claim_unused_channel(true);
    dma_rx_chan_b_ = dma_claim_unused_channel(true);
    dma_tx_chan_ = dma_claim_unused_channel(true);

    dma_channel_config tx_cfg = dma_channel_get_default_config(dma_tx_chan_);
    channel_config_set_transfer_data_size(&tx_cfg, DMA_SIZE_8);
    channel_config_set_read_increment(&tx_cfg, true);   // Read from buffer
    channel_config_set_write_increment(&tx_cfg, false); // Write to fixed UART register
    channel_config_set_dreq(&tx_cfg, uart_dreq_tx_);
    dma_channel_set_config(dma_tx_chan_, &tx_cfg, false);

    setup_rx_dma_channel(dma_rx_chan_a_, rx_dma_buffer_a_, dma_rx_chan_b_);
    setup_rx_dma_channel(dma_rx_chan_b_, rx_dma_buffer_b_, dma_rx_chan_a_);

    rx_dma_a_read_pos_.store(0, std::memory_order_relaxed);
    rx_dma_b_read_pos_.store(0, std::memory_order_relaxed);
    rx_circ_write_idx_.store(0, std::memory_order_relaxed);
    rx_circ_read_idx_.store(0, std::memory_order_relaxed);
    tx_busy_.store(false, std::memory_order_relaxed);
    current_active_rx_dma_.store(0, std::memory_order_relaxed);

    dma_channel_start(dma_rx_chan_a_);
}

void DmaController::process_rx_dma() {
    check_active_dma_buffer();
    check_for_dma_buffer_swap();
    check_for_dma_stall();
}

size_t DmaController::get_bytes_available() const {
    const uint32_t write_idx = rx_circ_write_idx_.load(std::memory_order_acquire);
    const uint32_t read_idx = rx_circ_read_idx_.load(std::memory_order_relaxed);
    return (write_idx - read_idx) & (ftl_config::RX_CIRCULAR_BUFFER_SIZE - 1);
}

size_t DmaController::read_from_circular_buffer(std::span<uint8_t> buffer) {
    uint32_t read_idx = rx_circ_read_idx_.load(std::memory_order_relaxed);
    const uint32_t write_idx = rx_circ_write_idx_.load(std::memory_order_acquire);

    const size_t available = (write_idx - read_idx) & (ftl_config::RX_CIRCULAR_BUFFER_SIZE - 1);
    const size_t bytes_to_read = std::min(available, buffer.size());

    for (size_t i = 0; i < bytes_to_read; ++i) {
        buffer[i] = rx_circular_buffer_[read_idx];
        read_idx = (read_idx + 1) & (ftl_config::RX_CIRCULAR_BUFFER_SIZE - 1);
    }

    rx_circ_read_idx_.store(read_idx, std::memory_order_release);
    return bytes_to_read;
}

bool DmaController::write_data(std::span<const uint8_t> data) {
    if(data.size() > ftl_config::TX_BUFFER_SIZE) {
        return false;
    }
    
    uint32_t loop_counter = 0;
    while(true) {
        if (!is_write_busy()) 
            break;
        loop_counter++;
        if(loop_counter > 1000) {
            printf("TX DMA busy timeout\n");
            return false;
        }  
    }

    memcpy(tx_buffer_.data(), data.data(), data.size());

    tx_busy_.store(true, std::memory_order_relaxed);

    dma_channel_set_read_addr(dma_tx_chan_, tx_buffer_.data(), false);
    dma_channel_set_write_addr(dma_tx_chan_, &uart_get_hw(uart_instance_)->dr, false);
    dma_channel_set_trans_count(dma_tx_chan_, data.size(), true); // Start transfer

    return true;
}

bool DmaController::is_write_busy() {
    if (!tx_busy_.load(std::memory_order_relaxed)) {
        return false;
    }

    if (!dma_channel_is_busy(dma_tx_chan_)) {
        tx_busy_.store(false, std::memory_order_relaxed);
        return false;
    }

    return true;
}

void DmaController::setup_rx_dma_channel(int channel, volatile uint8_t* buffer, int chain_to_channel) {
    dma_channel_config cfg = dma_channel_get_default_config(channel);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_8);
    channel_config_set_read_increment(&cfg, false);  // Read from fixed UART register
    channel_config_set_write_increment(&cfg, true);  // Write to buffer
    channel_config_set_dreq(&cfg, uart_dreq_rx_);
    channel_config_set_chain_to(&cfg, chain_to_channel);  // Chain to other buffer when done

    dma_channel_configure(
        channel,
        &cfg,
        (void*)buffer,                          // Destination buffer
        &uart_get_hw(uart_instance_)->dr,      // Source: UART data register
        ftl_config::RX_DMA_CHUNK_SIZE,        // Transfer count
        false                                   // Don't start yet
    );
}

void DmaController::transfer_to_circular_buffer(volatile uint8_t* dma_buffer, uint32_t start_idx, uint32_t count) {
    uint32_t write_idx = rx_circ_write_idx_.load(std::memory_order_acquire);
    uint32_t read_idx = rx_circ_read_idx_.load(std::memory_order_acquire);

    for (uint32_t i = 0; i < count; ++i) {
        const uint8_t byte = dma_buffer[start_idx + i];
        const uint32_t next_write_idx = (write_idx + 1) & (ftl_config::RX_CIRCULAR_BUFFER_SIZE - 1);

        if (next_write_idx == read_idx) {
            printf("RX circular buffer overflow - dropping oldest data");
            read_idx = (read_idx + 1) & (ftl_config::RX_CIRCULAR_BUFFER_SIZE - 1);
        }

        rx_circular_buffer_[write_idx] = byte;
        write_idx = next_write_idx;
    }

    rx_circ_write_idx_.store(write_idx, std::memory_order_release);
    rx_circ_read_idx_.store(read_idx, std::memory_order_release);
}

uint32_t DmaController::get_active_dma_bytes_available() const {
    const bool is_chan_a_active = (current_active_rx_dma_.load(std::memory_order_relaxed) == 0);
    const int active_channel = is_chan_a_active ? dma_rx_chan_a_ : dma_rx_chan_b_;

    const uint32_t remaining_transfers = dma_channel_hw_addr(active_channel)->transfer_count;
    const uint32_t transferred = ftl_config::RX_DMA_CHUNK_SIZE - remaining_transfers;

    const uint32_t already_read_pos = is_chan_a_active ?
                                      rx_dma_a_read_pos_.load(std::memory_order_acquire) :
                                      rx_dma_b_read_pos_.load(std::memory_order_acquire);

    if (transferred > already_read_pos) {
        return transferred - already_read_pos;
    }
    return 0;
}

void DmaController::check_active_dma_buffer() {
    const uint32_t available = get_active_dma_bytes_available();
    if (available == 0) {
        return;
    }

    if (current_active_rx_dma_.load(std::memory_order_relaxed) == 0) {
        const uint32_t read_pos = rx_dma_a_read_pos_.load(std::memory_order_relaxed);
        transfer_to_circular_buffer(rx_dma_buffer_a_, read_pos, available);
        rx_dma_a_read_pos_.store(read_pos + available, std::memory_order_release);
    } else {
        const uint32_t read_pos = rx_dma_b_read_pos_.load(std::memory_order_relaxed);
        transfer_to_circular_buffer(rx_dma_buffer_b_, read_pos, available);
        rx_dma_b_read_pos_.store(read_pos + available, std::memory_order_release);
    }
}

void DmaController::check_for_dma_buffer_swap() {
    const bool is_chan_a_active = (current_active_rx_dma_.load(std::memory_order_relaxed) == 0);
    const int active_channel = is_chan_a_active ? dma_rx_chan_a_ : dma_rx_chan_b_;
    const int inactive_channel = is_chan_a_active ? dma_rx_chan_b_ : dma_rx_chan_a_;

    if (dma_channel_is_busy(inactive_channel) && !dma_channel_is_busy(active_channel)) {        
        if (is_chan_a_active) {
            const uint32_t read_pos = rx_dma_a_read_pos_.load(std::memory_order_relaxed);
            if (read_pos < ftl_config::RX_DMA_CHUNK_SIZE) {
                transfer_to_circular_buffer(rx_dma_buffer_a_, read_pos, 
                                           ftl_config::RX_DMA_CHUNK_SIZE - read_pos);
            }
            
            rx_dma_a_read_pos_.store(0, std::memory_order_relaxed);
            dma_channel_set_write_addr(dma_rx_chan_a_, (void*)rx_dma_buffer_a_, false);
            dma_channel_set_trans_count(dma_rx_chan_a_, ftl_config::RX_DMA_CHUNK_SIZE, false);
        } else {
            const uint32_t read_pos = rx_dma_b_read_pos_.load(std::memory_order_relaxed);
            if (read_pos < ftl_config::RX_DMA_CHUNK_SIZE) {
                transfer_to_circular_buffer(rx_dma_buffer_b_, read_pos, 
                                           ftl_config::RX_DMA_CHUNK_SIZE - read_pos);
            }
            
            rx_dma_b_read_pos_.store(0, std::memory_order_relaxed);
            dma_channel_set_write_addr(dma_rx_chan_b_, (void*)rx_dma_buffer_b_, false);
            dma_channel_set_trans_count(dma_rx_chan_b_, ftl_config::RX_DMA_CHUNK_SIZE, false);
        }

        current_active_rx_dma_.store(1 - current_active_rx_dma_.load(std::memory_order_relaxed), 
                                    std::memory_order_relaxed);
    }
}

void DmaController::check_for_dma_stall() {
    if (!dma_channel_is_busy(dma_rx_chan_a_) && !dma_channel_is_busy(dma_rx_chan_b_)) {
        printf("DMA stall detected - recovering");
        
        const uint32_t read_pos_a = rx_dma_a_read_pos_.load(std::memory_order_relaxed);
        if (read_pos_a < ftl_config::RX_DMA_CHUNK_SIZE) {
            transfer_to_circular_buffer(rx_dma_buffer_a_, read_pos_a, 
                                       ftl_config::RX_DMA_CHUNK_SIZE - read_pos_a);
        }
        
        const uint32_t read_pos_b = rx_dma_b_read_pos_.load(std::memory_order_relaxed);
        if (read_pos_b < ftl_config::RX_DMA_CHUNK_SIZE) {
            transfer_to_circular_buffer(rx_dma_buffer_b_, read_pos_b, 
                                       ftl_config::RX_DMA_CHUNK_SIZE - read_pos_b);
        }

        rx_dma_a_read_pos_.store(0, std::memory_order_relaxed);
        rx_dma_b_read_pos_.store(0, std::memory_order_relaxed);
        current_active_rx_dma_.store(0, std::memory_order_relaxed);

        setup_rx_dma_channel(dma_rx_chan_a_, rx_dma_buffer_a_, dma_rx_chan_b_);
        setup_rx_dma_channel(dma_rx_chan_b_, rx_dma_buffer_b_, dma_rx_chan_a_);
        dma_channel_start(dma_rx_chan_a_);
    }
}

} // namespace ftl_internal