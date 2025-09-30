#pragma once

#include <atomic>
#include <cstdint>
#include <cstring>

#include "hardware/spi.h"
#include "hardware/sync.h"

#include "pico/stdlib.h"
#include "pico/stdio.h"
#include "pico/mutex.h"

#include "ff.h"

namespace storage
{
    enum class DataType : uint8_t {
        IMU_READING     = 0x01,
        BARO_READING    = 0x02,
        AIR_READING     = 0x03,
        GPS_READING     = 0x04,
        MAG_READING     = 0x05,
        ERROR_MSG       = 0x10,
        LOG_MSG         = 0x11,
        SYSTEM_EVENT    = 0x20
    };

    struct Header {
        uint8_t magic;
        uint8_t length;
        uint8_t type;
        uint8_t reserved;
    } __attribute__((packed));

    namespace config {
        static constexpr size_t BUFFER_SIZE = 4096;
        static constexpr uint8_t MAGIC = 0xAA;
        static constexpr const char* FILENAME = "__data.bin";

        inline spi_inst_t* const SPI_BUS = spi0;
        static constexpr uint8_t MISO = 16;
        static constexpr uint8_t CS = 17;
        static constexpr uint8_t SCK = 18;
        static constexpr uint8_t MOSI = 19;
        static constexpr uint32_t SPI_FREQ_HZ = 31250000;
    } // namespace config
} // namespace storage