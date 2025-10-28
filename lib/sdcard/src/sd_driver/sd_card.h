#pragma once

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include "hardware/gpio.h"
#include "pico/mutex.h"

#include "ff.h"

#include "SPI/my_spi.h"
#include "sd_card_constants.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { SD_IF_NONE, SD_IF_SPI } sd_if_t;
typedef int (*printer_t)(const char* format, ...);

typedef uint8_t CID_t[16];
typedef uint8_t CSD_t[16];

static inline uint32_t ext_bits(size_t n_src_bytes, unsigned char const *data, int msb, int lsb) {
    uint32_t bits = 0;
    uint32_t size = 1 + msb - lsb;
    for (uint32_t i = 0; i < size; i++) {
        uint32_t position = lsb + i;
        uint32_t byte = (n_src_bytes - 1) - (position >> 3);
        uint32_t bit = position & 0x7;
        uint32_t value = (data[byte] >> bit) & 1;
        bits |= value << i;
    }
    return bits;
}

static inline uint32_t ext_bits16(unsigned char const *data, int msb, int lsb) {
    return ext_bits(16, data, msb, lsb);
}

static inline uint32_t CSD_sectors(CSD_t csd) /*  const  */ {
    uint32_t c_size;
    uint8_t ver = ext_bits16(csd, 127, 126);
    if (ver == 0) {
        c_size = ext_bits16(csd, 73, 62);
        uint8_t c_size_mult = ext_bits16(csd, 49, 47);
        uint32_t mult = 1UL << (c_size_mult + 2);
        return (c_size + 1) * mult;
    } else if (ver == 1) {
        c_size = ext_bits16(csd, 69, 48);
        return (c_size + 1) * 1024; // sectors
    } else {
        return 0;
    }
}

typedef struct sd_spi_if_state_t {
    bool ongoing_mlt_blk_wrt;
    uint32_t cont_sector_wrt;
    uint32_t n_wrt_blks_reqd;
} sd_spi_if_state_t;

typedef struct sd_spi_if_t {
    spi_t *spi;
    uint ss_gpio;
    bool set_drive_strength;
    enum gpio_drive_strength ss_gpio_drive_strength;
    sd_spi_if_state_t state;
} sd_spi_if_t;

typedef struct sd_card_state_t {
    DSTATUS m_Status;       // Card status
    card_type_t card_type;  // Assigned dynamically
    CSD_t CSD;              // Card-Specific Data register.
    CID_t CID;              // Card IDentification register
    uint32_t sectors;       // Assigned dynamically

    mutex_t mutex;
    FATFS fatfs;
    bool mounted;
    char drive_prefix[4];
} sd_card_state_t;

typedef struct sd_card_t sd_card_t;

struct sd_card_t {
    sd_if_t type; 
    sd_spi_if_t *spi_if_p;
    bool use_card_detect;
    uint card_detect_gpio;    // Card detect; ignored if !use_card_detect
    uint card_detected_true;  // Varies with card socket; ignored if !use_card_detect
    bool card_detect_use_pull;
    bool card_detect_pull_hi;

    sd_card_state_t state;

    DSTATUS (*init)(sd_card_t *sd_card_p);
    void (*deinit)(sd_card_t *sd_card_p);
    block_dev_err_t (*write_blocks)(sd_card_t *sd_card_p, const uint8_t *buffer,
                                    uint32_t ulSectorNumber, uint32_t blockCnt);
    block_dev_err_t (*read_blocks)(sd_card_t *sd_card_p, uint8_t *buffer,
                                   uint32_t ulSectorNumber, uint32_t ulSectorCount);
    block_dev_err_t (*sync)(sd_card_t *sd_card_p);
    uint32_t (*get_num_sectors)(sd_card_t *sd_card_p);

    bool (*sd_test_com)(sd_card_t *sd_card_p);
};

void sd_lock(sd_card_t *sd_card_p);
void sd_unlock(sd_card_t *sd_card_p);
bool sd_is_locked(sd_card_t *sd_card_p);

bool sd_init_driver();
bool sd_card_detect(sd_card_t *sd_card_p);
void cidDmp(sd_card_t *sd_card_p, printer_t printer);
void csdDmp(sd_card_t *sd_card_p, printer_t printer);
bool sd_allocation_unit(sd_card_t *sd_card_p, size_t *au_size_bytes_p);
sd_card_t *sd_get_by_drive_prefix(const char *const name);

char const *sd_get_drive_prefix(sd_card_t *sd_card_p);

#ifdef __cplusplus
}
#endif
