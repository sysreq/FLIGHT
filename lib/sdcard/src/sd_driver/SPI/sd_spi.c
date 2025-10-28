#include <inttypes.h>
#include <stdint.h>
#include <string.h>

#include "hardware/gpio.h"

#include "delays.h"
#include "my_spi.h"

#if !defined(USE_DBG_PRINTF) || defined(NDEBUG)
#  pragma GCC diagnostic ignored "-Wunused-variable"
#endif

#include "sd_spi.h"

void sd_spi_go_high_frequency(sd_card_t *sd_card_p) {
    uint actual = spi_set_baudrate(sd_card_p->spi_if_p->spi->hw_inst, sd_card_p->spi_if_p->spi->baud_rate);
}
void sd_spi_go_low_frequency(sd_card_t *sd_card_p) {
    uint actual = spi_set_baudrate(sd_card_p->spi_if_p->spi->hw_inst, 400 * 1000);
}

void sd_spi_send_initializing_sequence(sd_card_t *sd_card_p) {
    if ((uint)-1 == sd_card_p->spi_if_p->ss_gpio) return; 
    bool old_ss = gpio_get(sd_card_p->spi_if_p->ss_gpio);
    gpio_put(sd_card_p->spi_if_p->ss_gpio, 1);
    uint8_t ones[10];
    memset(ones, 0xFF, sizeof ones);
    uint32_t start = millis();
    do {
        spi_transfer(sd_card_p->spi_if_p->spi, ones, NULL, sizeof ones);
    } while (millis() - start < 1);
    gpio_put(sd_card_p->spi_if_p->ss_gpio, old_ss);
}
