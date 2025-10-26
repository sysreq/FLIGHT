#pragma once

#include "messages_detail.h"
#include "serialization.h"

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
    serialization::Parser parser_;
    
    friend MessageResult<MSG_SYSTEM_STATE_View> parse_MSG_SYSTEM_STATE(const ftl::MessageHandle&);
    
    MSG_SYSTEM_STATE_View(const uint8_t* data, uint8_t length)
        : parser_(data, length)
        {}

public:
    static constexpr MessageType TYPE = MessageType::MSG_SYSTEM_STATE;
    
    // Field accessors
    uint8_t state_id() const {return parser_.read<uint8_t>(1);}
    bool is_active() const {return parser_.read<bool>(2);}
    uint32_t uptime_ms() const {return parser_.read<uint32_t>(3);}
    
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
    bool valid_;
    serialization::Serializer serializer_;
    
    ftl::MessagePoolType& get_pool() {
        return get_message_pool();
    }

public:
    MSG_SYSTEM_STATE_Builder() 
        : handle_(get_pool().acquire())
        , data_(nullptr)
        , valid_(handle_ != ftl::MessagePoolType::INVALID)
        , serializer_(nullptr, 0)
    {
        if (valid_) {
            data_ = get_pool().get_ptr<uint8_t>(handle_);
            if (data_) {
                serializer_ = serialization::Serializer(data_ + 3, ftl_config::MAX_PAYLOAD_SIZE - 3);
                serializer_.write(0, static_cast<uint8_t>(MessageType::MSG_SYSTEM_STATE));
                serializer_.set_dynamic_offset(7);
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
        , valid_(other.valid_)
        , serializer_(other.serializer_)
    {
        other.handle_ = ftl::MessagePoolType::INVALID;
        other.valid_ = false;
    }
    
    MSG_SYSTEM_STATE_Builder& state_id(uint8_t value) {
        if (valid_) {if (!serializer_.write<uint8_t>(1, value)) {
                valid_ = false;
            }}
        return *this;
    }
    MSG_SYSTEM_STATE_Builder& is_active(bool value) {
        if (valid_) {if (!serializer_.write<bool>(2, value)) {
                valid_ = false;
            }}
        return *this;
    }
    MSG_SYSTEM_STATE_Builder& uptime_ms(uint32_t value) {
        if (valid_) {if (!serializer_.write<uint32_t>(3, value)) {
                valid_ = false;
            }}
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
        
        uint8_t payload_length = static_cast<uint8_t>(serializer_.dynamic_offset());
        
        data_[0] = payload_length;
        data_[1] = ftl::get_my_source_id();
        
        auto msg_handle = ftl::MessageHandle(
            MsgHandle<ftl::MessagePoolType>(get_pool(), handle_)
        );
        
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