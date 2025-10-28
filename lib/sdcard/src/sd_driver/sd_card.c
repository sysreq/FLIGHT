#include <stdlib.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "pico/mutex.h"

#include "SPI/sd_card_spi.h"
#include "hw_config.h"
#include "sd_card_constants.h"

#include "sd_card.h"

#ifdef NDEBUG 
#  pragma GCC diagnostic ignored "-Wunused-variable"
#endif

static bool driver_initialized;

void sd_lock(sd_card_t *sd_card_p) {
    mutex_enter_blocking(&sd_card_p->state.mutex);
}
void sd_unlock(sd_card_t *sd_card_p) {
    mutex_exit(&sd_card_p->state.mutex);
}
bool sd_is_locked(sd_card_t *sd_card_p) {
    uint32_t owner_out;
    return !mutex_try_enter(&sd_card_p->state.mutex, &owner_out);
}

sd_card_t *sd_get_by_drive_prefix(const char *const drive_prefix) {
    if (2 == strlen(drive_prefix) && isdigit((unsigned char)drive_prefix[0]) &&
        ':' == drive_prefix[1])
        return sd_get_by_num(atoi(drive_prefix));

    return NULL;
}

bool sd_card_detect(sd_card_t *sd_card_p) {
    if (!sd_card_p->use_card_detect) {
        sd_card_p->state.m_Status &= ~STA_NODISK;
        return true;
    }

    if (gpio_get(sd_card_p->card_detect_gpio) == sd_card_p->card_detected_true) {
        sd_card_p->state.m_Status &= ~STA_NODISK;
        return true;
    } else {
        sd_card_p->state.m_Status |= (STA_NODISK | STA_NOINIT);
        sd_card_p->state.card_type = SDCARD_NONE;
        return false;
    }
}

void sd_set_drive_prefix(sd_card_t *sd_card_p, size_t phy_drv_num) {
    int rc = snprintf(sd_card_p->state.drive_prefix, sizeof sd_card_p->state.drive_prefix, "%d:", phy_drv_num);
    assert(0 <= rc && (size_t)rc < sizeof sd_card_p->state.drive_prefix);
}

char const *sd_get_drive_prefix(sd_card_t *sd_card_p) {
    assert(driver_initialized);
    assert(sd_card_p);
    if (!sd_card_p) return "";
    return sd_card_p->state.drive_prefix;
}

bool sd_init_driver() {
    auto_init_mutex(initialized_mutex);
    mutex_enter_blocking(&initialized_mutex);
    bool ok = true;
    if (!driver_initialized) {
        for (size_t i = 0; i < sd_get_num(); ++i) {
            sd_card_t *sd_card_p = sd_get_by_num(i);
            if (!sd_card_p) continue;
            if (!mutex_is_initialized(&sd_card_p->state.mutex))
                mutex_init(&sd_card_p->state.mutex);
            sd_lock(sd_card_p);

            sd_card_p->state.m_Status = STA_NOINIT;

            sd_set_drive_prefix(sd_card_p, i);

            if (sd_card_p->use_card_detect) {
                if (sd_card_p->card_detect_use_pull) {
                    if (sd_card_p->card_detect_pull_hi) {
                        gpio_pull_up(sd_card_p->card_detect_gpio);
                    } else {
                        gpio_pull_down(sd_card_p->card_detect_gpio);
                    }
                }
                gpio_init(sd_card_p->card_detect_gpio);
            }

            switch (sd_card_p->type) {
                case SD_IF_NONE:
                    assert(false);
                    break;
                case SD_IF_SPI:
                    assert(sd_card_p->spi_if_p);  // Must have an interface object
                    assert(sd_card_p->spi_if_p->spi);
                    sd_spi_ctor(sd_card_p);
                    if (!my_spi_init(sd_card_p->spi_if_p->spi)) {
                        ok = false;
                    }
                    sd_go_idle_state(sd_card_p);
                    break;
                default:
                    assert(false);
            }

            sd_unlock(sd_card_p);
        }
        driver_initialized = true;
    }
    mutex_exit(&initialized_mutex);
    return ok;
}

static inline void ext_str(size_t const data_sz,
                           uint8_t const data[],
                           size_t const msb,
                           size_t const lsb,
                           size_t const buf_sz,
                           char buf[]) {
    memset(buf, 0, buf_sz);
    size_t size = (1 + msb - lsb) / 8;  // bytes
    size_t byte = (data_sz - 1) - (msb / 8);
    for (uint32_t i = 0; i < size; i++) {
        buf[i] = data[byte++];
    }
}

