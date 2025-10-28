#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "crc.h"
#include "hw_config.h"  // Hardware Configuration of the SPI and SD Card "objects"
#include "delays.h"
#include "sd_card.h"
#include "sd_card_constants.h"
#include "sd_spi.h"

#include "sd_card_spi.h"

#if defined(NDEBUG) || !USE_DBG_PRINTF
#  pragma GCC diagnostic ignored "-Wunused-function"
#  pragma GCC diagnostic ignored "-Wunused-variable"
#  pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

#ifndef TRACE
#  define TRACE 0
#endif

#ifndef SD_CRC_ENABLED
#define SD_CRC_ENABLED 1
#endif

#if SD_CRC_ENABLED
static bool crc_on = true;
#else
static bool crc_on = false;
#endif

typedef enum {
    SPI_DATA_RESPONSE_MASK = 0x1F,
    SPI_DATA_ACCEPTED = 0x05,
    SPI_DATA_CRC_ERROR = 0x0B,
    SPI_DATA_WRITE_ERROR = 0x0D,
    SPI_START_BLOCK = 0xFE,
    SPI_START_BLK_MUL_WRITE = 0xFC,
    SPI_STOP_TRAN = 0xFD,
} spi_control_t;

typedef enum {
    SPI_DATA_READ_ERROR_MASK = 0xF,
} spi_data_read_error_mask_t;

typedef enum {
    SPI_READ_ERROR = 0x1 << 0,
    SPI_READ_ERROR_CC = 0x1 << 1,
    SPI_READ_ERROR_ECC_C = 0x1 << 2,
    SPI_READ_ERROR_OFR = 0x1 << 3,
} spi_data_read_error_t;

typedef enum {
    R1_NO_RESPONSE = 0xFF,
    R1_RESPONSE_RECV = 0x80,
    R1_IDLE_STATE = 1 << 0,
    R1_ERASE_RESET = 1 << 1,
    R1_ILLEGAL_COMMAND = 1 << 2,
    R1_COM_CRC_ERROR = 1 << 3,
    R1_ERASE_SEQUENCE_ERROR = 1 << 4,
    R1_ADDRESS_ERROR = 1 << 5,
    R1_PARAMETER_ERROR = 1 << 6,
} spi_r1_response_t;

#define PACKET_SIZE 6         /*!< SD Packet size CMD+ARG+CRC */

typedef enum {
    OCR_HCS_CCS = 0x1 << 30,
    OCR_LOW_VOLTAGE = 0x01 << 24,
    OCR_3_3V = 0x1 << 20,
} spi_ocr_register_t;

#define SPI_CMD(x) (0x40 | (x & 0x3f))

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"

static uint8_t sd_cmd_spi(sd_card_t *sd_card_p, cmdSupported cmd, uint32_t arg) {
    uint8_t cmd_packet[PACKET_SIZE] = {
        SPI_CMD(cmd),
        (arg >> 24),
        (arg >> 16),
        (arg >> 8),
        (arg >> 0),
    };

    if (crc_on) {
        cmd_packet[5] = (crc7(cmd_packet, 5) << 1) | 0x01;
    } else {
        switch (cmd) {
            case CMD0_GO_IDLE_STATE:
                cmd_packet[5] = 0x95;
                break;
            case CMD8_SEND_IF_COND:
                cmd_packet[5] = 0x87;
                break;
            default:
                cmd_packet[5] = 0xFF;
                break;
        }
    }

    for (size_t i = 0; i < PACKET_SIZE; i++) {
        sd_spi_write(sd_card_p, cmd_packet[i]);
    }

    if (CMD12_STOP_TRANSMISSION == cmd) {
        sd_spi_write(sd_card_p, SPI_FILL_CHAR);
    }

    uint8_t response;
    for (size_t i = 0; i < 0x10; i++) {
        response = sd_spi_read(sd_card_p);
        // Got the response
        if (!(response & R1_RESPONSE_RECV)) {
            break;
        }
    }

    return response;
}
#pragma GCC diagnostic pop

static bool sd_wait_ready(sd_card_t *sd_card_p, uint32_t timeout) {
    char resp;
    uint32_t start = millis();
    do {
        resp = sd_spi_write_read(sd_card_p, 0xFF);
    } while (resp != 0xFF && millis() - start < timeout);
    return (0xFF == resp);
}

static void sd_acquire(sd_card_t *sd_card_p) {
    sd_lock(sd_card_p);
    sd_spi_acquire(sd_card_p);
}

static void sd_release(sd_card_t *sd_card_p) {
    sd_spi_release(sd_card_p);
    sd_unlock(sd_card_p);
}

