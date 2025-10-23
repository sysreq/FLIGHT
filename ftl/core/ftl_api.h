#pragma once

#include "util/allocator.h"
#include "config.settings"
#include <span>
#include <cstddef>
#include <cstdint>
#include <string_view>

/**
 * @file ftl_api.h
 * @brief FTL (Faster Than Light) - Message-oriented UART API with framed protocol
 * 
 * Protocol format:
 * [0xAACC] [LENGTH] [SOURCE_ID] [PAYLOAD] [CRC16] [0xDEFA]
 * 
 * Features:
 * - DMA-based background transfers
 * - Automatic message framing and validation
 * - CRC16 error detection
 * - Source ID tracking
 * - Zero-copy message access via reference-counted handles
 * - Fixed-size pool allocation (no heap fragmentation)
 * 
 * Usage pattern:
 * 1. Call ftl::initialize() once at startup
 * 2. Call ftl::poll() regularly in main loop
 * 3. Use ftl::has_msg() and ftl::get_msg() to retrieve messages
 * 4. Use ftl::send_msg() to transmit messages with your source ID
 */

namespace ftl {

// Type aliases for the message pool
using MessagePoolType = MessagePool<ftl_config::MAX_MESSAGE_SIZE, ftl_config::MESSAGE_POOL_SIZE>;
using PoolHandle = MessagePoolType::Handle;

/**
 * @brief Structure for parsed message data
 */
struct MessageData {
    uint8_t length;       // Total payload length
    uint8_t source_id;    // Sender's ID
    uint16_t crc16;       // CRC16 of payload
    const uint8_t* payload; // Pointer to payload data
};

/**
 * @brief Reference-counted message handle with convenience methods
 * 
 * Provides easy access to message data with automatic lifetime management.
 * Messages are automatically returned to the pool when the handle goes out of scope.
 */
class MessageHandle {
private:
    MsgHandle<MessagePoolType> handle_;
    
    const uint8_t* get_raw_ptr() const {
        if (!handle_) return nullptr;
        return handle_.template operator-><uint8_t>();
    }
    
public:
    MessageHandle() = default;
    
    explicit MessageHandle(MsgHandle<MessagePoolType>&& h) 
        : handle_(std::move(h)) {}
    
    // Move-only type
    MessageHandle(MessageHandle&&) = default;
    MessageHandle& operator=(MessageHandle&&) = default;
    MessageHandle(const MessageHandle&) = delete;
    MessageHandle& operator=(const MessageHandle&) = delete;
    
    /**
     * @brief Get pointer to message payload data
     * @return Pointer to payload content, or nullptr if invalid
     */
    const uint8_t* data() const {
        const uint8_t* ptr = get_raw_ptr();
        if (!ptr) return nullptr;
        return ptr + 2;
    }
    
    /**
     * @brief Get message payload as char pointer
     * @return Pointer to payload as char*, or nullptr if invalid
     */
    const char* c_str() const {
        return reinterpret_cast<const char*>(data());
    }
    
    /**
     * @brief Get payload length in bytes
     * @return Length of payload content
     */
    uint8_t length() const {
        const uint8_t* ptr = get_raw_ptr();
        if (!ptr) return 0;
        return ptr[0];  // First byte is length
    }
    
    /**
     * @brief Get source ID of message sender
     * @return Source ID (0-255)
     */
    uint8_t source_id() const {
        const uint8_t* ptr = get_raw_ptr();
        if (!ptr) return 0;
        return ptr[1];  // Second byte is source ID
    }
    
    /**
     * @brief Get CRC16 value stored with message
     * @return CRC16 value
     */
    uint16_t crc16() const {
        const uint8_t* ptr = get_raw_ptr();
        if (!ptr) return 0;
        uint8_t len = ptr[0];
        // CRC is stored after payload: ptr[2 + len] and ptr[2 + len + 1]
        return (static_cast<uint16_t>(ptr[2 + len]) << 8) | ptr[2 + len + 1];
    }
    
