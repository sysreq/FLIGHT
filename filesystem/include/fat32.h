#pragma once

#include <cstdint>
#include <cstddef>
#include <expected>
#include <span>

#include "error.h"

namespace FileSystem::Fat32 {

// ========== Types ==========

enum class AccessMode : uint8_t {
    READ = 0x01,
    WRITE = 0x02,
    READ_WRITE = 0x03,
};


class Volume {
private:
    static uint32_t fat_begin_lba_;
    static uint32_t data_begin_lba_;
    static uint8_t sectors_per_cluster_;
    static uint32_t root_cluster_;
    static uint8_t sector_buf_[512];

public:
    static ErrorCode mount();
    static void unmount();
    static bool is_mounted();

    static std::expected<std::span<uint8_t>, ErrorCode> load_sector(uint32_t lba);
    static std::expected<uint32_t, ErrorCode> next_cluster(uint32_t cluster);
    static std::expected<uint32_t, ErrorCode> allocate_cluster();
    static ErrorCode free_cluster(uint32_t cluster);

    static uint32_t cluster_to_lba(uint32_t cluster);
    static uint32_t root_cluster() { return root_cluster_; }
    static uint8_t sectors_per_cluster() { return sectors_per_cluster_; }

    // Link cluster in FAT chain
    static ErrorCode link_cluster(uint32_t cluster, uint32_t next_cluster);
};


class File {
private:
    uint32_t start_cluster_{0};
    uint32_t current_cluster_{0};
    size_t file_size_{0};
    size_t position_{0};
    bool writable_{false};

    uint8_t sector_buffer_[512];
    uint32_t buffer_lba_{0};
    bool buffer_dirty_{false};

public:
    File() = default;
    ~File();

    File(const File&) = delete;
    File& operator=(const File&) = delete;
    File(File&& other) noexcept;
    File& operator=(File&& other) noexcept;

    ErrorCode open(const char* path, AccessMode mode);
    ErrorCode close();

    std::expected<size_t, ErrorCode> read(std::span<uint8_t> buffer);
    std::expected<size_t, ErrorCode> write(std::span<const uint8_t> buffer);

    ErrorCode seek(size_t position);
    ErrorCode sync();

    bool is_open() const { return file_size_ != 0; }
    size_t size() const { return file_size_; }
    size_t tell() const { return position_; }

private:
    ErrorCode flush_buffer();
    ErrorCode load_buffer(uint32_t lba);
};

// ========== Path Utilities ==========

namespace Utils {
    // Parse 8.3 filename into FAT32 format (11 bytes, space-padded)
    ErrorCode parse_8_3_name(const char* filename, char out_name[11]);

    // Compare two FAT32 directory names (case-insensitive)
    bool names_match(const char fat_name[11], const char* filename);

    // Split path into parent and filename
    // Returns pointer to last path component (filename)
    // Modifies path by inserting null terminator before filename
    const char* split_path(char* path);
}

// ========== Helper Functions ==========

ErrorCode create_file(const char* path);
// delete_file, create_directory, delete_directory removed - not needed for logging use case

}  // namespace FileSystem::Fat32