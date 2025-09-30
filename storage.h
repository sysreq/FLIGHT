#pragma once

#include "storage/storage_config.h"
#include "storage/storage_impl.h"

#include <cstdarg>
#include <cstdio>

class StorageGuard {
public:
    StorageGuard() {
        storage::Storage::instance().mount();
    }
    
    ~StorageGuard() {
        storage::Storage::instance().unmount();
    }
    
    // No copy/move
    StorageGuard(const StorageGuard&) = delete;
    StorageGuard& operator=(const StorageGuard&) = delete;
    StorageGuard(StorageGuard&&) = delete;
    StorageGuard& operator=(StorageGuard&&) = delete;
};

namespace storage {

template<typename T>
inline bool write_sensor_data(DataType type, const T& data) {
    static_assert(std::is_trivially_copyable_v<T>, 
                  "Sensor data must be trivially copyable");
    static_assert(sizeof(T) <= 252, 
                  "Sensor data too large (max 252 bytes)");
    
    return Storage::instance().write(type, &data, sizeof(T));
}

inline bool log(const char* format, ...) {
    char buffer[256];
    
    va_list args;
    va_start(args, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    if (len <= 0 || len >= static_cast<int>(sizeof(buffer))) {
        return false;
    }
    
    printf("%s", buffer);

    return Storage::instance().write(DataType::LOG_MSG, buffer, len + 1);
}

inline bool log_error(const char* format, ...) {
    char buffer[256];
    
    va_list args;
    va_start(args, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    if (len <= 0 || len >= static_cast<int>(sizeof(buffer))) {
        return false;
    }
    
    return Storage::instance().write(DataType::ERROR_MSG, buffer, len + 1);
}

} // namespace storage

#define LOG(...) storage::log(__VA_ARGS__)
#define ERROR(...) storage::log_error(__VA_ARGS__)