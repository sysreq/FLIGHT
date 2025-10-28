#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sd_card_t sd_card_t;

size_t sd_get_num();
sd_card_t* sd_get_by_num(size_t num);

#ifdef __cplusplus
}
#endif
