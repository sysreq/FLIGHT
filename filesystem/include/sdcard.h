#pragma once

#include <cstdint>
#include <cstddef>
#include <expected>

#include "error.h"

namespace FileSystem::SDCard {

// ========== SD Card Commands and Responses (moved from spi_protocol.h) ==========

enum class Command : uint8_t {
    CMD0_GO_IDLE_STATE = 0,
    CMD8_SEND_IF_COND = 8,
    CMD16_SET_BLOCKLEN = 16,
    CMD17_READ_SINGLE_BLOCK = 17,
    CMD24_WRITE_BLOCK = 24,
    CMD55_APP_CMD = 55,
    CMD58_READ_OCR = 58,
    ACMD41_SD_SEND_OP_COND = 41,
};

enum class R1Response : uint8_t {
    READY = 0x00,
    IDLE = 0x01,
    ERASE_RESET = 0x02,
    ILLEGAL_COMMAND = 0x04,
    CRC_ERROR = 0x08,
    ERASE_SEQ_ERROR = 0x10,
    ADDRESS_ERROR = 0x20,
    PARAMETER_ERROR = 0x40,
};

// ========== Low-Level SPI Interface (moved from spi_protocol.h) ==========

namespace Protocol {
    void init_spi();
    void deinit_spi();
    void set_frequency(uint32_t freq_hz);

    void cs_select();
    void cs_deselect();

    uint8_t spi_transfer(uint8_t data);
    void spi_write(uint8_t data);
    uint8_t spi_read();
    void spi_write_block(const uint8_t* data, size_t len);
    void spi_read_block(uint8_t* data, size_t len);

    uint8_t send_command(Command cmd, uint32_t arg);
    uint8_t send_app_command(Command cmd, uint32_t arg);
    bool wait_ready(uint32_t timeout_ms);
    bool wait_token(uint8_t token, uint32_t timeout_ms);

    uint8_t crc7(const uint8_t* data, size_t len);
    uint16_t crc16(const uint8_t* data, size_t len);
} // namespace Protocol

class Driver {
private:
    static inline bool initialized_ = false;

public:
    static ErrorCode init();
    static ErrorCode deinit();

    static ErrorCode read_sector(uint32_t lba, uint8_t* buffer, size_t buffer_size);
    static ErrorCode write_sector(uint32_t lba, const uint8_t* buffer, size_t buffer_size);

    static bool is_initialized();
};

} // namespace FileSystem::SDCard
