#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "hardware/clocks.h"
#include "hardware/spi.h"
#include "hardware/structs/clocks.h"

#include "pico.h"
#include "pico/mutex.h"
#include "pico/platform.h"
#include "pico/stdlib.h"

#include "delays.h"
#include "hw_config.h"

#include "my_spi.h"

#ifndef USE_DBG_PRINTF
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif

static bool chk_spi(spi_t *spi_p) {
    uint32_t sr = spi_get_const_hw(spi_p->hw_inst)->sr;
    const uint32_t bad_bits = SPI_SSPSR_BSY_BITS | SPI_SSPSR_RFF_BITS | SPI_SSPSR_RNE_BITS;
    const uint32_t required_bits = SPI_SSPSR_TNF_BITS | SPI_SSPSR_TFE_BITS;
    return (sr & bad_bits) == 0 && (sr & required_bits) == required_bits;
}

static bool chk_dma(uint chn) {
    return !(dma_hw->ch[chn].ctrl_trig & (DMA_CH0_CTRL_TRIG_AHB_ERROR_BITS |
                                          DMA_CH0_CTRL_TRIG_READ_ERROR_BITS |
                                          DMA_CH0_CTRL_TRIG_WRITE_ERROR_BITS |
                                          DMA_CH0_CTRL_TRIG_BUSY_BITS));
}

static bool chk_dmas(spi_t *spi_p) {
    bool tx_ok = chk_dma(spi_p->tx_dma);
    bool rx_ok = chk_dma(spi_p->rx_dma);
    return tx_ok && rx_ok;
}

void spi_transfer_start(spi_t *spi_p, const uint8_t *tx, uint8_t *rx, size_t length) {
    assert(spi_p);
    assert(tx || rx);

    if (tx) {
        channel_config_set_read_increment(&spi_p->tx_dma_cfg, true);
    } else {
        static const uint8_t dummy __attribute__((section(".time_critical."))) = SPI_FILL_CHAR;
        tx = &dummy;
        channel_config_set_read_increment(&spi_p->tx_dma_cfg, false);
    }

    if (rx) {
        channel_config_set_write_increment(&spi_p->rx_dma_cfg, true);
    } else {
        static uint8_t dummy = 0xA5;
        rx = &dummy;
        channel_config_set_write_increment(&spi_p->rx_dma_cfg, false);
    }

    dma_channel_configure(spi_p->tx_dma, &spi_p->tx_dma_cfg,
                          &spi_get_hw(spi_p->hw_inst)->dr,  // write address
                          tx,                               // read address
                          length,                           // element count (each element is of
                                                            // size transfer_data_size)
                          false);                           // start
    dma_channel_configure(spi_p->rx_dma, &spi_p->rx_dma_cfg,
                          rx,                               // write address
                          &spi_get_hw(spi_p->hw_inst)->dr,  // read address
                          length,                           // element count (each element is of
                                                            // size transfer_data_size)
                          false);                           // start

    dma_start_channel_mask((1u << spi_p->tx_dma) | (1u << spi_p->rx_dma));
}

uint32_t calculate_transfer_time_ms(spi_t *spi_p, uint32_t bytes) {
    uint32_t total_bits = bytes * 8;
    uint32_t baud_rate = spi_get_baudrate(spi_p->hw_inst);
    float transfer_time_sec = (double)total_bits / baud_rate;
    float transfer_time_ms = transfer_time_sec * 1000;

    transfer_time_ms *= 1.5f;
    transfer_time_ms += 4.0f;

    return (uint32_t)transfer_time_ms;
}

bool __not_in_flash_func(spi_transfer_wait_complete)(spi_t *spi_p, uint32_t timeout_ms) {
    assert(spi_p);
    bool timed_out = false;

    uint32_t start = millis();
    while ((dma_channel_is_busy(spi_p->rx_dma) || dma_channel_is_busy(spi_p->tx_dma)) &&
           millis() - start < timeout_ms)
        tight_loop_contents();

    timed_out = dma_channel_is_busy(spi_p->rx_dma) || dma_channel_is_busy(spi_p->tx_dma);

    if (!timed_out) {
        start = millis();
        while (spi_is_busy(spi_p->hw_inst) && millis() - start < timeout_ms)
            tight_loop_contents();

        timed_out = spi_is_busy(spi_p->hw_inst);
    }

    bool spi_ok = chk_spi(spi_p);

    if (timed_out || !spi_ok) {
        chk_dmas(spi_p);
        dma_channel_abort(spi_p->rx_dma);
        dma_channel_abort(spi_p->tx_dma);
    }

    return !(timed_out || !spi_ok);
}

