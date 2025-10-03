#include <cstring>

#include "../include/fat32.h"
#include "../include/sdcard.h"
#include "configs.h"
#include "utils.h"

namespace FileSystem::Fat32 {

using namespace Constants;

// ========== Static Member Definitions ==========

uint32_t Volume::fat_begin_lba_ = 0;
uint32_t Volume::data_begin_lba_ = 0;
uint8_t Volume::sectors_per_cluster_ = 0;
uint32_t Volume::root_cluster_ = 0;
uint8_t Volume::sector_buf_[SECTOR_SIZE];

// ========== Volume Implementation ==========

ErrorCode Volume::mount() {
    auto result = SDCard::Driver::init();
    if (result != ErrorCode::NONE) {
        return ErrorCode::IO_ERROR;
    }

    result = SDCard::Driver::read_sector(0, sector_buf_, SECTOR_SIZE);
    if (result != ErrorCode::NONE) {
        return ErrorCode::IO_ERROR;
    }

    if (Utils::read_u16(sector_buf_ + Constants::BS_55AA) != Constants::BOOT_SIGNATURE) {
        return ErrorCode::CORRUPT;
    }

    const uint16_t bytes_per_sector = Utils::read_u16(sector_buf_ + BPB_BytsPerSec);
    if (bytes_per_sector != SECTOR_SIZE) {
        return ErrorCode::CORRUPT;
    }

    sectors_per_cluster_ = sector_buf_[BPB_SecPerClus];
    const uint16_t reserved_sector_count = Utils::read_u16(sector_buf_ + BPB_RsvdSecCnt);
    const uint8_t fat_count = sector_buf_[BPB_NumFATs];
    const uint32_t sectors_per_fat = Utils::read_u32(sector_buf_ + BPB_FATSz32);

    root_cluster_ = Utils::read_u32(sector_buf_ + BPB_RootClus32);
    fat_begin_lba_ = reserved_sector_count;
    data_begin_lba_ = reserved_sector_count + (fat_count * sectors_per_fat);

    return ErrorCode::NONE;
}

void Volume::unmount() {
    fat_begin_lba_ = 0;
    data_begin_lba_ = 0;
    sectors_per_cluster_ = 0;
    root_cluster_ = 0;
}

bool Volume::is_mounted() {
    return data_begin_lba_ != 0;
}

std::expected<std::span<uint8_t>, ErrorCode> Volume::load_sector(uint32_t lba) {
    auto result = SDCard::Driver::read_sector(lba, sector_buf_, SECTOR_SIZE);
    if (result != ErrorCode::NONE) {
        return std::unexpected(ErrorCode::IO_ERROR);
    }
    return std::span<uint8_t>(sector_buf_, SECTOR_SIZE);
}

std::expected<uint32_t, ErrorCode> Volume::next_cluster(uint32_t cluster) {
    if (cluster < Constants::MIN_VALID_CLUSTER) {
        return std::unexpected(ErrorCode::CORRUPT);
    }

    const uint32_t fat_byte_offset = cluster * Constants::FAT_ENTRY_SIZE;
    const uint32_t fat_sector_lba = fat_begin_lba_ + (fat_byte_offset / SECTOR_SIZE);
    const uint32_t sector_offset = fat_byte_offset % SECTOR_SIZE;

    auto result = SDCard::Driver::read_sector(fat_sector_lba, sector_buf_, SECTOR_SIZE);
    if (result != ErrorCode::NONE) {
        return std::unexpected(ErrorCode::IO_ERROR);
    }

    const uint32_t next_cluster_value = Utils::read_u32(sector_buf_ + sector_offset) & FAT32_MASK;

    if (next_cluster_value >= FAT32_EOC) {
        return std::unexpected(ErrorCode::EOF_REACHED);
    }

    if (next_cluster_value == FAT32_BAD) {
        return std::unexpected(ErrorCode::CORRUPT);
    }

    return next_cluster_value;
}

std::expected<uint32_t, ErrorCode> Volume::allocate_cluster() {
    for (uint32_t cluster = Constants::MIN_VALID_CLUSTER; cluster < Constants::MAX_FAT32; cluster++) {
        const uint32_t fat_byte_offset = cluster * Constants::FAT_ENTRY_SIZE;
        const uint32_t fat_sector_lba = fat_begin_lba_ + (fat_byte_offset / SECTOR_SIZE);
        const uint32_t sector_offset = fat_byte_offset % SECTOR_SIZE;

        auto result = SDCard::Driver::read_sector(fat_sector_lba, sector_buf_, SECTOR_SIZE);
        if (result != ErrorCode::NONE) {
            return std::unexpected(ErrorCode::IO_ERROR);
        }

        const uint32_t cluster_value = Utils::read_u32(sector_buf_ + sector_offset) & FAT32_MASK;

        if (cluster_value == FAT32_FREE) {
            Utils::write_u32(sector_buf_ + sector_offset, FAT32_EOC);
            result = SDCard::Driver::write_sector(fat_sector_lba, sector_buf_, SECTOR_SIZE);
            if (result != ErrorCode::NONE) {
                return std::unexpected(ErrorCode::IO_ERROR);
            }
            return cluster;
        }
    }

    return std::unexpected(ErrorCode::NO_SPACE);
}

ErrorCode Volume::free_cluster(uint32_t cluster) {
    if (cluster < Constants::MIN_VALID_CLUSTER) {
        return ErrorCode::CORRUPT;
    }

    const uint32_t fat_byte_offset = cluster * Constants::FAT_ENTRY_SIZE;
    const uint32_t fat_sector_lba = fat_begin_lba_ + (fat_byte_offset / SECTOR_SIZE);
    const uint32_t sector_offset = fat_byte_offset % SECTOR_SIZE;

    auto result = SDCard::Driver::read_sector(fat_sector_lba, sector_buf_, SECTOR_SIZE);
    if (result != ErrorCode::NONE) {
        return ErrorCode::IO_ERROR;
    }

    Utils::write_u32(sector_buf_ + sector_offset, FAT32_FREE);

    result = SDCard::Driver::write_sector(fat_sector_lba, sector_buf_, SECTOR_SIZE);
    if (result != ErrorCode::NONE) {
        return ErrorCode::IO_ERROR;
    }

    return ErrorCode::NONE;
}

uint32_t Volume::cluster_to_lba(uint32_t cluster) {
    if (cluster < Constants::MIN_VALID_CLUSTER) {
        return 0;
    }
    return data_begin_lba_ + ((cluster - Constants::MIN_VALID_CLUSTER) * sectors_per_cluster_);
}

ErrorCode Volume::link_cluster(uint32_t cluster, uint32_t next_cluster) {
    if (cluster < Constants::MIN_VALID_CLUSTER) {
        return ErrorCode::CORRUPT;
    }

    const uint32_t fat_byte_offset = cluster * Constants::FAT_ENTRY_SIZE;
    const uint32_t fat_sector_lba = fat_begin_lba_ + (fat_byte_offset / SECTOR_SIZE);
    const uint32_t sector_offset = fat_byte_offset % SECTOR_SIZE;

    auto result = SDCard::Driver::read_sector(fat_sector_lba, sector_buf_, SECTOR_SIZE);
    if (result != ErrorCode::NONE) {
        return ErrorCode::IO_ERROR;
    }

    Utils::write_u32(sector_buf_ + sector_offset, next_cluster & FAT32_MASK);

    result = SDCard::Driver::write_sector(fat_sector_lba, sector_buf_, SECTOR_SIZE);
    if (result != ErrorCode::NONE) {
        return ErrorCode::IO_ERROR;
    }

    return ErrorCode::NONE;
}

} // namespace FileSystem::Fat32
