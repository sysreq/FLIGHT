#pragma once

#ifdef __cplusplus
extern "C" {
#endif

static const size_t sd_block_size = 512;

typedef enum {
    SD_BLOCK_DEVICE_ERROR_NONE = 0,
    SD_BLOCK_DEVICE_ERROR_WOULD_BLOCK = 1 << 0,     /*!< operation would block */
    SD_BLOCK_DEVICE_ERROR_UNSUPPORTED = 1 << 1,     /*!< unsupported operation */
    SD_BLOCK_DEVICE_ERROR_PARAMETER = 1 << 2,       /*!< invalid parameter */
    SD_BLOCK_DEVICE_ERROR_NO_INIT = 1 << 3,         /*!< uninitialized */
    SD_BLOCK_DEVICE_ERROR_NO_DEVICE = 1 << 4,       /*!< device is missing or not connected */
    SD_BLOCK_DEVICE_ERROR_WRITE_PROTECTED = 1 << 5, /*!< write protected */
    SD_BLOCK_DEVICE_ERROR_UNUSABLE = 1 << 6,        /*!< unusable card */
    SD_BLOCK_DEVICE_ERROR_NO_RESPONSE = 1 << 7,     /*!< No response from device */
    SD_BLOCK_DEVICE_ERROR_CRC = 1 << 8,             /*!< CRC error */
    SD_BLOCK_DEVICE_ERROR_ERASE = 1 << 9,           /*!< Erase error: reset/sequence */
    SD_BLOCK_DEVICE_ERROR_WRITE = 1 << 10           /*!< Write error: !SPI_DATA_ACCEPTED */
} block_dev_err_t;

typedef enum {
    SDCARD_NONE = 0, /**< No card is present */
    SDCARD_V1 = 1,   /**< v1.x Standard Capacity */
    SDCARD_V2 = 2,   /**< v2.x Standard capacity SD card */
    SDCARD_V2HC = 3, /**< v2.x High capacity SD card */
    CARD_UNKNOWN = 4 /**< Unknown or unsupported card */
} card_type_t;

typedef enum {                          /* Number on wire in parens */
    CMD_NOT_SUPPORTED = -1,             /* Command not supported error */
    CMD0_GO_IDLE_STATE = 0,             /* Resets the SD Memory Card */
    CMD1_SEND_OP_COND = 1,              /* Sends host capacity support */
    CMD2_ALL_SEND_CID = 2,              /* Asks any card to send the CID. */
    CMD3_SEND_RELATIVE_ADDR = 3,        /* Ask the card to publish a new RCA. */
    CMD6_SWITCH_FUNC = 6,               /* Check and Switches card function */
    CMD7_SELECT_CARD = 7,               /* SELECT/DESELECT_CARD - toggles between the stand-by and transfer states. */
    CMD8_SEND_IF_COND = 8,              /* Supply voltage info */
    CMD9_SEND_CSD = 9,                  /* Provides Card Specific data */
    CMD10_SEND_CID = 10,                /* Provides Card Identification */
    CMD12_STOP_TRANSMISSION = 12,       /* Forces the card to stop transmission */
    CMD13_SEND_STATUS = 13,             /* (0x4D) Card responds with status */
    CMD16_SET_BLOCKLEN = 16,            /* Length for SC card is set */
    CMD17_READ_SINGLE_BLOCK = 17,       /* (0x51) Read single block of data */
    CMD18_READ_MULTIPLE_BLOCK = 18,     
    CMD24_WRITE_BLOCK = 24,             /* (0x58) Write single block of data */
    CMD25_WRITE_MULTIPLE_BLOCK = 25,
    CMD27_PROGRAM_CSD = 27,             /* Programming bits of CSD */
    CMD32_ERASE_WR_BLK_START_ADDR = 32, 
    CMD33_ERASE_WR_BLK_END_ADDR = 33,  
    CMD38_ERASE = 38,                   /* Erases all previously selected write blocks */
    CMD55_APP_CMD = 55,                 /* Extend to Applications specific commands */
    CMD56_GEN_CMD = 56,                 /* General Purpose Command */
    CMD58_READ_OCR = 58,                /* Read OCR register of card */
    CMD59_CRC_ON_OFF = 59,              /* Turns the CRC option on or off*/
   
    ACMD6_SET_BUS_WIDTH = 6,
    ACMD13_SD_STATUS = 13,
    ACMD22_SEND_NUM_WR_BLOCKS = 22,
    ACMD23_SET_WR_BLK_ERASE_COUNT = 23,
    ACMD41_SD_SEND_OP_COND = 41,
    ACMD42_SET_CLR_CARD_DETECT = 42,
    ACMD51_SEND_SCR = 51,
} cmdSupported;

#ifdef __cplusplus
}
#endif
