#pragma once

#include <stdint.h>

#include "pico/stdlib.h"

#include "delays.h"
#include "my_spi.h"
#include "sd_card.h"

#ifdef NDEBUG
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif

#define SD_COMMAND                    2000
#define SD_COMMAND_RETRIES            3 
#define SD_SPI_READ                   1000
#define SD_SPI_WRITE                  1000
#define SD_SPI_WRITE_READ             1000

#ifdef __cplusplus
extern "C" {
#endif

void sd_spi_go_low_frequency(sd_card_t *this);
void sd_spi_go_high_frequency(sd_card_t *this);
void sd_spi_send_initializing_sequence(sd_card_t *sd_card_p);

static inline uint8_t sd_spi_read(sd_card_t *sd_card_p) {
    uint8_t received = SPI_FILL_CHAR;
    uint32_t start = millis();
    while (!spi_is_writable(sd_card_p->spi_if_p->spi->hw_inst) &&
           millis() - start < SD_SPI_READ)
        tight_loop_contents();
    assert(spi_is_writable(sd_card_p->spi_if_p->spi->hw_inst));
    int num = spi_read_blocking(sd_card_p->spi_if_p->spi->hw_inst, SPI_FILL_CHAR, &received, 1);
    assert(1 == num);
    return received;
}

static inline void sd_spi_write(sd_card_t *sd_card_p, const uint8_t value) {
    uint32_t start = millis();
    while (!spi_is_writable(sd_card_p->spi_if_p->spi->hw_inst) &&
           millis() - start < SD_SPI_WRITE)
        tight_loop_contents();
    assert(spi_is_writable(sd_card_p->spi_if_p->spi->hw_inst));
    int num = spi_write_blocking(sd_card_p->spi_if_p->spi->hw_inst, &value, 1);
    assert(1 == num);
}
static inline uint8_t sd_spi_write_read(sd_card_t *sd_card_p, const uint8_t value) {
    uint8_t received = SPI_FILL_CHAR;
    uint32_t start = millis();
    while (!spi_is_writable(sd_card_p->spi_if_p->spi->hw_inst) &&
           millis() - start < SD_SPI_WRITE_READ)
        tight_loop_contents();
    assert(spi_is_writable(sd_card_p->spi_if_p->spi->hw_inst));
    int num = spi_write_read_blocking(sd_card_p->spi_if_p->spi->hw_inst, &value, &received, 1);    
    assert(1 == num);
    return received;
}

static inline void sd_spi_select(sd_card_t *sd_card_p) {
    if ((uint)-1 == sd_card_p->spi_if_p->ss_gpio) return;
    gpio_put(sd_card_p->spi_if_p->ss_gpio, 0);
    sd_spi_write(sd_card_p, SPI_FILL_CHAR);
}

static inline void sd_spi_deselect(sd_card_t *sd_card_p) {
    if ((uint)-1 == sd_card_p->spi_if_p->ss_gpio) return;
    gpio_put(sd_card_p->spi_if_p->ss_gpio, 1);
    sd_spi_write(sd_card_p, SPI_FILL_CHAR);
}

static inline void sd_spi_lock(sd_card_t *sd_card_p) { spi_lock(sd_card_p->spi_if_p->spi); }
static inline void sd_spi_unlock(sd_card_t *sd_card_p) { spi_unlock(sd_card_p->spi_if_p->spi); }

static inline void sd_spi_acquire(sd_card_t *sd_card_p) {
    sd_spi_lock(sd_card_p);
    sd_spi_select(sd_card_p);
}
static inline void sd_spi_release(sd_card_t *sd_card_p) {
    sd_spi_deselect(sd_card_p);
    sd_spi_unlock(sd_card_p);
}

static inline void sd_spi_transfer_start(sd_card_t *sd_card_p, const uint8_t *tx, uint8_t *rx,
                                         size_t length) {
    return spi_transfer_start(sd_card_p->spi_if_p->spi, tx, rx, length);
}

static inline bool sd_spi_transfer_wait_complete(sd_card_t *sd_card_p, uint32_t timeout_ms) {
    return spi_transfer_wait_complete(sd_card_p->spi_if_p->spi, timeout_ms);
}

static inline bool sd_spi_transfer(sd_card_t *sd_card_p, const uint8_t *tx, uint8_t *rx,
                                   size_t length) {
	return spi_transfer(sd_card_p->spi_if_p->spi, tx, rx, length);
}

#ifdef __cplusplus
}
#endif
