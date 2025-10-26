#pragma once

#include "messages_detail.h"
#include "serialization.h"

/**
 * @file MSG_REMOTE_LOG.h
 * @brief MSG_REMOTE_LOG message type
 *
 * Auto-generated from messages.yaml - Do not edit manually!
 */

namespace ftl {
namespace messages {

// =============================================================================
// MSG_REMOTE_LOG
// =============================================================================

class MSG_REMOTE_LOG_View;
class MSG_REMOTE_LOG_Builder;

/**
 * @brief MSG_REMOTE_LOG message container
 * 
 * Provides nested type access: MSG_REMOTE_LOG::View and MSG_REMOTE_LOG::Builder
 */
class MSG_REMOTE_LOG {
public:
    using View = MSG_REMOTE_LOG_View;
    using Builder = MSG_REMOTE_LOG_Builder;
    static constexpr MessageType TYPE = MessageType::MSG_REMOTE_LOG;
};

/**
 * @brief Read-only view of MSG_REMOTE_LOG message
 * 
 * Zero-copy accessor for received messages.
 * Lifetime is tied to underlying MessageHandle.
 */
class MSG_REMOTE_LOG_View {
private:
    serialization::Parser parser_;
    mutable size_t dynamic_offset_;
    
    friend MessageResult<MSG_REMOTE_LOG_View> parse_MSG_REMOTE_LOG(const ftl::MessageHandle&);
    
    MSG_REMOTE_LOG_View(const uint8_t* data, uint8_t length)
        : parser_(data, length)
        , dynamic_offset_(5)
        {}

public:
    static constexpr MessageType TYPE = MessageType::MSG_REMOTE_LOG;
    
    // Field accessors
    uint32_t timestamp() const {return parser_.read<uint32_t>(1);}
    std::string_view remote_printf() const {return parser_.read_string(dynamic_offset_);}
    
    // Get message type
    MessageType type() const { return TYPE; }
};

/**
 * @brief Builder for MSG_REMOTE_LOG message
 * 
 * Fluent API for constructing messages.
 * Automatically manages MessageHandle lifecycle.
 */
class MSG_REMOTE_LOG_Builder {
private:
    ftl::MessagePoolType::Handle handle_;
    uint8_t* data_;
    bool valid_;
    serialization::Serializer serializer_;
    
    ftl::MessagePoolType& get_pool() {
        return get_message_pool();
    }

public:
    MSG_REMOTE_LOG_Builder() 
        : handle_(get_pool().acquire())
        , data_(nullptr)
        , valid_(handle_ != ftl::MessagePoolType::INVALID)
        , serializer_(nullptr, 0)
    {
        if (valid_) {
            data_ = get_pool().get_ptr<uint8_t>(handle_);
            if (data_) {
                serializer_ = serialization::Serializer(data_ + 3, ftl_config::MAX_PAYLOAD_SIZE - 3);
                serializer_.write(0, static_cast<uint8_t>(MessageType::MSG_REMOTE_LOG));
                serializer_.set_dynamic_offset(5);
            } else {
                valid_ = false;
            }
        }
    }
    
    ~MSG_REMOTE_LOG_Builder() {
        if (handle_ != ftl::MessagePoolType::INVALID && valid_) {
            get_pool().release(handle_);
        }
    }
    
    // Disable copy, enable move
    MSG_REMOTE_LOG_Builder(const MSG_REMOTE_LOG_Builder&) = delete;
    MSG_REMOTE_LOG_Builder& operator=(const MSG_REMOTE_LOG_Builder&) = delete;
    
    MSG_REMOTE_LOG_Builder(MSG_REMOTE_LOG_Builder&& other) noexcept
        : handle_(other.handle_)
        , data_(other.data_)
        , valid_(other.valid_)
        , serializer_(other.serializer_)
    {
        other.handle_ = ftl::MessagePoolType::INVALID;
        other.valid_ = false;
    }
    
    MSG_REMOTE_LOG_Builder& timestamp(uint32_t value) {
        if (valid_) {if (!serializer_.write<uint32_t>(1, value)) {
                valid_ = false;
            }}
        return *this;
    }
    MSG_REMOTE_LOG_Builder& remote_printf(std::string_view value) {
        if (valid_) {size_t current_offset = serializer_.dynamic_offset();
            if (!serializer_.write_string(current_offset, value)) {
                valid_ = false;
            }
            serializer_.set_dynamic_offset(current_offset);}
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
 * @brief Parse MSG_REMOTE_LOG from MessageHandle
 */
inline MessageResult<MSG_REMOTE_LOG_View> parse_MSG_REMOTE_LOG(const ftl::MessageHandle& handle) {
    if (!handle.is_valid()) {
        return std::unexpected(MessageError::INVALID_HANDLE);
    }
    
    const uint8_t* data = handle.data();
    uint8_t length = handle.length();
    
    if (length < 1) {
        return std::unexpected(MessageError::BUFFER_TOO_SMALL);
    }
    
    MessageType type = static_cast<MessageType>(data[0]);
    if (type != MessageType::MSG_REMOTE_LOG) {
        return std::unexpected(MessageError::WRONG_MESSAGE_TYPE);
    }
    
    return MSG_REMOTE_LOG_View(data, length);
}

} // namespace messages
} // namespace ftl