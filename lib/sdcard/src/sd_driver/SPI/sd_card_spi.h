#pragma once

#include "sd_card.h"

#ifdef __cplusplus
extern "C" {
#endif

void sd_spi_ctor(sd_card_t *sd_card_p);
uint32_t sd_go_idle_state(sd_card_t *sd_card_p);

#ifdef __cplusplus
}
#endif