bool __not_in_flash_func(spi_transfer)(spi_t *spi_p, const uint8_t *tx, uint8_t *rx,
                                       size_t length) {
    spi_transfer_start(spi_p, tx, rx, length);
    uint32_t timeout = calculate_transfer_time_ms(spi_p, length);
    return spi_transfer_wait_complete(spi_p, timeout);
}

bool my_spi_init(spi_t *spi_p) {
    auto_init_mutex(my_spi_init_mutex);
    mutex_enter_blocking(&my_spi_init_mutex);
    if (!spi_p->initialized) {
        if (!mutex_is_initialized(&spi_p->mutex)) mutex_init(&spi_p->mutex);
        spi_lock(spi_p);

        if (!spi_p->hw_inst) spi_p->hw_inst = spi0;
        if (!spi_p->baud_rate) spi_p->baud_rate = clock_get_hz(clk_sys) / 12;

        spi_init(spi_p->hw_inst, 100 * 1000);

        switch (spi_p->spi_mode) {
            case 0:
                spi_set_format(spi_p->hw_inst, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
                break;
            case 1:
                spi_set_format(spi_p->hw_inst, 8, SPI_CPOL_0, SPI_CPHA_1, SPI_MSB_FIRST);
                break;
            case 2:
                spi_set_format(spi_p->hw_inst, 8, SPI_CPOL_1, SPI_CPHA_0, SPI_MSB_FIRST);
                break;
            case 3:
                spi_set_format(spi_p->hw_inst, 8, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST);
                break;
            default:
                spi_set_format(spi_p->hw_inst, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
                break;
        }
        gpio_set_function(spi_p->miso_gpio, GPIO_FUNC_SPI);
        gpio_set_function(spi_p->mosi_gpio, GPIO_FUNC_SPI);
        gpio_set_function(spi_p->sck_gpio, GPIO_FUNC_SPI);
        gpio_set_slew_rate(spi_p->sck_gpio, GPIO_SLEW_RATE_FAST);

        if (spi_p->set_drive_strength) {
            gpio_set_drive_strength(spi_p->mosi_gpio, spi_p->mosi_gpio_drive_strength);
            gpio_set_drive_strength(spi_p->sck_gpio, spi_p->sck_gpio_drive_strength);
        }

        if (!spi_p->no_miso_gpio_pull_up) gpio_pull_up(spi_p->miso_gpio);

        if (spi_p->use_static_dma_channels) {
            dma_channel_claim(spi_p->tx_dma);
            dma_channel_claim(spi_p->rx_dma);
        } else {
            spi_p->tx_dma = dma_claim_unused_channel(true);
            spi_p->rx_dma = dma_claim_unused_channel(true);
        }
        spi_p->tx_dma_cfg = dma_channel_get_default_config(spi_p->tx_dma);
        spi_p->rx_dma_cfg = dma_channel_get_default_config(spi_p->rx_dma);
        channel_config_set_transfer_data_size(&spi_p->tx_dma_cfg, DMA_SIZE_8);
        channel_config_set_transfer_data_size(&spi_p->rx_dma_cfg, DMA_SIZE_8);

        channel_config_set_dreq(&spi_p->tx_dma_cfg, spi_get_dreq(spi_p->hw_inst, true));
        channel_config_set_write_increment(&spi_p->tx_dma_cfg, false);

        channel_config_set_dreq(&spi_p->rx_dma_cfg, spi_get_dreq(spi_p->hw_inst, false));
        channel_config_set_read_increment(&spi_p->rx_dma_cfg, false);

        spi_p->initialized = true;
        spi_unlock(spi_p);
    }
    mutex_exit(&my_spi_init_mutex);
    return true;
}