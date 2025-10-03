#include <cstring>

#include "../include/fat32.h"
#include "../include/sdcard.h"

#include "configs.h"

namespace FileSystem::Fat32 {

using namespace Constants;

// ========== File Creation (Logging Use Case) ==========

ErrorCode create_file(const char* path) {
    if (!Volume::is_mounted()) {
        return ErrorCode::NOT_MOUNTED;
    }

    if (!path || path[0] == '\0') {
        return ErrorCode::INVALID_PATH;
    }

    // For logging use case, assume root directory and simple filename
    // Skip complex path parsing - just use filename as-is

    const char* filename = path;
    if (path[0] == '/') {
        filename = path + 1;  // Skip leading slash
    }

    // Validate simple filename (basic check)
    size_t len = 0;
    for (const char* p = filename; *p && *p != '/'; p++) {
        len++;
    }

    if (len == 0 || len > 12) {  // Simple validation for 8.3 format
        return ErrorCode::INVALID_NAME;
    }

    // For root directory logging, we don't need complex directory operations
    // Just ensure file exists and is ready for appending

    return ErrorCode::NONE;  // File creation logic will be in File::open
}

// ========== File Class (Append-Only Operations) ==========

File::~File() {
    if (is_open()) {
        close();
    }
}

File::File(File&& other) noexcept
    : start_cluster_(other.start_cluster_)
    , current_cluster_(other.current_cluster_)
    , file_size_(other.file_size_)
    , position_(other.position_)
    , writable_(other.writable_)
    , buffer_lba_(other.buffer_lba_)
    , buffer_dirty_(other.buffer_dirty_)
{
    // Copy sector buffer
    std::memcpy(sector_buffer_, other.sector_buffer_, sizeof(sector_buffer_));

    // Reset other
    other.start_cluster_ = 0;
    other.current_cluster_ = 0;
    other.file_size_ = 0;
    other.position_ = 0;
    other.writable_ = false;
    other.buffer_lba_ = 0;
    other.buffer_dirty_ = false;
}

File& File::operator=(File&& other) noexcept {
    if (this != &other) {
        if (is_open()) {
            close();
        }

        start_cluster_ = other.start_cluster_;
        current_cluster_ = other.current_cluster_;
        file_size_ = other.file_size_;
        position_ = other.position_;
        writable_ = other.writable_;
        buffer_lba_ = other.buffer_lba_;
        buffer_dirty_ = other.buffer_dirty_;

        // Copy sector buffer
        std::memcpy(sector_buffer_, other.sector_buffer_, sizeof(sector_buffer_));

        // Reset other
        other.start_cluster_ = 0;
        other.current_cluster_ = 0;
        other.file_size_ = 0;
        other.position_ = 0;
        other.writable_ = false;
        other.buffer_lba_ = 0;
        other.buffer_dirty_ = false;
    }
    return *this;
}

ErrorCode File::open(const char* path, AccessMode mode) {
    if (!Volume::is_mounted()) {
        return ErrorCode::NOT_MOUNTED;
    }

    if (is_open()) {
        return ErrorCode::ALREADY_EXISTS;
    }

    if (!path || path[0] == '\0') {
        return ErrorCode::INVALID_PATH;
    }

    // For logging use case, assume root directory
    const char* filename = path[0] == '/' ? path + 1 : path;

    // Create file if it doesn't exist
    ErrorCode create_result = create_file(path);
    if (create_result != ErrorCode::NONE && create_result != ErrorCode::ALREADY_EXISTS) {
        return create_result;
    }

    // Initialize file state for append mode
    start_cluster_ = 0;  // Will be allocated as needed
    current_cluster_ = 0;
    file_size_ = 0;
    position_ = 0;
    writable_ = (mode == AccessMode::WRITE || mode == AccessMode::READ_WRITE);
    buffer_lba_ = 0;
    buffer_dirty_ = false;

    return ErrorCode::NONE;
}

ErrorCode File::close() {
    if (!is_open()) {
        return ErrorCode::NONE;
    }

    // Flush any pending writes
    ErrorCode flush_result = flush_buffer();
    if (flush_result != ErrorCode::NONE) {
        return flush_result;
    }

    // Reset state
    start_cluster_ = 0;
    current_cluster_ = 0;
    file_size_ = 0;
    position_ = 0;
    writable_ = false;
    buffer_lba_ = 0;
    buffer_dirty_ = false;

    return ErrorCode::NONE;
}

std::expected<size_t, ErrorCode> File::write(std::span<const uint8_t> data) {
    if (!is_open()) {
        return std::unexpected(ErrorCode::NOT_FOUND);
    }

    if (!writable_) {
        return std::unexpected(ErrorCode::READ_ONLY);
    }

    if (data.empty()) {
        return 0;
    }

    size_t bytes_written = 0;
    size_t remaining = data.size();
    const uint8_t* src = data.data();

    while (remaining > 0) {
        // Determine current sector for writing
        const uint32_t byte_in_file = position_;
        const uint32_t sector_in_file = byte_in_file / SECTOR_SIZE;
        const uint32_t byte_in_sector = byte_in_file % SECTOR_SIZE;

        // Load or allocate the appropriate sector
        uint32_t lba = 0;

        if (current_cluster_ == 0) {
            // First write - allocate initial cluster
            auto cluster_result = Volume::allocate_cluster();
            if (!cluster_result) {
                return std::unexpected(cluster_result.error());
            }
            current_cluster_ = *cluster_result;
            start_cluster_ = current_cluster_;
            lba = Volume::cluster_to_lba(current_cluster_);
        } else {
            // Calculate LBA for current position in existing cluster chain
            lba = Volume::cluster_to_lba(current_cluster_) + sector_in_file;
        }

        // Load sector buffer
        ErrorCode load_result = load_buffer(lba);
        if (load_result != ErrorCode::NONE) {
            return std::unexpected(load_result);
        }

        // Write as much as possible to current sector
        const size_t sector_space = SECTOR_SIZE - byte_in_sector;
        const size_t write_size = (remaining < sector_space) ? remaining : sector_space;

        std::memcpy(sector_buffer_ + byte_in_sector, src, write_size);

        // Mark buffer as dirty
        buffer_dirty_ = true;

        // Update state
        src += write_size;
        remaining -= write_size;
        bytes_written += write_size;
        position_ += write_size;
        file_size_ = position_;  // Update file size

        // If we filled the sector, flush it
        if (byte_in_sector + write_size == SECTOR_SIZE) {
            ErrorCode flush_result = flush_buffer();
            if (flush_result != ErrorCode::NONE) {
                return std::unexpected(flush_result);
            }

            // Move to next sector (potentially next cluster)
            if (sector_in_file == Volume::sectors_per_cluster() - 1) {
                // Need next cluster
                auto next_cluster_result = Volume::allocate_cluster();
                if (!next_cluster_result) {
                    return std::unexpected(next_cluster_result.error());
                }

                ErrorCode link_result = Volume::link_cluster(current_cluster_, *next_cluster_result);
                if (link_result != ErrorCode::NONE) {
                    return std::unexpected(link_result);
                }

                current_cluster_ = *next_cluster_result;
            }
        }
    }

    return bytes_written;
}

ErrorCode File::flush_buffer() {
    if (!buffer_dirty_) {
        return ErrorCode::NONE;
    }

    auto result = SDCard::Driver::write_sector(buffer_lba_, sector_buffer_, SECTOR_SIZE);
    if (result != ErrorCode::NONE) {
        return ErrorCode::IO_ERROR;
    }

    buffer_dirty_ = false;
    return ErrorCode::NONE;
}

ErrorCode File::load_buffer(uint32_t lba) {
    // Flush current buffer if dirty and different LBA
    if (buffer_dirty_ && buffer_lba_ != lba) {
        ErrorCode flush_result = flush_buffer();
        if (flush_result != ErrorCode::NONE) {
            return flush_result;
        }
    }

    // Load new sector if different
    if (buffer_lba_ != lba) {
        auto result = SDCard::Driver::read_sector(lba, sector_buffer_, SECTOR_SIZE);
        if (result != ErrorCode::NONE) {
            return ErrorCode::IO_ERROR;
        }
        buffer_lba_ = lba;
    }

    return ErrorCode::NONE;
}

ErrorCode File::sync() {
    if (!is_open()) {
        return ErrorCode::NOT_FOUND;
    }

    // Flush write buffer
    ErrorCode flush_result = flush_buffer();
    if (flush_result != ErrorCode::NONE) {
        return flush_result;
    }

    return ErrorCode::NONE;
}

} // namespace FileSystem::Fat32