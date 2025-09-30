#pragma once

#include "sd_config.h"
#include "sd_driver.h"
#include "sd_filesystem.h"

namespace sdcard {

template<typename FileType>
class SDFile {
private:
    FIL fil_;
    bool is_open_ = false;
    
    static constexpr size_t BUFFER_SIZE = FileTraits<FileType>::buffer_size;
    uint8_t buffer_[BUFFER_SIZE];
    size_t buffer_pos_ = 0;
    
    SDFile() {
        memset(&fil_, 0, sizeof(fil_));
        memset(buffer_, 0, BUFFER_SIZE);
    }
    
    bool flush_buffer() {
        if (buffer_pos_ == 0) return true;
        
        UINT written;
        FRESULT res = f_write(&fil_, buffer_, buffer_pos_, &written);
        
        if (res != FR_OK || written != buffer_pos_) {
            return false;
        }
        
        buffer_pos_ = 0;
        return true;
    }
    
public:
    static SDFile& instance() {
        static SDFile instance;
        return instance;
    }
    
    SDFile(const SDFile&) = delete;
    SDFile& operator=(const SDFile&) = delete;
    SDFile(SDFile&&) = delete;
    SDFile& operator=(SDFile&&) = delete;
    
    bool open() {
        if (is_open_) return true;
        
        auto& fs = SDFilesystem::instance();
        if (!SDFilesystem::IsReady() || !SDFilesystem::AddFile()) {
            return false;
        }
        
        uint8_t mode = FA_WRITE | FA_READ;
        if (FileTraits<FileType>::append_mode) {
            mode |= FA_OPEN_APPEND;
            if (!SDFilesystem::Exists(FileTraits<FileType>::name)) {
                mode |= FA_CREATE_NEW;
            }
        } else {
            mode |= FA_CREATE_ALWAYS;
        }
        
        FRESULT res = f_open(&fil_, FileTraits<FileType>::name, mode);
        if (res != FR_OK) {
            fs.unregister_file();
            return false;
        }
        
        is_open_ = true;
        buffer_pos_ = 0;
        return true;
    }
    
    bool close() {
        if (!is_open_) return true;
        
        flush_buffer();
        
        FRESULT res = f_close(&fil_);
        is_open_ = false;
        
        SDFilesystem::instance().unregister_file();
        return (res == FR_OK);
    }
    
    bool write(const char* format, ...) {
        if (!is_open_) return false;
        
        char temp[256];
        va_list args;
        va_start(args, format);
        int len = vsnprintf(temp, sizeof(temp), format, args);
        va_end(args);
        
        if (len < 0 || len >= sizeof(temp)) {
            len = sizeof(temp) - 1;
        }
        
        return write_raw(reinterpret_cast<uint8_t*>(temp), len);
    }
    
    bool write_raw(const uint8_t* data, size_t length) {
        if (!is_open_) return false;
        
        size_t bytes_written = 0;
        
        while (bytes_written < length) {
            size_t space = BUFFER_SIZE - buffer_pos_;
            size_t to_copy = std::min(length - bytes_written, space);
            
            memcpy(buffer_ + buffer_pos_, data + bytes_written, to_copy);
            buffer_pos_ += to_copy;
            bytes_written += to_copy;
            
            if (buffer_pos_ >= BUFFER_SIZE) {
                if (!flush_buffer()) return false;
            }
        }
        
        if constexpr (FileTraits<FileType>::sync_time_ms == 0) {
            return sync();
        }
        
        return true;
    }
    
    bool sync() {
        if (!is_open_) return true;
        
        if (!flush_buffer()) return false;
        
        return (f_sync(&fil_) == FR_OK);
    }
        
    static bool Open() { return instance().open(); }
    static bool Close() { return instance().close(); }
    static bool Write(const char* format, ...) {
        char temp[256];
        va_list args;
        va_start(args, format);
        int len = vsnprintf(temp, sizeof(temp), format, args);
        va_end(args);
        
        if (len < 0 || len >= sizeof(temp)) {
            len = sizeof(temp) - 1;
        }
        
        return instance().write_raw(reinterpret_cast<uint8_t*>(temp), len);
    }
    static bool WriteRaw(const uint8_t* data, size_t length) { 
        return instance().write_raw(data, length); 
    }
    static bool Sync() { return instance().sync(); }
    static bool IsOpen() { return instance().is_open_; }
};

} // namespace sdcard