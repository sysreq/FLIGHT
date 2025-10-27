#pragma once

// ============================================
// STANDARD LIBRARY INCLUDES
// ============================================
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdarg>
#include <cstdio>
#include <array>
#include <algorithm>
#include <concepts>
#include <type_traits>
#include <utility>

// ============================================
// PICO SDK INCLUDES
// ============================================
#include "pico/stdlib.h"
#include "pico/time.h"
#include "pico/mutex.h"
#include "hardware/spi.h"
#include "hardware/dma.h"
#include "hardware/irq.h"

// ============================================
// FATFS C LIBRARY INCLUDES
// ============================================
extern "C" {
    #include "ff.h"
    #include "f_util.h"
    #include "sd_card.h"
}

// ============================================
// C INTERFACE FUNCTIONS (need to be declared before class)
// ============================================
extern "C" {
    size_t sd_get_num();
    sd_card_t* sd_get_by_num(size_t num);
}

namespace sdcard {

// ============================================
// HARDWARE CONFIGURATION
// ============================================
namespace hw {
    inline spi_inst_t* const SPI_BUS = spi0;
    inline constexpr uint8_t MISO = 0;
    inline constexpr uint8_t CS = 1;
    inline constexpr uint8_t SCK = 2;
    inline constexpr uint8_t MOSI = 3;
    inline constexpr uint32_t SPI_FREQ_HZ = 31250000;  // 31.25MHz
    
    // DMA Configuration
    inline constexpr uint8_t DMA_IRQ_PRIORITY = 0;
    inline constexpr size_t MAX_DMA_CHANNELS = 2;  // One for read, one for write
}

// ============================================
// SYSTEM CONFIGURATION
// ============================================
namespace sys {
    inline constexpr size_t MAX_OPEN_FILES = 8;
    inline constexpr size_t DEFAULT_BUFFER_SIZE = 512;
    inline constexpr uint32_t DEFAULT_SYNC_TIME_MS = 5000;
    inline constexpr size_t WRITE_QUEUE_SIZE = 16; 
    inline constexpr uint32_t MOUNT_RETRY_DELAY_MS = 100;
    inline constexpr uint8_t MOUNT_MAX_RETRIES = 3;
}

// ============================================
// FILE TYPE IDENTIFIERS
// ============================================
struct LogFile {};
struct Force {};
struct Current {};
struct Speed {};

// ============================================
// FILE TRAITS TEMPLATE
// ============================================
template<typename FileType>
struct FileTraits {
    static_assert(sizeof(FileType) == -1, "FileTraits must be specialized for each file type");
};

template<>
struct FileTraits<LogFile> {
    static constexpr const char* name = "system.log";
    static constexpr uint32_t sync_time_ms = 2500;
    static constexpr size_t buffer_size = 1024;
    static constexpr bool append_mode = true;
    static constexpr bool auto_sync = true;
    static constexpr bool use_dma = false;  // Log files don't need DMA
};

template<>
struct FileTraits<Force> {
    static constexpr const char* name = "load_cell.txt";
    static constexpr uint32_t sync_time_ms = 2500;
    static constexpr size_t buffer_size = 1024;
    static constexpr bool append_mode = true;
    static constexpr bool auto_sync = true;
    static constexpr bool use_dma = false;  // Log files don't need DMA
};

template<>
struct FileTraits<Current> {
    static constexpr const char* name = "power_sensor.txt";
    static constexpr uint32_t sync_time_ms = 2500;
    static constexpr size_t buffer_size = 1024;
    static constexpr bool append_mode = true;
    static constexpr bool auto_sync = true;
    static constexpr bool use_dma = false;  // Log files don't need DMA
};

template<>
struct FileTraits<Speed> {
    static constexpr const char* name = "air_speed.txt";
    static constexpr uint32_t sync_time_ms = 2500;
    static constexpr size_t buffer_size = 1024;
    static constexpr bool append_mode = true;
    static constexpr bool auto_sync = true;
    static constexpr bool use_dma = false;  // Log files don't need DMA
};


// ============================================
// TYPE DEFINITIONS
// ============================================
using FileHandle = FIL*;
using DirHandle = DIR*;
using FileResult = FRESULT;

// ============================================
// WRITE REQUEST STRUCTURE (for async writes)
// ============================================
struct WriteRequest {
    const uint8_t* data;
    size_t length;
    absolute_time_t timestamp;
    
    WriteRequest() : data(nullptr), length(0), timestamp(0) {}
    WriteRequest(const uint8_t* d, size_t l) 
        : data(d), length(l), timestamp(get_absolute_time()) {}
};

// ============================================
// ERROR REPORTING
// ============================================
inline bool report_error(const char* function, const char* message, FRESULT fr = FR_OK) {
    if (fr != FR_OK) {
        printf("[SD] %s failed: %s (FR=%d)\n", function, message, fr);
    } else {
        printf("[SD] %s failed: %s\n", function, message);
    }
    return false;
}

inline bool report_success(const char* function, const char* message = nullptr) {
    if (message) {
        printf("[SD] %s: %s\n", function, message);
    }
    return true;
}

// ============================================
// UTILITY FUNCTIONS
// ============================================
template<typename T>
concept FileTypeValid = requires {
    FileTraits<T>::name;
    FileTraits<T>::sync_time_ms;
    FileTraits<T>::buffer_size;
    FileTraits<T>::append_mode;
    FileTraits<T>::auto_sync;
    FileTraits<T>::use_dma;
};

// Helper to get aligned buffer size
inline constexpr size_t align_buffer_size(size_t size) {
    // Align to 512-byte sectors for efficiency
    return ((size + 511) / 512) * 512;
}

// Time helpers
inline uint32_t time_since_ms(absolute_time_t start) {
    return absolute_time_diff_us(start, get_absolute_time()) / 1000;
}

inline bool time_reached(absolute_time_t target) {
    return absolute_time_diff_us(get_absolute_time(), target) <= 0;
}

// FatFS result checker
inline bool check_fresult(FRESULT fr, const char* operation) {
    if (fr != FR_OK) {
        return report_error(operation, "FatFS error", fr);
    }
    return true;
}

} // namespace sdcard