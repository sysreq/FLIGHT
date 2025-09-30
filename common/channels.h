#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>

#include "pico/time.h"

static constexpr std::size_t MESSAGE_BUFFER_SIZE = 16;

struct Message {
    uint8_t                 data[122];
    uint8_t                 type;
    uint8_t                 buffer_offset;
    uint32_t                acquire_time;
    std::atomic<bool>       active;
    
    template<typename T>
    void Put(const T& obj) noexcept {
        static_assert(sizeof(T) <= sizeof(data), "Object too large");
        std::memcpy(data, &obj, sizeof(T));
    }
    
    template<typename T>
    T* As() noexcept {
        return reinterpret_cast<T*>(data);
    }
    
    template<typename T>
    const T* As() const noexcept {
        return reinterpret_cast<const T*>(data);
    }
};

template<typename ChannelTag>
class MessageChannel {
public:
    static Message* acquire() noexcept {
        auto& inst = Instance();
        uint8_t pos = inst.write_pos.load(std::memory_order_acquire);
        Message& msg = inst.buffer[pos];
        
        if (!msg.active.load(std::memory_order_acquire)) {
            msg.acquire_time = time_us_32();
            return &msg;
        }
        return nullptr;
    }
    
    static void commit(Message* msg) noexcept {
        auto& inst = Instance();
        msg->active.store(true, std::memory_order_release);
        uint8_t next_pos = (msg->buffer_offset + 1) % MESSAGE_BUFFER_SIZE;
        inst.write_pos.store(next_pos, std::memory_order_release);
    }
    
    static Message* pop() noexcept {
        auto& inst = Instance();
        uint8_t pos = inst.read_pos.load(std::memory_order_acquire);
        Message& msg = inst.buffer[pos];
        
        if (msg.active.load(std::memory_order_acquire)) {
            return &msg;
        }
        return nullptr;
    }
    
    static void release(Message* msg) noexcept {
        auto& inst = Instance();
        msg->acquire_time = 0;
        msg->active.store(false, std::memory_order_release);
        uint8_t next_pos = (msg->buffer_offset + 1) % MESSAGE_BUFFER_SIZE;
        inst.read_pos.store(next_pos, std::memory_order_release);
    }
    
    static bool empty() noexcept {
        auto& inst = Instance();
        uint8_t pos = inst.read_pos.load(std::memory_order_acquire);
        return !inst.buffer[pos].active.load(std::memory_order_acquire);
    }
    
private:
    static MessageChannel& Instance() {
        static MessageChannel instance;
        return instance;
    }
    
    MessageChannel() {
        for (std::size_t i = 0; i < MESSAGE_BUFFER_SIZE; ++i) {
            buffer[i].active.store(false, std::memory_order_relaxed);
            buffer[i].buffer_offset = static_cast<uint8_t>(i);
        }
    }
    
    MessageChannel(const MessageChannel&) = delete;
    MessageChannel& operator=(const MessageChannel&) = delete;
    
    alignas(64) std::array<Message, MESSAGE_BUFFER_SIZE> buffer;
    alignas(64) std::atomic<uint8_t> read_pos{0};
    alignas(64) std::atomic<uint8_t> write_pos{0};
};