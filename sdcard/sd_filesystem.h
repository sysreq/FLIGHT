#pragma once

#include "sd_config.h"
#include "sd_driver.h"

namespace sdcard {

class SDFilesystem {
private:
    FATFS fs_;
    bool mounted_ = false;
    uint8_t open_files_ = 0;
    
    SDFilesystem() {
        memset(&fs_, 0, sizeof(fs_));
    }
    
public:
    static SDFilesystem& instance() {
        static SDFilesystem instance;
        return instance;
    }
    
    SDFilesystem(const SDFilesystem&) = delete;
    SDFilesystem& operator=(const SDFilesystem&) = delete;
    SDFilesystem(SDFilesystem&&) = delete;
    SDFilesystem& operator=(SDFilesystem&&) = delete;
    
    bool mount() {
        if (mounted_) return true;
        
        // Initialize driver first
        if (!SDDriver::Init()) return false;
        
        // Mount filesystem
        FRESULT res = f_mount(&fs_, "", 1);
        mounted_ = (res == FR_OK);
        return mounted_;
    }
    
    bool unmount() {
        if (!mounted_) return true;
        
        // Force close any open files
        if (open_files_ > 0) {
            open_files_ = 0;
        }
        
        f_unmount("");
        mounted_ = false;
        
        SDDriver::Shutdown();
        return true;
    }
    
    bool sync() {
        if (!mounted_) return false;
        return (f_sync(nullptr) == FR_OK);
    }
    
    bool exists(const char* path) {
        if (!mounted_) return false;
        FILINFO fno;
        return (f_stat(path, &fno) == FR_OK);
    }
    
    bool is_file(const char* path) {
        if (!mounted_) return false;
        FILINFO fno;
        if (f_stat(path, &fno) != FR_OK) return false;
        return !(fno.fattrib & AM_DIR);
    }
    
    bool is_directory(const char* path) {
        if (!mounted_) return false;
        FILINFO fno;
        if (f_stat(path, &fno) != FR_OK) return false;
        return (fno.fattrib & AM_DIR);
    }
    
    bool create_directory(const char* path) {
        if (!mounted_) return false;
        FRESULT res = f_mkdir(path);
        return (res == FR_OK || res == FR_EXIST);
    }
    
    bool remove(const char* path) {
        if (!mounted_) return false;
        return (f_unlink(path) == FR_OK);
    }
    
    int find_highest_numbered_folder(const char* prefix = "") {
        if (!mounted_) return -1;
        
        DIR dir;
        FILINFO fno;
        int highest = -1;
        
        if (f_opendir(&dir, prefix) != FR_OK) return -1;
        
        while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0] != 0) {
            if (fno.fattrib & AM_DIR) {
                char* endptr;
                int num = strtol(fno.fname, &endptr, 10);
                if (*endptr == '\0' && num > highest) {
                    highest = num;
                }
            }
        }
        
        f_closedir(&dir);
        return highest;
    }
    
    static bool AddFile() {
        if (instance().open_files_ >= sys::MAX_OPEN_FILES) return false;
        instance().open_files_++;
        return true;
    }
    
    void unregister_file() {
        if (open_files_ > 0) open_files_--;
    }
        
    static bool Mount() { return instance().mount(); }
    static bool Unmount() { return instance().unmount(); }
    static bool Sync() { return instance().sync(); }
    static bool Exists(const char* path) { return instance().exists(path); }
    static bool IsFile(const char* path) { return instance().is_file(path); }
    static bool IsDirectory(const char* path) { return instance().is_directory(path); }
    static bool CreateDirectory(const char* path) { return instance().create_directory(path); }
    static bool Remove(const char* path) { return instance().remove(path); }
    static int FindHighestNumberedFolder(const char* prefix = "") { 
        return instance().find_highest_numbered_folder(prefix); 
    }
    static bool IsReady() { return instance().mounted_; }
};

} // namespace sdcard