    /**
     * @brief Get message payload as string_view
     * @return String view of payload content
     */
    std::string_view view() const {
        return std::string_view(c_str(), length());
    }
    
    /**
     * @brief Get message payload as span
     * @return Span of payload bytes
     */
    std::span<const uint8_t> span() const {
        return std::span<const uint8_t>(data(), length());
    }
    
    /**
     * @brief Check if handle is valid
     * @return true if handle points to valid message
     */
    explicit operator bool() const {
        return static_cast<bool>(handle_);
    }
    
    /**
     * @brief Check if handle is valid
     * @return true if handle points to valid message
     */
    bool is_valid() const {
        return static_cast<bool>(handle_);
    }
    
    /**
     * @brief Get complete message data structure
     * @return MessageData structure with all fields
     */
    MessageData get_data() const {
        MessageData msg{};
        const uint8_t* ptr = get_raw_ptr();
        if (ptr) {
            msg.length = ptr[0];
            msg.source_id = ptr[1];
            msg.payload = ptr + 2;
            msg.crc16 = (static_cast<uint16_t>(ptr[2 + msg.length]) << 8) | 
                        ptr[2 + msg.length + 1];
        }
        return msg;
    }
};

/**
 * @brief Initialize UART hardware and DMA subsystem
 * 
 * @param my_source_id Source ID for this device (0-255)
 * 
 * Configures UART peripheral, GPIO pins, DMA channels, and message pool.
 * Must be called once before any other API calls.
 */
void initialize();

/**
 * @brief Process all UART background tasks (call regularly from main loop)
 * 
 * This function handles:
 * - Processing DMA buffers
 * - Assembling received bytes into complete messages
 * - Validating message format and CRC
 * - Managing message pool and queue
 * 
 * Call this frequently (ideally every main loop iteration) for low latency.
 */
void poll();

/**
 * @brief Check if a complete message is available
 * 
 * @return true if at least one complete, validated message is ready
 */
bool has_msg();

/**
 * @brief Retrieve the next complete message
 * 
 * @return MessageHandle with reference-counted access to message data
 * 
 * Returns a handle to the oldest unread message. The message remains valid
 * for the lifetime of the handle. Returns invalid handle if no messages
 * are available.
 */
MessageHandle get_msg();

/**
 * @brief Send a message with automatic framing and CRC
 * 
 * @param payload Payload data to transmit (max 248 bytes)
 * @return true if transmission started, false if busy or data too large
 * 
 * Automatically adds:
 * - Start delimiter (0xAACC)
 * - Length byte
 * - Source ID (set during initialization)
 * - CRC16 of payload
 * - End delimiter (0xDEFA)
 */
bool send_msg(std::span<const uint8_t> payload);

/**
 * @brief Send a string message
 * 
 * @param message String message to transmit
 * @return true if transmission started, false if busy or data too large
 */
bool send_msg(std::string_view message);

/**
 * @brief Check if transmitter is ready for new message
 * 
 * @return true if TX is idle and ready for new data
 */
bool is_tx_ready();

/**
 * @brief Get current device source ID
 * 
 * @return Source ID set during initialization
 */
uint8_t get_my_source_id();

/**
 * @brief Get statistics about message processing
 * 
 * @param total_bytes_rx Total bytes received
 * @param total_messages_rx Total valid messages received
 * @param messages_queued Number of messages currently in queue
 * @param pool_allocated Number of pool allocations currently in use
 * @param crc_errors Number of messages with CRC errors
 * @param framing_errors Number of framing errors detected
 */
void get_stats(uint32_t& total_bytes_rx, uint32_t& total_messages_rx, 
               uint32_t& messages_queued, uint32_t& pool_allocated,
               uint32_t& crc_errors, uint32_t& framing_errors);

} // namespace ftl
