#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "pico/stdlib.h"
#include "pico/mutex.h"
#include "pico/types.h"

#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/spi.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SPI_FILL_CHAR (0xFF)

typedef struct spi_t {
    spi_inst_t *hw_inst;
    uint miso_gpio;
    uint mosi_gpio;
    uint sck_gpio;
    uint baud_rate;
    uint spi_mode;
    
    bool no_miso_gpio_pull_up;
    bool set_drive_strength;
    enum gpio_drive_strength mosi_gpio_drive_strength;
    enum gpio_drive_strength sck_gpio_drive_strength;

    bool use_static_dma_channels;
    uint tx_dma;
    uint rx_dma;

    dma_channel_config tx_dma_cfg;
    dma_channel_config rx_dma_cfg;
    mutex_t mutex;    
    bool initialized;  
} spi_t;

void spi_transfer_start(spi_t *spi_p, const uint8_t *tx, uint8_t *rx, size_t length);
uint32_t calculate_transfer_time_ms(spi_t *spi_p, uint32_t bytes);
bool spi_transfer_wait_complete(spi_t *spi_p, uint32_t timeout_ms);
bool spi_transfer(spi_t *spi_p, const uint8_t *tx, uint8_t *rx, size_t length);
bool my_spi_init(spi_t *spi_p);

static inline void spi_lock(spi_t *spi_p) {
    mutex_enter_blocking(&spi_p->mutex);
}
static inline void spi_unlock(spi_t *spi_p) {
    mutex_exit(&spi_p->mutex);
}

#ifdef __cplusplus
}
#endif