static int chk_CMD13_response(uint32_t response) {
    int32_t status = 0;
    if (response & 0x01 << 0) {
        status |= SD_BLOCK_DEVICE_ERROR_WRITE;
    }
    if (response & 0x01 << 1) {
        status |= SD_BLOCK_DEVICE_ERROR_WRITE_PROTECTED;
    }
    if (response & 0x01 << 2) {
        status |= SD_BLOCK_DEVICE_ERROR_WRITE;
    }
    if (response & 0x01 << 3) {
        status |= SD_BLOCK_DEVICE_ERROR_WRITE;
    }
    if (response & 0x01 << 4) {
        status |= SD_BLOCK_DEVICE_ERROR_WRITE;
    }
    if (response & 0x01 << 5) {
        status |= SD_BLOCK_DEVICE_ERROR_WRITE_PROTECTED;
    }
    if (response & 0x01 << 6) {
        status |= SD_BLOCK_DEVICE_ERROR_ERASE;
    }
    if (response & 0x01 << 7) {
        status |= SD_BLOCK_DEVICE_ERROR_PARAMETER;
    }
    if (response & 0x01 << 8) {
        status |= SD_BLOCK_DEVICE_ERROR_NONE;
    }
    if (response & 0x01 << 9) {
        status |= SD_BLOCK_DEVICE_ERROR_ERASE;
    }
    if (response & 0x01 << 10) {
        status |= SD_BLOCK_DEVICE_ERROR_UNSUPPORTED;
    }
    if (response & 0x01 << 11) {
        status |= SD_BLOCK_DEVICE_ERROR_CRC;
    }
    if (response & 0x01 << 12) {
        status |= SD_BLOCK_DEVICE_ERROR_ERASE;
    }
    if (response & 0x01 << 13) {
        status |= SD_BLOCK_DEVICE_ERROR_PARAMETER;
    }
    if (response & 0x01 << 14) {
        status |= SD_BLOCK_DEVICE_ERROR_PARAMETER;
    }
    return status;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"

static block_dev_err_t sd_cmd(sd_card_t *sd_card_p, const cmdSupported cmd, uint32_t arg, bool isAcmd, uint32_t *resp) {
    assert(sd_is_locked(sd_card_p));
    assert(0 == gpio_get(sd_card_p->spi_if_p->ss_gpio));

    int32_t status = SD_BLOCK_DEVICE_ERROR_NONE;
    uint32_t response = 0;

    if (CMD12_STOP_TRANSMISSION != cmd && CMD0_GO_IDLE_STATE != cmd) {
        if (false == sd_wait_ready(sd_card_p, SD_COMMAND)) {
            return SD_BLOCK_DEVICE_ERROR_NO_RESPONSE;
        }
    }
    for (unsigned i = 0; i < SD_COMMAND_RETRIES; i++) {
        if (isAcmd) {
            response = sd_cmd_spi(sd_card_p, CMD55_APP_CMD, 0x0);
        }
        response = sd_cmd_spi(sd_card_p, cmd, arg);
        if (R1_NO_RESPONSE == response) {
            continue;
        }
        break;
    }

    if (NULL != resp) {
        *resp = response;
    }

    if (R1_NO_RESPONSE == response) {
        return SD_BLOCK_DEVICE_ERROR_NO_RESPONSE;
    }
    if (response & R1_COM_CRC_ERROR && ACMD23_SET_WR_BLK_ERASE_COUNT != cmd) {
        return SD_BLOCK_DEVICE_ERROR_CRC;  // CRC error
    }
    if (response & R1_ILLEGAL_COMMAND) {
        if (CMD8_SEND_IF_COND == cmd) {
            // Illegal command is for Ver1 or not SD Card
            sd_card_p->state.card_type = CARD_UNKNOWN;
        }
        return SD_BLOCK_DEVICE_ERROR_UNSUPPORTED;  // Command not supported
    }

    if ((response & R1_ERASE_RESET) || (response & R1_ERASE_SEQUENCE_ERROR)) {
        status = SD_BLOCK_DEVICE_ERROR_ERASE;  // Erase error
    } else if ((response & R1_ADDRESS_ERROR) || (response & R1_PARAMETER_ERROR)) {
        status = SD_BLOCK_DEVICE_ERROR_PARAMETER;
    }

    switch (cmd) {
        case CMD8_SEND_IF_COND:  // Response R7
            sd_card_p->state.card_type = SDCARD_V2;  // fallthrough
        case CMD58_READ_OCR:  // Response R3
            response = (sd_spi_read(sd_card_p) << 24);
            response |= (sd_spi_read(sd_card_p) << 16);
            response |= (sd_spi_read(sd_card_p) << 8);
            response |= sd_spi_read(sd_card_p);
            break;
        case CMD12_STOP_TRANSMISSION:  // Response R1b
        case CMD38_ERASE:
            sd_wait_ready(sd_card_p, SD_COMMAND);
            break;
        case CMD13_SEND_STATUS:  // Response R2
            response <<= 8;
            response |= sd_spi_read(sd_card_p);
            if (response) status = chk_CMD13_response(response);
        default:;
    }

    if (NULL != resp) {
        *resp = response;
    }
    return status;
}
#pragma GCC diagnostic pop

#define CMD8_PATTERN (0xAA)
static block_dev_err_t sd_cmd8(sd_card_t *sd_card_p) {
    uint32_t arg = (CMD8_PATTERN << 0);  // [7:0]check pattern
    uint32_t response = 0;
    int32_t status = SD_BLOCK_DEVICE_ERROR_NONE;

    arg |= (0x1 << 8);  // 2.7-3.6V             // [11:8]supply voltage(VHS)

    status = sd_cmd(sd_card_p, CMD8_SEND_IF_COND, arg, false, &response);
    // Verify voltage and pattern for V2 version of card
    if ((SD_BLOCK_DEVICE_ERROR_NONE == status) && (SDCARD_V2 == sd_card_p->state.card_type)) {
        // If check pattern is not matched, CMD8 communication is not valid
        if ((response & 0xFFF) != arg) {
            sd_card_p->state.card_type = CARD_UNKNOWN;
            status = SD_BLOCK_DEVICE_ERROR_UNUSABLE;
        }
    }
    return status;
}

static block_dev_err_t read_bytes(sd_card_t *sd_card_p, uint8_t *buffer, uint32_t length);

static uint32_t in_sd_spi_sectors(sd_card_t *sd_card_p) {
    // CMD9, Response R2 (R1 byte + 16-byte block read)
    if (sd_cmd(sd_card_p, CMD9_SEND_CSD, 0x0, false, 0) != 0x0) {
        return 0;
    }
    if (read_bytes(sd_card_p, sd_card_p->state.CSD, 16) != 0) {
        return 0;
    }
    return CSD_sectors(sd_card_p->state.CSD);
}

uint32_t sd_spi_sectors(sd_card_t *sd_card_p) {
    sd_acquire(sd_card_p);
    uint32_t sectors = in_sd_spi_sectors(sd_card_p);
    sd_release(sd_card_p);
    return sectors;
}

static bool sd_wait_token(sd_card_t *sd_card_p, uint8_t token) {
    uint32_t start = millis();
    do {
        if (token == sd_spi_read(sd_card_p)) {
            return true;
        }
    } while (millis() - start < SD_COMMAND);

    return false;
}

static bool chk_crc16(uint8_t *buffer, size_t length, uint16_t crc) {
    if (crc_on) {
        uint16_t crc_result;
        crc_result = crc16(buffer, length);
        return (crc_result == crc);
    }
    return true;
}

#define SPI_START_BLOCK (0xFE) /* For Single Block Read/Write and Multiple Block Read */

static block_dev_err_t stop_wr_tran(sd_card_t *sd_card_p);

static block_dev_err_t read_bytes(sd_card_t *sd_card_p, uint8_t *buffer, uint32_t length) {
    uint16_t crc;

    // read until start byte (0xFE)
    if (false == sd_wait_token(sd_card_p, SPI_START_BLOCK)) {
        return SD_BLOCK_DEVICE_ERROR_NO_RESPONSE;
    }
    bool ok = sd_spi_transfer(sd_card_p, NULL, buffer, length);
    if (!ok) return SD_BLOCK_DEVICE_ERROR_NO_RESPONSE;

    // Read the CRC16 checksum for the data block
    crc = (sd_spi_read(sd_card_p) << 8);
    crc |= sd_spi_read(sd_card_p);

    if (!chk_crc16(buffer, length, crc)) {
        return SD_BLOCK_DEVICE_ERROR_CRC;
    }
    return 0;
}

static block_dev_err_t in_sd_read_blocks(sd_card_t *sd_card_p, uint8_t *buffer, const uint32_t data_address, const uint32_t num_rd_blks) {
    if (sd_card_p->state.m_Status & (STA_NOINIT | STA_NODISK))
        return SD_BLOCK_DEVICE_ERROR_PARAMETER;
    if (!num_rd_blks) return SD_BLOCK_DEVICE_ERROR_PARAMETER;
    if (data_address + num_rd_blks > sd_card_p->state.sectors)
        return SD_BLOCK_DEVICE_ERROR_PARAMETER;

    block_dev_err_t status = SD_BLOCK_DEVICE_ERROR_NONE;

    if (sd_card_p->spi_if_p->state.ongoing_mlt_blk_wrt) {
        status = stop_wr_tran(sd_card_p);
        if (SD_BLOCK_DEVICE_ERROR_NONE != status) return status;
    }

    if (num_rd_blks == 1)
        status = sd_cmd(sd_card_p, CMD17_READ_SINGLE_BLOCK, data_address, false, 0);
    else
        status = sd_cmd(sd_card_p, CMD18_READ_MULTIPLE_BLOCK, data_address, false, 0);
    if (SD_BLOCK_DEVICE_ERROR_NONE != status) return status;

    uint16_t prev_block_crc = 0;
    uint8_t *prev_buffer_addr = 0;
    uint32_t blk_cnt = num_rd_blks;

    while (blk_cnt) {
        if (!sd_wait_token(sd_card_p, SPI_START_BLOCK)) {
            return SD_BLOCK_DEVICE_ERROR_NO_RESPONSE;
        }
        sd_spi_transfer_start(sd_card_p, NULL, buffer, sd_block_size);
        if (prev_buffer_addr) {
            if (!chk_crc16(prev_buffer_addr, sd_block_size, prev_block_crc)) {
                return SD_BLOCK_DEVICE_ERROR_CRC;
            }
        }
        
        uint32_t timeout = calculate_transfer_time_ms(sd_card_p->spi_if_p->spi, sd_block_size);
        bool ok = sd_spi_transfer_wait_complete(sd_card_p, timeout);
        if (!ok) return SD_BLOCK_DEVICE_ERROR_NO_RESPONSE;

        prev_block_crc = sd_spi_read(sd_card_p) << 8;
        prev_block_crc |= sd_spi_read(sd_card_p);
        prev_buffer_addr = buffer;
        buffer += sd_block_size;
        --blk_cnt;
    }

    if (num_rd_blks > 1) {
        status = sd_cmd(sd_card_p, CMD12_STOP_TRANSMISSION, 0x0, false, 0);
        if (SD_BLOCK_DEVICE_ERROR_NONE != status) return status;
    }
    if (!chk_crc16(prev_buffer_addr, sd_block_size, prev_block_crc)) {
        return SD_BLOCK_DEVICE_ERROR_CRC;
    }
    return status;
}

static block_dev_err_t sd_read_blocks(sd_card_t *sd_card_p, uint8_t *buffer, uint32_t data_address, uint32_t num_rd_blks) {
    sd_acquire(sd_card_p);
    unsigned retries = SD_COMMAND_RETRIES;
    block_dev_err_t status;
    do {
        status = in_sd_read_blocks(sd_card_p, buffer, data_address, num_rd_blks);
        if (status != SD_BLOCK_DEVICE_ERROR_NONE) {
            if (SD_BLOCK_DEVICE_ERROR_NONE !=
                sd_cmd(sd_card_p, CMD12_STOP_TRANSMISSION, 0x0, false, 0))
                return SD_BLOCK_DEVICE_ERROR_NO_RESPONSE;
        }
    } while (--retries && status != SD_BLOCK_DEVICE_ERROR_NONE);
    sd_release(sd_card_p);
    return status;
}

static block_dev_err_t get_num_wr_blocks(sd_card_t *sd_card_p, uint32_t *num_p) {
    block_dev_err_t err = sd_cmd(sd_card_p, ACMD22_SEND_NUM_WR_BLOCKS, 0, true, NULL);

    if (SD_BLOCK_DEVICE_ERROR_NONE != err) {
        return err;
    }
    
    err = read_bytes(sd_card_p, (uint8_t *)num_p, sizeof(uint32_t));
    *num_p = __builtin_bswap32(*num_p);
    
    if (SD_BLOCK_DEVICE_ERROR_NONE != err) {
        return err;
    }
    
    return SD_BLOCK_DEVICE_ERROR_NONE;
}

static block_dev_err_t send_block(sd_card_t *sd_card_p, const uint8_t *buffer, uint8_t token,
                                     uint32_t length)
{
    uint8_t response;

    response = sd_spi_write_read(sd_card_p, token);
    if (!response) {
        return SD_BLOCK_DEVICE_ERROR_WRITE;
    }

    sd_spi_transfer_start(sd_card_p, buffer, NULL, length);

    uint16_t crc = (~0);
    if (crc_on) {
        crc = crc16((void *)buffer, length);
    }
    uint32_t timeout = calculate_transfer_time_ms(sd_card_p->spi_if_p->spi, length);
    bool ok = sd_spi_transfer_wait_complete(sd_card_p, timeout);
    if (!ok) return SD_BLOCK_DEVICE_ERROR_WRITE;

    sd_spi_write(sd_card_p, crc >> 8);
    sd_spi_write(sd_card_p, crc);

    block_dev_err_t rc = SD_BLOCK_DEVICE_ERROR_NONE;
    response = sd_spi_read(sd_card_p);

    if ((response & SPI_DATA_RESPONSE_MASK) != SPI_DATA_ACCEPTED) {
        rc = SD_BLOCK_DEVICE_ERROR_WRITE;
    }

    if (false == sd_wait_ready(sd_card_p, SD_COMMAND)) {
        rc = SD_BLOCK_DEVICE_ERROR_WRITE;
    }
    return rc;
}

static block_dev_err_t send_all_blocks(sd_card_t *sd_card_p, const uint8_t *buffer_p[],
        uint32_t * const data_address_p,
        uint32_t * const num_wrt_blks_p)
{
    block_dev_err_t status;
    do {
        status = send_block(sd_card_p, *buffer_p, SPI_START_BLK_MUL_WRITE, sd_block_size);
        if (SD_BLOCK_DEVICE_ERROR_NONE != status) break;
        *buffer_p += sd_block_size;
        ++*data_address_p;
    } while (--*num_wrt_blks_p);
    if (SD_BLOCK_DEVICE_ERROR_NONE == status) {
        sd_card_p->spi_if_p->state.cont_sector_wrt = *data_address_p;
        sd_card_p->spi_if_p->state.ongoing_mlt_blk_wrt = true;
    } else {
        // sd_card_p->spi_if_p->state.n_wrt_blks_reqd cleared in stop_wr_tran
        uint32_t n_wrt_blks_reqd = sd_card_p->spi_if_p->state.n_wrt_blks_reqd;
        stop_wr_tran(sd_card_p); // Ignore return value
        uint32_t nw;
        block_dev_err_t err = get_num_wr_blocks(sd_card_p, &nw);
        if (SD_BLOCK_DEVICE_ERROR_NONE == err) {
            *num_wrt_blks_p = n_wrt_blks_reqd - nw;
        }
    }
    return status;
}

static block_dev_err_t in_sd_write_blocks(sd_card_t *sd_card_p, const uint8_t *buffer_p[], uint32_t * const data_address_p, uint32_t * const num_wrt_blks_p)
{
    block_dev_err_t status = SD_BLOCK_DEVICE_ERROR_NONE;

    if (sd_card_p->spi_if_p->state.ongoing_mlt_blk_wrt && sd_card_p->spi_if_p->state.cont_sector_wrt == *data_address_p) {
        sd_card_p->spi_if_p->state.n_wrt_blks_reqd += *num_wrt_blks_p;
        return send_all_blocks(sd_card_p, buffer_p, data_address_p, num_wrt_blks_p);
    }

    if (sd_card_p->spi_if_p->state.ongoing_mlt_blk_wrt) {
        status = stop_wr_tran(sd_card_p);
        if (SD_BLOCK_DEVICE_ERROR_NONE != status) return status;
    }

    status = sd_cmd(sd_card_p, CMD25_WRITE_MULTIPLE_BLOCK, *data_address_p, false, 0);
    if (SD_BLOCK_DEVICE_ERROR_NONE != status) return status;

    sd_card_p->spi_if_p->state.n_wrt_blks_reqd = *num_wrt_blks_p;
    return send_all_blocks(sd_card_p, buffer_p, data_address_p, num_wrt_blks_p);
}
static block_dev_err_t stop_wr_tran(sd_card_t *sd_card_p) {
    sd_card_p->spi_if_p->state.ongoing_mlt_blk_wrt = false;
    sd_spi_write(sd_card_p, SPI_STOP_TRAN);
    if (false == sd_wait_ready(sd_card_p, SD_COMMAND)) { __nop; }

    uint32_t stat = 0;
    sd_card_p->spi_if_p->state.n_wrt_blks_reqd = 0;
    return sd_cmd(sd_card_p, CMD13_SEND_STATUS, 0, false, &stat);
}

static block_dev_err_t write_block(sd_card_t *sd_card_p, const uint8_t *buffer,
                                          uint32_t const address)
{
    // Stop any ongoing multiple block write transmission
    block_dev_err_t status = SD_BLOCK_DEVICE_ERROR_NONE;
    if (sd_card_p->spi_if_p->state.ongoing_mlt_blk_wrt) {
        status = stop_wr_tran(sd_card_p);
        if (SD_BLOCK_DEVICE_ERROR_NONE != status) return status;
    }

    status = sd_cmd(sd_card_p, CMD24_WRITE_BLOCK, address, false, 0);
    if (SD_BLOCK_DEVICE_ERROR_NONE != status) return status;

    send_block(sd_card_p, buffer, SPI_START_BLOCK, sd_block_size);

    uint32_t stat = 0;
    status = sd_cmd(sd_card_p, CMD13_SEND_STATUS, 0, false, &stat);

    return status;
}

static block_dev_err_t sd_write_blocks(sd_card_t *sd_card_p, uint8_t const buffer[], uint32_t data_address, uint32_t num_wrt_blks) 
{
    if (NULL == sd_card_p)
        return SD_BLOCK_DEVICE_ERROR_PARAMETER;

    // Check if the device is initialized and not missing
    if (sd_card_p->state.m_Status & (STA_NOINIT | STA_NODISK))
        return SD_BLOCK_DEVICE_ERROR_PARAMETER;

    // Check if the number of blocks to write is valid
    if (!num_wrt_blks) return SD_BLOCK_DEVICE_ERROR_PARAMETER;

    // Calculate the end address
    uint32_t end_address = data_address + num_wrt_blks;

    // Check if the end address is within the device's boundaries
    if (end_address >= sd_card_p->state.sectors)
        return SD_BLOCK_DEVICE_ERROR_PARAMETER;

    // Acquire the SD card
    sd_acquire(sd_card_p);

    block_dev_err_t status;

    // If writing only one block, use the optimized function
    if (1 == num_wrt_blks) {
        status = write_block(sd_card_p, buffer, data_address);
    } else {
        // If writing multiple blocks, retry the operation until it succeeds or reaches the maximum number of retries
        unsigned retries = SD_COMMAND_RETRIES;
        do {
            status = in_sd_write_blocks(sd_card_p, &buffer, &data_address, &num_wrt_blks);
        } while (SD_BLOCK_DEVICE_ERROR_WRITE == status && --retries && num_wrt_blks);
    }

    sd_release(sd_card_p);

    return status;
}

static block_dev_err_t sd_sync(sd_card_t *sd_card_p) {
    block_dev_err_t status = SD_BLOCK_DEVICE_ERROR_NONE;
    sd_acquire(sd_card_p);
    if (sd_card_p->spi_if_p->state.ongoing_mlt_blk_wrt) status = stop_wr_tran(sd_card_p);
    sd_release(sd_card_p);
    return status;
}

#define SD_CMD0_GO_IDLE_STATE_RETRIES 10

static uint32_t in_sd_go_idle_state(sd_card_t *sd_card_p) {
    sd_spi_go_low_frequency(sd_card_p);

    uint32_t response = R1_NO_RESPONSE;
    for (int i = 0; i < SD_CMD0_GO_IDLE_STATE_RETRIES; i++) {
        sd_spi_deselect(sd_card_p);
        uint8_t ones[10];
        memset(ones, 0xFF, sizeof ones);
        uint32_t start = millis();
        do {
            spi_transfer(sd_card_p->spi_if_p->spi, ones, NULL, sizeof ones);
        } while (millis() - start < 1);
        sd_spi_select(sd_card_p);

        sd_cmd(sd_card_p, CMD0_GO_IDLE_STATE, 0x0, false, &response);
        if (R1_IDLE_STATE == response) {
            break;
        }
    }
    return response;
}

uint32_t sd_go_idle_state(sd_card_t *sd_card_p) {
    sd_spi_lock(sd_card_p);
    uint32_t response = in_sd_go_idle_state(sd_card_p);
    sd_spi_release(sd_card_p);
    return response;
}

static block_dev_err_t sd_init_medium(sd_card_t *sd_card_p) {
    int32_t status = SD_BLOCK_DEVICE_ERROR_NONE;
    uint32_t response, arg;

    // The card is transitioned from SDCard mode to SPI mode by sending the CMD0
    // + CS Asserted("0")
    if (in_sd_go_idle_state(sd_card_p) != R1_IDLE_STATE) {
        return SD_BLOCK_DEVICE_ERROR_NO_DEVICE;
    }

    // Send CMD8, if the card rejects the command then it's probably using the
    // legacy protocol, or is a MMC, or just flat-out broken
    status = sd_cmd8(sd_card_p);
    if (SD_BLOCK_DEVICE_ERROR_NONE != status && SD_BLOCK_DEVICE_ERROR_UNSUPPORTED != status) {
        return status;
    }

    if (crc_on) {
        size_t retries = 3;
        do {
            // Enable CRC
            status = sd_cmd(sd_card_p, CMD59_CRC_ON_OFF, 1, false, 0);
        } while (--retries && (SD_BLOCK_DEVICE_ERROR_NONE != status));
    }

    // Read OCR - CMD58 Response contains OCR register
    if (SD_BLOCK_DEVICE_ERROR_NONE !=
        (status = sd_cmd(sd_card_p, CMD58_READ_OCR, 0x0, false, &response))) {
        return status;
    }
    // Check if card supports voltage range: 3.3V
    if (!(response & OCR_3_3V)) {
        sd_card_p->state.card_type = CARD_UNKNOWN;
        status = SD_BLOCK_DEVICE_ERROR_UNUSABLE;
        return status;
    }

    arg = 0x0;
    if (SDCARD_V2 == sd_card_p->state.card_type) {
        arg |= OCR_HCS_CCS;
    }

    uint32_t start = millis();
    do {
        status = sd_cmd(sd_card_p, ACMD41_SD_SEND_OP_COND, arg, true, &response);
    } while (response & R1_IDLE_STATE && millis() - start < SD_COMMAND);
    // Initialization complete: ACMD41 successful
    if ((SD_BLOCK_DEVICE_ERROR_NONE != status) || (0x00 != response)) {
        sd_card_p->state.card_type = CARD_UNKNOWN;
        return status;
    }

    if (SDCARD_V2 == sd_card_p->state.card_type) {
        // Get the card capacity CCS: CMD58
        if (SD_BLOCK_DEVICE_ERROR_NONE ==
            (status = sd_cmd(sd_card_p, CMD58_READ_OCR, 0x0, false, &response))) {
            // High Capacity card
            if (response & OCR_HCS_CCS) {
                sd_card_p->state.card_type = SDCARD_V2HC;
            } else {
            }
        }
    } else {
        sd_card_p->state.card_type = SDCARD_V1;
    }

    if (!crc_on) {
        status = sd_cmd(sd_card_p, CMD59_CRC_ON_OFF, 0, false, 0);
    }

    status = sd_cmd(sd_card_p, ACMD42_SET_CLR_CARD_DETECT, 0, true, NULL);
    return status;
}

/**
 * @brief Tests the communication with the SD card.
 *
 * This function is used to test the communication with the SD card. It first checks if the
 * SD card is already initialized, and if so, it sends a command to get the card status. If the
 * card status is not received, it assumes that the card is no longer present and sets the
 * `STA_NOINIT` flag in the card status. If the card is not initialized, it performs a light
 * version of the initialization to test the communication. It sends the initializing sequence
 * and waits for the card to go idle. If the card responds with a response status, it assumes
 * that the communication is successful and returns `true`. If the card does not respond, it
 * assumes that something is holding the DO line and returns `false`.
 *
 * @param sd_card_p Pointer to the SD card object.
 * @return `true` if the communication with the SD card is successful, `false` otherwise.
 */
static bool sd_spi_test_com(sd_card_t *sd_card_p) {
    // This is allowed to be called before initialization, so ensure mutex is created
    if (!mutex_is_initialized(&sd_card_p->state.mutex)) mutex_init(&sd_card_p->state.mutex);

    sd_acquire(sd_card_p);

    bool success = false;

    if (!(sd_card_p->state.m_Status & STA_NOINIT)) {
        // SD card is currently initialized

        // Timeout of 0 means only check once
        if (sd_wait_ready(sd_card_p, 0)) {
            // DO has been released, try to get status
            uint32_t response;
            for (unsigned i = 0; i < SD_COMMAND_RETRIES; i++) {
                // Send command over SPI interface
                response = sd_cmd_spi(sd_card_p, CMD13_SEND_STATUS, 0);
                if (R1_NO_RESPONSE != response) {
                    // Got a response!
                    success = true;
                    break;
                }
            }

            if (!success) {
                // Card no longer sensed - ensure card is initialized once re-attached
                sd_card_p->state.m_Status |= STA_NOINIT;
            }
        } else {
            // SD card is currently holding DO which is sufficient enough to know it's still
            // there
            success = true;
        }
    } else {
        // Do a "light" version of init, just enough to test com

        // Initialize the member variables
        sd_card_p->state.card_type = SDCARD_NONE;

        sd_spi_go_low_frequency(sd_card_p);
        sd_spi_send_initializing_sequence(sd_card_p);

        if (sd_wait_ready(sd_card_p, 0)) {
            // DO has been released, try to make SD card go idle
            uint32_t response;
            for (unsigned i = 0; i < SD_COMMAND_RETRIES; i++) {
                // Send command over SPI interface
                response = sd_cmd_spi(sd_card_p, CMD0_GO_IDLE_STATE, 0);
                if (R1_NO_RESPONSE != response) {
                    // Got a response!
                    success = true;
                    break;
                }
            }
        } else {
            // Something is holding DO - better to return false and allow user to try again
            // later
            success = false;
        }
    }

    sd_release(sd_card_p);

    return success;
}

/**
 * @brief Initializes the SD card over SPI.
 *
 * This function initializes the SD card over SPI. It first checks if the SD card is already
 * initialized, and if so, it returns the current status. If the card is not initialized, it
 * performs the initialization sequence and returns the status of the card. The card is
 * initialized by sending the initializing sequence, checking the card type, setting the SCK
 * for data transfer, and setting the block length to 512. The card is now considered initialized
 * and its status is returned.
 *
 * @param sd_card_p Pointer to the SD card object.
 * @return The status of the SD card:
 *  STA_NOINIT = 0x01, // Drive not initialized 
 *  STA_NODISK = 0x02, // No medium in the drive
 *  STA_PROTECT = 0x04 // Write protected
 */
DSTATUS sd_card_spi_init(sd_card_t *sd_card_p) {
    sd_lock(sd_card_p);
    sd_card_detect(sd_card_p);
    if (sd_card_p->state.m_Status & STA_NODISK) {
        sd_unlock(sd_card_p);
        return sd_card_p->state.m_Status;
    }
    if (!(sd_card_p->state.m_Status & STA_NOINIT)) {
        sd_unlock(sd_card_p);
        return sd_card_p->state.m_Status;
    }

    sd_card_p->state.card_type = SDCARD_NONE;

    sd_spi_acquire(sd_card_p);

    int err = sd_init_medium(sd_card_p);
    if (SD_BLOCK_DEVICE_ERROR_NONE != err) {
        sd_release(sd_card_p);
        return sd_card_p->state.m_Status;
    }

    if (SDCARD_V2HC != sd_card_p->state.card_type) {
        sd_release(sd_card_p);
        return sd_card_p->state.m_Status;
    }

    sd_spi_go_high_frequency(sd_card_p);

    sd_card_p->state.sectors = in_sd_spi_sectors(sd_card_p);
    if (0 == sd_card_p->state.sectors) {
        sd_release(sd_card_p);
        return sd_card_p->state.m_Status;
    }
    if (SD_BLOCK_DEVICE_ERROR_NONE != sd_cmd(sd_card_p, CMD10_SEND_CID, 0x0, false, 0)) {
        sd_release(sd_card_p);
        return sd_card_p->state.m_Status;
    }
    if (read_bytes(sd_card_p, (uint8_t *)&sd_card_p->state.CID, sizeof(CID_t)) != 0) {
        sd_release(sd_card_p);
        return sd_card_p->state.m_Status;
    }

    if (SD_BLOCK_DEVICE_ERROR_NONE != sd_cmd(sd_card_p, CMD16_SET_BLOCKLEN, sd_block_size, false, 0)) {
        sd_release(sd_card_p);
        return sd_card_p->state.m_Status;
    }

    sd_card_p->state.m_Status &= ~STA_NOINIT;
    sd_release(sd_card_p);
    return sd_card_p->state.m_Status;
}

static void sd_deinit(sd_card_t *sd_card_p) {
    sd_card_p->state.m_Status |= STA_NOINIT;
    sd_card_p->state.card_type = SDCARD_NONE;

    if ((uint)-1 != sd_card_p->spi_if_p->ss_gpio) {
        gpio_deinit(sd_card_p->spi_if_p->ss_gpio);
        gpio_set_dir(sd_card_p->spi_if_p->ss_gpio, GPIO_IN);
    }
}

void sd_spi_ctor(sd_card_t *sd_card_p) {
    sd_card_p->write_blocks = sd_write_blocks;
    sd_card_p->read_blocks = sd_read_blocks;
    sd_card_p->sync = sd_sync;
    sd_card_p->init = sd_card_spi_init;
    sd_card_p->deinit = sd_deinit;
    sd_card_p->get_num_sectors = sd_spi_sectors;
    sd_card_p->sd_test_com = sd_spi_test_com;

    if ((uint)-1 == sd_card_p->spi_if_p->ss_gpio) return;
    gpio_init(sd_card_p->spi_if_p->ss_gpio);
    gpio_put(sd_card_p->spi_if_p->ss_gpio, 1);
    gpio_set_dir(sd_card_p->spi_if_p->ss_gpio, GPIO_OUT);
    gpio_put(sd_card_p->spi_if_p->ss_gpio, 1);
    if (sd_card_p->spi_if_p->set_drive_strength) {
        gpio_set_drive_strength(sd_card_p->spi_if_p->ss_gpio,
                                sd_card_p->spi_if_p->ss_gpio_drive_strength);
    }
}
