#pragma once

#include <cstdint>
#include <cstddef>
#include <cstdarg>

namespace FileSystem {

// ========== Data Types ==========

enum class DataType : uint8_t {
    IMU_DATA = 0,
    BAROMETER_DATA = 1,
    AIRSPEED_DATA = 2,
    LOADCELL_DATA = 3,
};

// ========== Status Structures ==========

struct FilesystemStatus {
    bool sd_ready_for_write;
    size_t buffer_bytes_used;
    size_t buffer_bytes_total;
    uint32_t total_bytes_written;
    uint32_t write_errors;
    uint32_t overflow_events;
};

struct SDStatus {
    bool present;
    bool mounted;
    uint32_t free_space_kb;
    uint32_t total_space_kb;
};

// ========== Lifecycle Management ==========

/**
 * Initialize the filesystem
 * - Initializes SD card hardware
 * - Mounts FAT32 volume if card present
 * - Sets up internal buffers
 *
 * @return true if initialization successful, false otherwise
 */
bool Init();

/**
 * Update filesystem state (call periodically from Core 0 main loop)
 * - Drains write buffer and commits to SD card
 * - Detects SD card insertion/removal
 * - Auto-remounts card when reinserted
 * - Flushes data periodically for durability
 */
void Update();

/**
 * Force flush all pending writes to SD card
 * Blocks until all buffered data is written
 */
void Flush();

/**
 * Shutdown filesystem gracefully
 * - Flushes all pending writes
 * - Closes open files
 * - Unmounts volume
 */
void Shutdown();

// ========== Write Operations (Thread-Safe) ==========

/**
 * Write binary sensor data
 * Thread-safe: Can be called from any core
 *
 * @param type Data type identifier
 * @param data Pointer to data buffer
 * @param size Size of data in bytes
 * @return true if data queued successfully, false if buffer full
 */
bool Write(DataType type, const void* data, size_t size);

/**
 * Write formatted text to log file
 * Thread-safe: Can be called from any core
 *
 * @param format Printf-style format string
 * @param ... Variable arguments for format string
 */
void Log(const char* format, ...);

// ========== Status Queries ==========

/**
 * Get current filesystem status
 * @return FilesystemStatus structure with buffer and write statistics
 */
FilesystemStatus GetStatus();

/**
 * Get current SD card status
 * @return SDStatus structure with card presence and capacity info
 */
SDStatus GetSDStatus();

} // namespace FileSystem
