#pragma once

#include <atomic>
#include <array>
#include <cstddef>
#include <cstdint>

template <bool UseAtomics>
class MaybeAtomic;

template<>
class MaybeAtomic<true> {
private:
    alignas(64) std::atomic<uint8_t> value_{0};
public:
    inline uint8_t get_relaxed() const { return value_.load(std::memory_order_relaxed); }
    inline uint8_t get_ordered() const { return value_.load(std::memory_order_acquire); }
    inline void set_relaxed(uint8_t new_value) { value_.store(new_value, std::memory_order_relaxed); }
    inline void set_ordered(uint8_t new_value) { value_.store(new_value, std::memory_order_release); }
};

template<>
class MaybeAtomic<false> {
private:
    uint8_t value_{0};
public:
    inline uint8_t get_relaxed() const { return value_; }
    inline uint8_t get_ordered() const { return value_; }
    inline void set_relaxed(uint8_t new_value) { value_ = new_value; }
    inline void set_ordered(uint8_t new_value) { value_ = new_value; }
};

template<typename T, size_t Capacity, bool UseAtomics>
class CircularQueue {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    
private:
    std::array<T, Capacity> buffer_;
    static constexpr size_t MASK = Capacity - 1;
    
    MaybeAtomic<UseAtomics> head_;
    MaybeAtomic<UseAtomics> tail_;
    
public:
    CircularQueue() = default;
    ~CircularQueue() = default;
    
    CircularQueue(const CircularQueue&) = delete;
    CircularQueue& operator=(const CircularQueue&) = delete;
    
    bool enqueue(const T& item) {
        uint8_t current_tail = tail_.get_relaxed();
        uint8_t next_tail = (current_tail + 1) & MASK;
        
        uint8_t current_head = head_.get_ordered();
        if (next_tail == current_head) {
            return false;
        }
        
        buffer_[current_tail] = item;
        
        tail_.set_ordered(next_tail);
        return true;
    }
    
    bool dequeue(T& item) {
        uint8_t current_head = head_.get_relaxed();
        
        uint8_t current_tail = tail_.get_ordered();
        if (current_head == current_tail) {
            return false;
        }
        
        item = buffer_[current_head]; 
        
        head_.set_ordered((current_head + 1) & MASK);
        return true;
    }
    
    bool peek(T& item) const {
        uint8_t current_head = head_.get_relaxed();
        uint8_t current_tail = tail_.get_ordered();
        
        if (current_head == current_tail) {
            return false;
        }
        
        item = buffer_[current_head];
        return true;
    }
    
    bool is_empty() const {
        uint8_t h = head_.get_relaxed();
        uint8_t t = tail_.get_ordered();
        return h == t;
    }
    
    bool is_full() const {
        uint8_t t = tail_.get_relaxed();
        uint8_t h = head_.get_ordered();
        return ((t + 1) & MASK) == h;
    }
    
    uint8_t count() const {
        uint8_t h = head_.get_ordered();
        uint8_t t = tail_.get_ordered();
        return (t >= h) ? (t - h) : (Capacity - h + t);
    }
    
    constexpr size_t capacity() const { return Capacity; }
    
    void clear() {
        head_.set_relaxed(0);
        tail_.set_relaxed(0);
    }
};