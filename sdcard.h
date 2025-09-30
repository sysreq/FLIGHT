#pragma once

/**
 * @file sdcard.h
 * @brief Complete SD card filesystem for Pi Pico 2 telemetry systems
 * 
 * This is the single include file that provides the complete SD card abstraction
 * system for flight telemetry and logging.
 * 
 * Key Features:
 * - Template-based file management with compile-time configuration
 * - Simple non-template file class for runtime flexibility
 * - Buffered writes with manual sync control
 * - Directory management and filesystem operations
 * 
 * Example Usage:
 * 
 *   // Mount the SD card
 *   sdcard::SDCard::mount();
 * 
 *   // Template file with compile-time configuration (defined in sd_config.h)
 *   sdcard::SDFile<LogFile>::Open();          // Opens "system.log" with 1KB buffer
 *   sdcard::SDFile<TelemetryFile>::Open();    // Opens "telemetry.csv" with 2KB buffer
 * 
 *   // Simple runtime file for dynamic paths
 *   sdcard::SimpleFile datafile;
 *   datafile.open("/flights/001/data.bin", true);  // true = append mode
 * 
 *   // Write data (buffered)
 *   sdcard::SDFile<LogFile>::Write("Boot time: %u ms\n", to_ms_since_boot(get_absolute_time()));
 *   datafile.write("Sensor: %f\n", sensor_value);
 * 
 *   // Sync when needed
 *   sdcard::SDFile<LogFile>::Sync();          // Force flush to disk
 *   datafile.sync();                          // Manual sync control
 * 
 *   // Cleanup
 *   sdcard::SDFile<LogFile>::Close();
 *   datafile.close();
 *   sdcard::SDCard::unmount();
 */

#include "sdcard/sd_config.h"
#include "sdcard/sd_driver.h"
#include "sdcard/sd_filesystem.h"
#include "sdcard/sd_file.h"

namespace sdcard {

// ============================================
// SD CARD MAIN API
// ============================================
class SDCard {
public:
    // Prevent instantiation
    SDCard() = delete;
    
    // ========================================
    // Core Operations
    // ========================================
    static bool mount() {
        return SDFilesystem::Mount();
    }

    static bool mounted() {
        return SDFilesystem::IsReady();
    }
    
    static bool unmount() {
        return SDFilesystem::Unmount();
    }
    
    static bool sync() {
        return SDFilesystem::Sync();
    }
    
    // ========================================
    // Directory Operations
    // ========================================
    static bool exists(const char* path) {
        return SDFilesystem::Exists(path);
    }
    
    static bool is_file(const char* path) {
        return SDFilesystem::IsFile(path);
    }
    
    static bool is_directory(const char* path) {
        return SDFilesystem::IsDirectory(path);
    }
    
    static bool create_directory(const char* path) {
        return SDFilesystem::CreateDirectory(path);
    }
    
    static bool remove(const char* path) {
        return SDFilesystem::Remove(path);
    }
    
    static int find_highest_numbered_folder(const char* prefix = "") {
        return SDFilesystem::FindHighestNumberedFolder(prefix);
    }
};

} // namespace sdcard