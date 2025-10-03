#include "filesystem.h"
#include "include/fat32.h"
#include "include/sdcard.h"

#include "pico/stdlib.h"
#include "pico/sync.h"
#include "hardware/timer.h"

#include <cstdio>
#include <cstring>

namespace FileSystem {

// ========== Configuration ==========

static constexpr size_t RING_BUFFER_SIZE = 32768; // 32KB ring buffer for cross-core writes (4x increase)
static constexpr size_t MAX_LOG_ENTRY = 256;      // Max single log message size
static constexpr size_t MAX_DATA_ENTRY = 128;     // Max single binary data write
static constexpr uint32_t SD_POLL_INTERVAL_MS = 500;
static constexpr uint32_t FLUSH_INTERVAL_MS = 1000;

// ========== File Write Buffers ==========

static constexpr size_t FILE_BUFFER_SIZE = 8192;  // 8KB write buffer per file (16 sectors)
static uint8_t log_file_buffer[FILE_BUFFER_SIZE];
static uint8_t sensor_file_buffers[4][FILE_BUFFER_SIZE];
static size_t log_buffer_pos = 0;
static size_t sensor_buffer_pos[4] = {0, 0, 0, 0};
static bool log_buffer_dirty = false;
static bool sensor_buffer_dirty[4] = {false, false, false, false};

// ========== Ring Buffer Entry ==========

enum class EntryType : uint8_t {
    NONE = 0,
    LOG_MESSAGE = 1,
    BINARY_DATA = 2,
};

struct RingEntry {
    EntryType type;
    DataType data_type;
    uint16_t size;
    uint8_t data[MAX_DATA_ENTRY];
};

// ========== Lock-Free Ring Buffer ==========

static RingEntry ring_buffer[RING_BUFFER_SIZE / sizeof(RingEntry)];
static constexpr size_t RING_CAPACITY = sizeof(ring_buffer) / sizeof(RingEntry);
static volatile uint32_t ring_head = 0;  // Written by producer cores
static volatile uint32_t ring_tail = 0;  // Written by consumer (Core 0)

// ========== State Variables ==========

static bool initialized = false;
static bool sd_mounted = false;
static uint32_t last_sd_poll_time = 0;
static uint32_t last_flush_time = 0;

static Fat32::File* log_file = nullptr;
static Fat32::File* sensor_files[4] = {nullptr, nullptr, nullptr, nullptr};

// Statistics
static uint32_t total_bytes_written = 0;
static uint32_t write_errors = 0;
static uint32_t overflow_events = 0;

// ========== Helper Functions ==========

static inline uint32_t ring_next(uint32_t index) {
    return (index + 1) % RING_CAPACITY;
}

static inline bool ring_is_full() {
    return ring_next(ring_head) == ring_tail;
}

static inline bool ring_is_empty() {
    return ring_head == ring_tail;
}

static bool ring_push(const RingEntry& entry) {
    if (ring_is_full()) {
        __atomic_fetch_add(&overflow_events, 1, __ATOMIC_SEQ_CST);
        return false;
    }

    uint32_t head = ring_head;
    ring_buffer[head] = entry;

    // Memory barrier before updating head
    __atomic_store_n(&ring_head, ring_next(head), __ATOMIC_RELEASE);

    return true;
}

static bool ring_pop(RingEntry& entry) {
    if (ring_is_empty()) {
        return false;
    }

    uint32_t tail = ring_tail;
    entry = ring_buffer[tail];

    // Update tail
    ring_tail = ring_next(tail);

    return true;
}

// ========== File Management ==========

static const char* get_filename_for_type(DataType type) {
    switch (type) {
        case DataType::IMU_DATA: return "imu.dat";
        case DataType::BAROMETER_DATA: return "baro.dat";
        case DataType::AIRSPEED_DATA: return "speed.dat";
        case DataType::LOADCELL_DATA: return "load.dat";
        default: return nullptr;
    }
}

static bool open_log_file() {
    if (log_file) {
        return true;
    }

    // Try to open existing log file
    log_file = new Fat32::File();
    ErrorCode result = log_file->open("/system.log", Fat32::AccessMode::WRITE);

    if (result == ErrorCode::NOT_FOUND) {
        // Create new log file
        result = Fat32::create_file("/system.log");
        if (result == ErrorCode::NONE) {
            result = log_file->open("/system.log", Fat32::AccessMode::WRITE);
        }
    }

    if (result != ErrorCode::NONE) {
        delete log_file;
        log_file = nullptr;
        return false;
    }

    // For append-only operations, start at end of file
    // No seek needed - file position will be set during first write
    return true;
}

static bool open_sensor_file(DataType type) {
    uint8_t index = static_cast<uint8_t>(type);
    if (index >= 4) {
        return false;
    }

    if (sensor_files[index]) {
        return true;
    }

    const char* filename = get_filename_for_type(type);
    if (!filename) {
        return false;
    }

    char path[32];
    snprintf(path, sizeof(path), "/%s", filename);

    sensor_files[index] = new Fat32::File();
    ErrorCode result = sensor_files[index]->open(path, Fat32::AccessMode::WRITE);

    if (result == ErrorCode::NOT_FOUND) {
        result = Fat32::create_file(path);
        if (result == ErrorCode::NONE) {
            result = sensor_files[index]->open(path, Fat32::AccessMode::WRITE);
        }
    }

    if (result != ErrorCode::NONE) {
        delete sensor_files[index];
        sensor_files[index] = nullptr;
        return false;
    }

    // For append-only operations, start at end of file
    // No seek needed - file position will be set during first write
    return true;
}

static void close_all_files() {
    if (log_file) {
        log_file->close();
        delete log_file;
        log_file = nullptr;
    }

    for (int i = 0; i < 4; i++) {
        if (sensor_files[i]) {
            sensor_files[i]->close();
            delete sensor_files[i];
            sensor_files[i] = nullptr;
        }
    }
}

static void flush_log_buffer();
static void flush_sensor_buffer(uint8_t index);

static void flush_all_files() {
    // Flush file buffers first
    flush_log_buffer();
    for (int i = 0; i < 4; i++) {
        flush_sensor_buffer(i);
    }

    // Then sync files
    if (log_file) {
        log_file->sync();
    }

    for (int i = 0; i < 4; i++) {
        if (sensor_files[i]) {
            sensor_files[i]->sync();
        }
    }
}

static void flush_log_buffer() {
    if (!log_buffer_dirty || !log_file) {
        return;
    }

    // Write buffer to file
    auto result = log_file->write(std::span<const uint8_t>(log_file_buffer, log_buffer_pos));
    if (result) {
        total_bytes_written += *result;
    } else {
        write_errors++;
    }

    log_buffer_pos = 0;
    log_buffer_dirty = false;
}

static void flush_sensor_buffer(uint8_t index) {
    if (!sensor_buffer_dirty[index] || !sensor_files[index]) {
        return;
    }

    // Write buffer to file
    auto result = sensor_files[index]->write(std::span<const uint8_t>(sensor_file_buffers[index], sensor_buffer_pos[index]));
    if (result) {
        total_bytes_written += *result;
    } else {
        write_errors++;
    }

    sensor_buffer_pos[index] = 0;
    sensor_buffer_dirty[index] = false;
}

static bool write_to_log_buffer(const uint8_t* data, size_t size) {
    if (log_buffer_pos + size > FILE_BUFFER_SIZE) {
        flush_log_buffer();
    }

    if (size > FILE_BUFFER_SIZE) {
        // Data too large for buffer, write directly
        if (log_file) {
            auto result = log_file->write(std::span<const uint8_t>(data, size));
            if (result) {
                total_bytes_written += *result;
                return true;
            }
        }
        return false;
    }

    // Copy to buffer
    std::memcpy(log_file_buffer + log_buffer_pos, data, size);
    log_buffer_pos += size;
    log_buffer_dirty = true;
    return true;
}

static bool write_to_sensor_buffer(uint8_t index, const uint8_t* data, size_t size) {
    if (sensor_buffer_pos[index] + size > FILE_BUFFER_SIZE) {
        flush_sensor_buffer(index);
    }

    if (size > FILE_BUFFER_SIZE) {
        // Data too large for buffer, write directly
        if (sensor_files[index]) {
            auto result = sensor_files[index]->write(std::span<const uint8_t>(data, size));
            if (result) {
                total_bytes_written += *result;
                return true;
            }
        }
        return false;
    }

    // Copy to buffer
    std::memcpy(sensor_file_buffers[index] + sensor_buffer_pos[index], data, size);
    sensor_buffer_pos[index] += size;
    sensor_buffer_dirty[index] = true;
    return true;
}

// ========== Public API Implementation ==========

bool Init() {
    if (initialized) {
        return true;
    }

    // Initialize SD card and mount FAT32
    ErrorCode result = Fat32::Volume::mount();
    if (result == ErrorCode::NONE) {
        sd_mounted = true;
    } else {
        sd_mounted = false;
    }

    // Initialize ring buffer
    ring_head = 0;
    ring_tail = 0;

    // Reset statistics
    total_bytes_written = 0;
    write_errors = 0;
    overflow_events = 0;

    initialized = true;
    last_sd_poll_time = time_us_32();
    last_flush_time = time_us_32();

    return true;
}

void Update() {
    if (!initialized) {
        return;
    }

    uint32_t now = time_us_32();

    // Check for SD card presence periodically
    if ((now - last_sd_poll_time) >= (SD_POLL_INTERVAL_MS * 1000)) {
        bool was_mounted = sd_mounted;

        if (Fat32::Volume::is_mounted()) {
            sd_mounted = true;
        } else {
            // Try to remount
            ErrorCode result = Fat32::Volume::mount();
            if (result == ErrorCode::NONE) {
                sd_mounted = true;
                // Reopen files after remount
                open_log_file();
            } else {
                sd_mounted = false;
                close_all_files();
            }
        }

        last_sd_poll_time = now;
    }

    // Process ring buffer entries
    if (sd_mounted) {
        RingEntry entry;
        int processed = 0;

        // Process up to 32 entries per update to avoid blocking too long
        while (processed < 32 && ring_pop(entry)) {
            processed++;

            if (entry.type == EntryType::LOG_MESSAGE) {
                if (open_log_file()) {
                    // Write to buffer instead of directly to file
                    if (!write_to_log_buffer(entry.data, entry.size)) {
                        write_errors++;
                    }
                } else {
                    write_errors++;
                }
            } else if (entry.type == EntryType::BINARY_DATA) {
                if (open_sensor_file(entry.data_type)) {
                    uint8_t index = static_cast<uint8_t>(entry.data_type);
                    // Write to buffer instead of directly to file
                    if (!write_to_sensor_buffer(index, entry.data, entry.size)) {
                        write_errors++;
                    }
                } else {
                    write_errors++;
                }
            }
        }
    }

    // Periodic flush
    if ((now - last_flush_time) >= (FLUSH_INTERVAL_MS * 1000)) {
        if (sd_mounted) {
            flush_all_files();
        }
        last_flush_time = now;
    }
}

void Flush() {
    if (!initialized || !sd_mounted) {
        return;
    }

    // Drain ring buffer and flush to file buffers
    RingEntry entry;
    while (ring_pop(entry)) {
        if (entry.type == EntryType::LOG_MESSAGE) {
            if (open_log_file()) {
                write_to_log_buffer(entry.data, entry.size);
            }
        } else if (entry.type == EntryType::BINARY_DATA) {
            if (open_sensor_file(entry.data_type)) {
                uint8_t index = static_cast<uint8_t>(entry.data_type);
                write_to_sensor_buffer(index, entry.data, entry.size);
            }
        }
    }

    // Flush all file buffers to disk
    flush_all_files();
}

void Shutdown() {
    if (!initialized) {
        return;
    }

    Flush();
    close_all_files();

    if (sd_mounted) {
        Fat32::Volume::unmount();
        sd_mounted = false;
    }

    initialized = false;
}

bool Write(DataType type, const void* data, size_t size) {
    if (!data || size == 0 || size > MAX_DATA_ENTRY) {
        return false;
    }

    RingEntry entry;
    entry.type = EntryType::BINARY_DATA;
    entry.data_type = type;
    entry.size = static_cast<uint16_t>(size);
    std::memcpy(entry.data, data, size);

    return ring_push(entry);
}

void Log(const char* format, ...) {
    if (!format) {
        return;
    }

    char buffer[MAX_LOG_ENTRY];
    va_list args;
    va_start(args, format);
    int written = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (written <= 0) {
        return;
    }

    // Add newline if not present
    size_t len = written;
    if (len < sizeof(buffer) - 1 && buffer[len - 1] != '\n') {
        buffer[len] = '\n';
        len++;
        buffer[len] = '\0';
    }

    RingEntry entry;
    entry.type = EntryType::LOG_MESSAGE;
    entry.data_type = DataType::IMU_DATA;  // Unused for log messages
    entry.size = static_cast<uint16_t>(len);
    std::memcpy(entry.data, buffer, len);

    ring_push(entry);
}

FilesystemStatus GetStatus() {
    FilesystemStatus status;
    status.sd_ready_for_write = sd_mounted;

    // Calculate ring buffer usage
    uint32_t head = ring_head;
    uint32_t tail = ring_tail;
    if (head >= tail) {
        status.buffer_bytes_used = (head - tail) * sizeof(RingEntry);
    } else {
        status.buffer_bytes_used = (RING_CAPACITY - tail + head) * sizeof(RingEntry);
    }

    status.buffer_bytes_total = RING_CAPACITY * sizeof(RingEntry);

    // Add file buffer usage to total
    size_t file_buffer_used = log_buffer_pos;
    for (int i = 0; i < 4; i++) {
        file_buffer_used += sensor_buffer_pos[i];
    }
    status.buffer_bytes_used += file_buffer_used;

    status.total_bytes_written = total_bytes_written;
    status.write_errors = write_errors;
    status.overflow_events = overflow_events;

    return status;
}

SDStatus GetSDStatus() {
    SDStatus status;
    status.present = SDCard::Driver::is_initialized();
    status.mounted = sd_mounted;

    // TODO: Get actual SD card capacity
    // For now, return dummy values
    status.free_space_kb = 1024 * 1024;  // 1GB
    status.total_space_kb = 8 * 1024 * 1024;  // 8GB

    return status;
}

} // namespace FileSystem
