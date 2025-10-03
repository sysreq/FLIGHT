#pragma once

#include "hardware/spi.h"

namespace FileSystem::SDCard {

// ========== SPI Configuration ==========

struct SPIConfig {
    // SPI Hardware Instance
    static inline spi_inst_t* SPI_INSTANCE = spi0;

    // GPIO Pin Assignments
    static constexpr uint8_t PIN_MISO = 16;
    static constexpr uint8_t PIN_CS   = 17;
    static constexpr uint8_t PIN_SCK  = 18;
    static constexpr uint8_t PIN_MOSI = 19;

    // Clock Frequencies
    static constexpr uint32_t FREQ_INIT_HZ = 400 * 1000;      // 400 kHz for initialization
    static constexpr uint32_t FREQ_NORMAL_HZ = 12'500'000;    // 12.5 MHz for normal operation

    // Timeouts (in milliseconds)
    static constexpr uint32_t TIMEOUT_INIT_MS = 1000;
    static constexpr uint32_t TIMEOUT_COMMAND_MS = 500;
    static constexpr uint32_t TIMEOUT_READ_MS = 200;
    static constexpr uint32_t TIMEOUT_WRITE_MS = 500;

    // Retry Counts
    static constexpr uint8_t MAX_INIT_RETRIES = 10;
    static constexpr uint8_t MAX_CMD_RETRIES = 3;
};

namespace Constants {
// ========== SD Card Constants (moved from spi_protocol.h) ==========

static constexpr size_t SECTOR_SIZE = 512;
static constexpr uint8_t SPI_FILL = 0xFF;

// Data Tokens
static constexpr uint8_t DATA_START_TOKEN = 0xFE;
static constexpr uint8_t DATA_ACCEPTED = 0x05;
static constexpr uint8_t DATA_ERROR_MASK = 0x1F;

} // namespace Constants

} // namespace FileSystem::SDCard
