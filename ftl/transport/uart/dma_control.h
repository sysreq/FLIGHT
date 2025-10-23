#pragma once

#include "config.settings"
#include <array>
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <span>

struct uart_inst;
typedef struct uart_inst uart_inst_t;

namespace ftl_internal {

class DmaController {
public:
    DmaController() = default;
    ~DmaController() = default;

    DmaController(const DmaController&) = delete;
    DmaController& operator=(const DmaController&) = delete;
    DmaController(DmaController&&) = delete;
    DmaController& operator=(DmaController&&) = delete;

    void init(uart_inst_t* uart_inst);
    void process_rx_dma();

    size_t get_bytes_available() const;
    size_t read_from_circular_buffer(std::span<uint8_t> buffer);

    bool write_data(std::span<const uint8_t> data);
    bool is_write_busy();

private:
    uart_inst_t* uart_instance_ = nullptr;
    uint32_t uart_dreq_tx_ = 0;  // TX DMA request signal
    uint32_t uart_dreq_rx_ = 0;  // RX DMA request signal

    int dma_rx_chan_a_ = -1;
    int dma_rx_chan_b_ = -1;
    int dma_tx_chan_ = -1;

    alignas(4) volatile uint8_t rx_dma_buffer_a_[ftl_config::RX_DMA_CHUNK_SIZE]{};
    alignas(4) volatile uint8_t rx_dma_buffer_b_[ftl_config::RX_DMA_CHUNK_SIZE]{};

    std::atomic<uint32_t> rx_dma_a_read_pos_{0};
    std::atomic<uint32_t> rx_dma_b_read_pos_{0};
    
    std::atomic<uint32_t> current_active_rx_dma_{0};

    std::array<uint8_t, ftl_config::RX_CIRCULAR_BUFFER_SIZE> rx_circular_buffer_{};
    std::atomic<uint32_t> rx_circ_write_idx_{0};  // DMA writes here
    std::atomic<uint32_t> rx_circ_read_idx_{0};   // Application reads here

    alignas(4) std::array<uint8_t, ftl_config::TX_BUFFER_SIZE> tx_buffer_{};
    std::atomic<bool> tx_busy_{false};

    void setup_rx_dma_channel(int channel, volatile uint8_t* buffer, int chain_to_channel);
    void transfer_to_circular_buffer(volatile uint8_t* dma_buffer, uint32_t start_idx, uint32_t count);

    uint32_t get_active_dma_bytes_available() const;

    void check_active_dma_buffer();
    void check_for_dma_buffer_swap();
    void check_for_dma_stall();
};

} // namespace ftl_internal