void cidDmp(sd_card_t *sd_card_p, printer_t printer) {
    (*printer)(
        "\nManufacturer ID: "
        "0x%x\n",
        ext_bits16(sd_card_p->state.CID, 127, 120));
    (*printer)("OEM ID: ");
    {
        char buf[3];
        ext_str(16, sd_card_p->state.CID, 119, 104, sizeof buf, buf);
        (*printer)("%s", buf);
    }
    (*printer)("Product: ");
    {
        char buf[6];
        ext_str(16, sd_card_p->state.CID, 103, 64, sizeof buf, buf);
        (*printer)("%s", buf);
    }
    (*printer)(
        "\nRevision: "
        "%d.%d\n",
        ext_bits16(sd_card_p->state.CID, 63, 60), ext_bits16(sd_card_p->state.CID, 59, 56));
    (*printer)(
        "Serial number: "
        "0x%lx\n",
        ext_bits16(sd_card_p->state.CID, 55, 24));
    (*printer)(
        "Manufacturing date: "
        "%d/%d\n",
        ext_bits16(sd_card_p->state.CID, 11, 8),
        ext_bits16(sd_card_p->state.CID, 19, 12) + 2000);
    (*printer)("\n");
}
void csdDmp(sd_card_t *sd_card_p, printer_t printer) {
    uint32_t c_size, c_size_mult, read_bl_len;
    uint32_t block_len, mult, blocknr;
    uint32_t hc_c_size;
    uint64_t blocks = 0, capacity = 0;
    bool erase_single_block_enable = 0;
    uint8_t erase_sector_size = 0;

    int csd_structure = ext_bits16(sd_card_p->state.CSD, 127, 126);
    switch (csd_structure) {
        case 0:
            c_size = ext_bits16(sd_card_p->state.CSD, 73, 62);
            c_size_mult = ext_bits16(sd_card_p->state.CSD, 49, 47);
            read_bl_len = ext_bits16(sd_card_p->state.CSD, 83, 80);
            block_len = 1 << read_bl_len;
            mult = 1 << (c_size_mult + 2);
            blocknr = (c_size + 1) * mult;
            capacity = (uint64_t)blocknr * block_len;
            blocks = capacity / sd_block_size;

            (*printer)("Standard Capacity: c_size: %" PRIu32 "\r\n", c_size);
            (*printer)("Sectors: 0x%llx : %llu\r\n", blocks, blocks);
            (*printer)("Capacity: 0x%llx : %llu MiB\r\n", capacity,
                       (capacity / (1024U * 1024U)));
            break;

        case 1:
            hc_c_size =
                ext_bits16(sd_card_p->state.CSD, 69, 48);  // device size : C_SIZE : [69:48]
            blocks = (hc_c_size + 1) << 10;                // block count = C_SIZE+1) * 1K
                                                           // byte (512B is block size)
            erase_single_block_enable = ext_bits16(sd_card_p->state.CSD, 46, 46);
            erase_sector_size = ext_bits16(sd_card_p->state.CSD, 45, 39) + 1;

            (*printer)("SDHC/SDXC Card: hc_c_size: %" PRIu32 "\r\n", hc_c_size);
            (*printer)("Sectors: %llu\r\n", blocks);
            (*printer)("Capacity: %llu MiB (%llu MB)\r\n", blocks / 2048,
                       blocks * sd_block_size / 1000000);
            (*printer)("ERASE_BLK_EN: %s\r\n", erase_single_block_enable
                                                   ? "units of 512 bytes"
                                                   : "units of SECTOR_SIZE");
            (*printer)("SECTOR_SIZE (size of an erasable sector): %d (%lu bytes)\r\n",
                       erase_sector_size,
                       (uint32_t)(erase_sector_size ? 512 : 1) * erase_sector_size);
            break;

        default:
            (*printer)("CSD struct unsupported\r\n");
    };
}

#define KB 1024
#define MB (1024 * 1024)

bool sd_allocation_unit(sd_card_t *sd_card_p, size_t *au_size_bytes_p) {
    if (SD_IF_SPI == sd_card_p->type) return false;  // SPI can't do full SD Status
    return true;
}
