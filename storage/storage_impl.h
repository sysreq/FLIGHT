#pragma once

#include "storage_config.h"

namespace storage {

class Storage {
private:   
    alignas(4) uint8_t buffer_a[config::BUFFER_SIZE];
    alignas(4) uint8_t buffer_b[config::BUFFER_SIZE];
    
    uint8_t* active_buffer;
    uint8_t* pending_buffer;
    uint32_t active_pos;
    
    std::atomic<bool> write_pending;
    std::atomic<uint32_t> pending_size;
    
    mutex_t buffer_mutex;
    
    FATFS fs;
    FIL file;
    bool mounted = false;
    
    Storage() : active_buffer(buffer_a), 
                pending_buffer(buffer_b),
                active_pos(0), 
                write_pending(false),
                pending_size(0) {
        memset(buffer_a, 0, config::BUFFER_SIZE);
        memset(buffer_b, 0, config::BUFFER_SIZE);
        mutex_init(&buffer_mutex);
    }
    
    void swap_buffers() {
        uint8_t* temp = active_buffer;
        active_buffer = pending_buffer;
        pending_buffer = temp;
        
        pending_size.store(active_pos, std::memory_order_relaxed);
        active_pos = 0;
        write_pending.store(true, std::memory_order_release);
    }
    
    // Helper function to wait for pending write to complete
    // MUST be called with buffer_mutex held!
    void wait_for_pending_write() {
        while (write_pending.load(std::memory_order_acquire)) {
            mutex_exit(&buffer_mutex);
            
            if (get_core_num() == 0) {
                process_pending_write();
            } else {
                tight_loop_contents();
            }
            
            mutex_enter_blocking(&buffer_mutex);
        }
    }
    
public:
    static Storage& instance() {
        static Storage inst;
        return inst;
    }
    
    Storage(const Storage&) = delete;
    Storage& operator=(const Storage&) = delete;
    
    bool mount();
    void unmount();
    
    bool write(DataType type, const void* data, uint8_t length) {
        if (!mounted || !data || length == 0) return false;
        
        uint32_t total_size = sizeof(Header) + ((length + 3) & ~3);
        
        if (total_size > config::BUFFER_SIZE) return false;
        
        mutex_enter_blocking(&buffer_mutex);
        
        if (active_pos + total_size > config::BUFFER_SIZE) {
            wait_for_pending_write();  // Clean!
            swap_buffers();
        }
        
        Header* hdr = reinterpret_cast<Header*>(active_buffer + active_pos);
        hdr->magic = config::MAGIC;
        hdr->length = length;
        hdr->type = static_cast<uint8_t>(type);
        hdr->reserved = 0;
        
        memcpy(active_buffer + active_pos + sizeof(Header), data, length);
        
        active_pos += total_size;
        
        mutex_exit(&buffer_mutex);
        
        return true;
    }
    
    void flush() {
        mutex_enter_blocking(&buffer_mutex);
        
        if (active_pos > 0) {
            wait_for_pending_write();  // Same clean call
            swap_buffers();
        }
        
        mutex_exit(&buffer_mutex);
        
        // Process any remaining pending write
        if (write_pending.load(std::memory_order_acquire)) {
            process_pending_write();
        }
        
        f_sync(&file);
    }
    
    void process_pending_write() {
        if (!write_pending.load(std::memory_order_acquire) || !mounted) return;
        
        uint32_t size_to_write = pending_size.load(std::memory_order_relaxed);
        if (size_to_write == 0) return;
        
        UINT written;
        FRESULT res = f_write(&file, pending_buffer, size_to_write, &written);
                
        pending_size.store(0, std::memory_order_relaxed);
        write_pending.store(false, std::memory_order_release);
    }
    
    bool has_pending_write() const {
        return write_pending.load(std::memory_order_acquire);
    }
};

} // namespace storage