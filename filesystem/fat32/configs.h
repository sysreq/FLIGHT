#pragma once

#include <cstdint>
#include <cstddef>

namespace FileSystem::Fat32 {
    static constexpr size_t MAX_FILENAME = 12;  // 8.3 (e.g., "FILE.TXT" = 8+1+3)
    static constexpr uint16_t SECTOR_SIZE = 512;

    namespace Constants {
    static constexpr uint8_t FS_FAT32 = 3;

    static constexpr uint32_t FAT32_MASK = 0x0FFFFFFF;
    static constexpr uint32_t FAT32_EOC = 0x0FFFFFF8;    // End of cluster chain
    static constexpr uint32_t FAT32_BAD = 0x0FFFFFF7;    // Bad cluster marker
    static constexpr uint32_t FAT32_FREE = 0x00000000;   // Free cluster

    static constexpr uint32_t MIN_FAT32 = 0x00010000;    // Minimum FAT32 clusters
    static constexpr uint32_t MAX_FAT32 = 0x0FFFFFF5;    // Maximum FAT32 clusters

    static constexpr uint8_t DDEM = 0xE5;     // Deleted directory entry mark
    static constexpr uint8_t RDDEM = 0x05;    // Replacement of character that collides with DDEM
    static constexpr uint8_t AM_LFN = 0x0F;   // LFN entry attribute
    static constexpr uint32_t MAX_DIR = 0x200000;  // Max directory index

    static constexpr uint32_t BS_JmpBoot      = 0;
    static constexpr uint32_t BS_OEMName      = 3;
    static constexpr uint32_t BS_55AA         = 510;
    static constexpr uint32_t BPB_BytsPerSec  = 11;
    static constexpr uint32_t BPB_SecPerClus  = 13;
    static constexpr uint32_t BPB_RsvdSecCnt  = 14;
    static constexpr uint32_t BPB_NumFATs     = 16;
    static constexpr uint32_t BPB_RootEntCnt  = 17;
    static constexpr uint32_t BPB_TotSec16    = 19;
    static constexpr uint32_t BPB_Media       = 21;
    static constexpr uint32_t BPB_FATSz16     = 22;
    static constexpr uint32_t BPB_SecPerTrk   = 24;
    static constexpr uint32_t BPB_NumHeads    = 26;
    static constexpr uint32_t BPB_HiddSec     = 28;
    static constexpr uint32_t BPB_TotSec32    = 32;

    static constexpr uint32_t BPB_FATSz32     = 36;
    static constexpr uint32_t BPB_ExtFlags    = 40;
    static constexpr uint32_t BPB_FSVer32     = 42;
    static constexpr uint32_t BPB_FSVer       = 42;  // Alias
    static constexpr uint32_t BPB_RootClus32  = 44;
    static constexpr uint32_t BPB_RootClus    = 44;  // Alias
    static constexpr uint32_t BPB_FSInfo32    = 48;
    static constexpr uint32_t BPB_FSInfo      = 48;  // Alias
    static constexpr uint32_t BPB_BkBootSec   = 50;
    static constexpr uint32_t BS_DrvNum32     = 64;
    static constexpr uint32_t BS_BootSig32    = 66;
    static constexpr uint32_t BS_VolID32      = 67;
    static constexpr uint32_t BS_VolLab32     = 71;
    static constexpr uint32_t BS_FilSysType32 = 82;

    static constexpr uint32_t FSI_LeadSig     = 0;
    static constexpr uint32_t FSI_StrucSig    = 484;
    static constexpr uint32_t FSI_Free_Count  = 488;
    static constexpr uint32_t FSI_Nxt_Free    = 492;

    static constexpr uint32_t DIR_Name        = 0;
    static constexpr uint32_t DIR_Attr        = 11;
    static constexpr uint32_t DIR_NTres       = 12;
    static constexpr uint32_t DIR_CrtTime     = 14;
    static constexpr uint32_t DIR_CrtDate     = 16;
    static constexpr uint32_t DIR_LstAccDate  = 18;
    static constexpr uint32_t DIR_FstClusHI   = 20;
    static constexpr uint32_t DIR_ModTime     = 22;
    static constexpr uint32_t DIR_ModDate     = 24;
    static constexpr uint32_t DIR_FstClusLO   = 26;
    static constexpr uint32_t DIR_FileSize    = 28;

    static constexpr uint32_t SZDIRE = 32;

    // Additional constants needed for volume operations
    static constexpr uint16_t BOOT_SIGNATURE = 0xAA55;
    static constexpr uint32_t MIN_VALID_CLUSTER = 2;
    static constexpr uint32_t FAT_ENTRY_SIZE = 4;
    } // namespace Constants
} // namespace FileSystem::FAT