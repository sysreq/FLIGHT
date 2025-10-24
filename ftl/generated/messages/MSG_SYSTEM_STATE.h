#pragma once

#include "messages_detail.h"

/**
 * @file MSG_SYSTEM_STATE.h
 * @brief MSG_SYSTEM_STATE message type
 *
 * Auto-generated from messages.yaml - Do not edit manually!
 */

namespace ftl {
namespace messages {

// =============================================================================
// MSG_SYSTEM_STATE
// =============================================================================

class MSG_SYSTEM_STATE_View;
class MSG_SYSTEM_STATE_Builder;

/**
 * @brief MSG_SYSTEM_STATE message container
 * 
 * Provides nested type access: MSG_SYSTEM_STATE::View and MSG_SYSTEM_STATE::Builder
 */
class MSG_SYSTEM_STATE {
public:
    using View = MSG_SYSTEM_STATE_View;
    using Builder = MSG_SYSTEM_STATE_Builder;
    static constexpr MessageType TYPE = MessageType::MSG_SYSTEM_STATE;
};

/**
 * @brief Read-only view of MSG_SYSTEM_STATE message
 * 
 * Zero-copy accessor for received messages.
 * Lifetime is tied to underlying MessageHandle.
 */
class MSG_SYSTEM_STATE_View {
private:
    const uint8_t* data_;
    uint8_t length_;
    
    friend MessageResult<MSG_SYSTEM_STATE_View> parse_MSG_SYSTEM_STATE(const ftl::MessageHandle&);
    
    MSG_SYSTEM_STATE_View(const uint8_t* data, uint8_t length)
        : data_(data), length_(length) {}

public:
    static constexpr MessageType TYPE = MessageType::MSG_SYSTEM_STATE;
    
    // Field accessors
    uint8_t state_id() const {
        size_t offset = 1;  // Skip message type byte
        return detail::read_primitive<uint8_t>(data_, offset);
    }
    bool is_active() const {
        size_t offset = 1;  // Skip message type byte
        offset += 1;
        return detail::read_primitive<bool>(data_, offset);
    }
    uint32_t uptime_ms() const {
        size_t offset = 1;  // Skip message type byte
        offset += 1;
        offset += 1;
        return detail::read_primitive<uint32_t>(data_, offset);
    }
    
    // Get message type
    MessageType type() const { return TYPE; }
};

/**
 * @brief Builder for MSG_SYSTEM_STATE message
 * 
 * Fluent API for constructing messages.
 * Automatically manages MessageHandle lifecycle.
 */
class MSG_SYSTEM_STATE_Builder {
private:
    ftl::MessagePoolType::Handle handle_;
    uint8_t* data_;
    size_t offset_;
    bool valid_;
    
    ftl::MessagePoolType& get_pool() {
        return get_message_pool();
    }

public:
    MSG_SYSTEM_STATE_Builder() 
        : handle_(get_pool().acquire())
        , data_(nullptr)
        , offset_(3)  // Start after [LENGTH][SOURCE][TYPE]
        , valid_(handle_ != ftl::MessagePoolType::INVALID)
    {
        if (valid_) {
            data_ = get_pool().get_ptr<uint8_t>(handle_);
            if (data_) {
                // Buffer layout: [LENGTH][SOURCE][TYPE][FIELDS...]
                // Length and Source will be filled in build()
                data_[2] = static_cast<uint8_t>(MessageType::MSG_SYSTEM_STATE);
            } else {
                valid_ = false;
            }
        }
    }
    
    ~MSG_SYSTEM_STATE_Builder() {
        if (handle_ != ftl::MessagePoolType::INVALID && valid_) {
            get_pool().release(handle_);
        }
    }
    
    // Disable copy, enable move
    MSG_SYSTEM_STATE_Builder(const MSG_SYSTEM_STATE_Builder&) = delete;
    MSG_SYSTEM_STATE_Builder& operator=(const MSG_SYSTEM_STATE_Builder&) = delete;
    
    MSG_SYSTEM_STATE_Builder(MSG_SYSTEM_STATE_Builder&& other) noexcept
        : handle_(other.handle_)
        , data_(other.data_)
        , offset_(other.offset_)
        , valid_(other.valid_)
    {
        other.handle_ = ftl::MessagePoolType::INVALID;
        other.valid_ = false;
    }
    
    MSG_SYSTEM_STATE_Builder& state_id(uint8_t value) {
        if (valid_ && data_) {
            if (offset_ + sizeof(uint8_t) <= ftl_config::MAX_PAYLOAD_SIZE) {
                detail::write_primitive(data_, offset_, value);
            } else {
                valid_ = false;
            }
        }
        return *this;
    }
    MSG_SYSTEM_STATE_Builder& is_active(bool value) {
        if (valid_ && data_) {
            if (offset_ + sizeof(bool) <= ftl_config::MAX_PAYLOAD_SIZE) {
                detail::write_primitive(data_, offset_, value);
            } else {
                valid_ = false;
            }
        }
        return *this;
    }
    MSG_SYSTEM_STATE_Builder& uptime_ms(uint32_t value) {
        if (valid_ && data_) {
            if (offset_ + sizeof(uint32_t) <= ftl_config::MAX_PAYLOAD_SIZE) {
                detail::write_primitive(data_, offset_, value);
            } else {
                valid_ = false;
            }
        }
        return *this;
    }
    
    /**
     * @brief Build and return MessageHandle
     * 
     * Finalizes message construction and transfers ownership.
     * Builder becomes invalid after this call.
     * 
     * @return MessageHandle with constructed message, or invalid handle if build failed
     */
    ftl::MessageHandle build() {
        if (!valid_ || handle_ == ftl::MessagePoolType::INVALID) {
            return ftl::MessageHandle{};
        }
        
        // Calculate payload length: offset - 2 (skip LENGTH and SOURCE bytes)
        uint8_t payload_length = static_cast<uint8_t>(offset_ - 2);
        
        // Set buffer header: [LENGTH][SOURCE][TYPE][FIELDS...]
        data_[0] = payload_length;                      // Payload length
        data_[1] = ftl::get_my_source_id();        // Source ID
        // data_[2] already set to message type in constructor
        
        // Create MessageHandle with the raw handle
        auto msg_handle = ftl::MessageHandle(
            MsgHandle<ftl::MessagePoolType>(get_pool(), handle_)
        );
        
        // Transfer ownership
        handle_ = ftl::MessagePoolType::INVALID;
        valid_ = false;
        
        return msg_handle;
    }
    
    bool is_valid() const { return valid_; }
};

/**
 * @brief Parse MSG_SYSTEM_STATE from MessageHandle
 */
inline MessageResult<MSG_SYSTEM_STATE_View> parse_MSG_SYSTEM_STATE(const ftl::MessageHandle& handle) {
    if (!handle.is_valid()) {
        return std::unexpected(MessageError::INVALID_HANDLE);
    }
    
    const uint8_t* data = handle.data();
    uint8_t length = handle.length();
    
    if (length < 1) {
        return std::unexpected(MessageError::BUFFER_TOO_SMALL);
    }
    
    MessageType type = static_cast<MessageType>(data[0]);
    if (type != MessageType::MSG_SYSTEM_STATE) {
        return std::unexpected(MessageError::WRONG_MESSAGE_TYPE);
    }
    
    return MSG_SYSTEM_STATE_View(data, length);
}

} // namespace messages
} // namespace ftl