#pragma once

#include <atomic>
#include <array>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <concepts>

template<size_t ObjectSize = 32, size_t MaxObjects = 255>
    requires (ObjectSize > 0) && (MaxObjects <= 255) && ((ObjectSize % 4) == 0)
class MessagePool {
public:
    using Handle = std::uint8_t;
    
    static constexpr size_t TOTAL_SIZE = ObjectSize * MaxObjects;
    static constexpr std::uint8_t STATE_FREE = 0x00;
    static constexpr std::uint8_t STATE_ALLOCATING = 0xFF;
    static constexpr Handle INVALID = 0xFF;
    static constexpr std::uint8_t MAX_REF_COUNT = 8;
    
private:
    alignas(4) std::array<uint8_t, TOTAL_SIZE> memory_pool{};
    std::array<std::atomic<uint8_t>, MaxObjects> ref_counts{};
    std::atomic<uint8_t> next_hint{0};

    void* get_raw_ptr(Handle h) {
        return &memory_pool[h * ObjectSize];
    }
    
    const void* get_raw_ptr(Handle h) const {
        return &memory_pool[h * ObjectSize];
    }

public:
    MessagePool() noexcept = default;
    
    MessagePool(const MessagePool&) = delete;
    MessagePool& operator=(const MessagePool&) = delete;
    MessagePool(MessagePool&&) = delete;
    MessagePool& operator=(MessagePool&&) = delete;
    
    Handle acquire() {
        std::uint8_t start = next_hint.load(std::memory_order_relaxed);
        
        for (std::uint8_t attempts = 0; attempts < MaxObjects; ++attempts) {
            std::uint8_t idx = (start + attempts) % MaxObjects;
            std::uint8_t expected = STATE_FREE;
            
            if (ref_counts[idx].compare_exchange_strong(expected, STATE_ALLOCATING,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                
                std::memset(get_raw_ptr(idx), 0, ObjectSize);
                        
                std::atomic_thread_fence(std::memory_order_release);
                ref_counts[idx].store(1, std::memory_order_release);
                next_hint.store((idx + 1) % MaxObjects, std::memory_order_relaxed);
                
                return idx;
            }
        }
        return INVALID;
    }
    
    bool add_ref(Handle h) {
        if (h >= MaxObjects) return false;
        
        std::uint8_t count = ref_counts[h].load(std::memory_order_acquire);
        
        while (count > 0 && count < MAX_REF_COUNT) {
            if (ref_counts[h].compare_exchange_weak(count, count + 1,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                return true;
            }
        }
        return false;
    }
    
    bool release(Handle h) {
        if (h >= MaxObjects) return false;
        
        std::uint8_t count = ref_counts[h].load(std::memory_order_acquire);
        
        while (count > 0 && count <= MAX_REF_COUNT) {
            std::uint8_t new_count = count - 1;
            if (ref_counts[h].compare_exchange_weak(count, new_count,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                return new_count == STATE_FREE;
            }
        }
        return false;
    }
    
    template<typename T = void>
    requires (sizeof(T) <= ObjectSize || std::same_as<T, void>)
    T* get_ptr(Handle h) {
        if (h >= MaxObjects) return nullptr;
        
        std::uint8_t count = ref_counts[h].load(std::memory_order_acquire);
        if (count > 0 && count <= MAX_REF_COUNT) {
            if constexpr (std::same_as<T, void>) {
                return get_raw_ptr(h);
            } else {
                return reinterpret_cast<T*>(get_raw_ptr(h));
            }
        }
        return nullptr;
    }
    
    template<typename T = void>
    requires (sizeof(T) <= ObjectSize || std::same_as<T, void>)
    const T* get_ptr(Handle h) const {
        if (h >= MaxObjects) return nullptr;
        
        std::uint8_t count = ref_counts[h].load(std::memory_order_acquire);
        if (count > 0 && count <= MAX_REF_COUNT) {
            if constexpr (std::same_as<T, void>) {
                return get_raw_ptr(h);
            } else {
                return reinterpret_cast<const T*>(get_raw_ptr(h));
            }
        }
        return nullptr;
    }
    
    bool is_valid(Handle h) const {
        if (h >= MaxObjects) return false;
        std::uint8_t count = ref_counts[h].load(std::memory_order_acquire);
        return count > 0 && count <= MAX_REF_COUNT;
    }
    
    uint8_t get_ref_count(Handle h) const {
        if (h >= MaxObjects) return 0;
        std::uint8_t count = ref_counts[h].load(std::memory_order_acquire);
        return (count > 0 && count <= MAX_REF_COUNT) ? count : 0;
    }
};

template<typename Pool>
class MsgHandle {
private:
    using Handle = typename Pool::Handle;

    Pool* alloc;
    Handle handle;
    
public:    
    MsgHandle() : alloc(nullptr), handle(Pool::INVALID) {}
    
    explicit MsgHandle(Pool& a, Handle h = Pool::INVALID)
        : alloc(&a), handle(h) {}
    
    ~MsgHandle() {
        reset();
    }
    
    MsgHandle(MsgHandle&& other)
        : alloc(other.alloc), handle(other.handle) {
        other.handle = Pool::INVALID;
    }
    
    MsgHandle& operator=(MsgHandle&& other) {
        if (this != &other) {
            reset();
            alloc = other.alloc;
            handle = other.handle;
            other.handle = Pool::INVALID;
        }
        return *this;
    }
    
    MsgHandle(const MsgHandle&) = delete;
    MsgHandle& operator=(const MsgHandle&) = delete;
    
    void reset(Handle h = Pool::INVALID) noexcept {
        if (handle != Pool::INVALID && alloc) {
            alloc->release(handle);
        }
        handle = h;
    }
    
    Handle get() const { return handle; }
    Handle detach() {
        Handle h = handle;
        handle = Pool::INVALID;
        return h;
    }
    
    explicit operator bool() const {
        return handle != Pool::INVALID && alloc && alloc->is_valid(handle);
    }
    
    template<typename T>
    T* operator->() const {
        return alloc ? alloc->template get_ptr<T>(handle) : nullptr;
    }
};