#pragma once

#include "messages_detail.h"

/**
 * @file MSG_LOG_STRING.h
 * @brief MSG_LOG_STRING message type
 *
 * Auto-generated from messages.yaml - Do not edit manually!
 */

namespace ftl {
namespace messages {

// =============================================================================
// MSG_LOG_STRING
// =============================================================================

class MSG_LOG_STRING_View;
class MSG_LOG_STRING_Builder;

/**
 * @brief MSG_LOG_STRING message container
 * 
 * Provides nested type access: MSG_LOG_STRING::View and MSG_LOG_STRING::Builder
 */
class MSG_LOG_STRING {
public:
    using View = MSG_LOG_STRING_View;
    using Builder = MSG_LOG_STRING_Builder;
    static constexpr MessageType TYPE = MessageType::MSG_LOG_STRING;
};

/**
 * @brief Read-only view of MSG_LOG_STRING message
 * 
 * Zero-copy accessor for received messages.
 * Lifetime is tied to underlying MessageHandle.
 */
class MSG_LOG_STRING_View {
private:
    const uint8_t* data_;
    uint8_t length_;
    
    friend MessageResult<MSG_LOG_STRING_View> parse_MSG_LOG_STRING(const ftl::MessageHandle&);
    
    MSG_LOG_STRING_View(const uint8_t* data, uint8_t length)
        : data_(data), length_(length) {}

public:
    static constexpr MessageType TYPE = MessageType::MSG_LOG_STRING;
    
    // Field accessors
    std::string_view category() const {
        size_t offset = 1;  // Skip message type byte
        return detail::read_string(data_, offset, length_);
    }
    std::string_view message() const {
        size_t offset = 1;  // Skip message type byte
        detail::read_string(data_, offset, length_);  // Skip previous string
        return detail::read_string(data_, offset, length_);
    }
    
    // Get message type
    MessageType type() const { return TYPE; }
};

/**
 * @brief Builder for MSG_LOG_STRING message
 * 
 * Fluent API for constructing messages.
 * Automatically manages MessageHandle lifecycle.
 */
class MSG_LOG_STRING_Builder {
private:
    ftl::MessagePoolType::Handle handle_;
    uint8_t* data_;
    size_t offset_;
    bool valid_;
    
    ftl::MessagePoolType& get_pool() {
        return get_message_pool();
    }

public:
    MSG_LOG_STRING_Builder() 
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
                data_[2] = static_cast<uint8_t>(MessageType::MSG_LOG_STRING);
            } else {
                valid_ = false;
            }
        }
    }
    
    ~MSG_LOG_STRING_Builder() {
        if (handle_ != ftl::MessagePoolType::INVALID && valid_) {
            get_pool().release(handle_);
        }
    }
    
    // Disable copy, enable move
    MSG_LOG_STRING_Builder(const MSG_LOG_STRING_Builder&) = delete;
    MSG_LOG_STRING_Builder& operator=(const MSG_LOG_STRING_Builder&) = delete;
    
    MSG_LOG_STRING_Builder(MSG_LOG_STRING_Builder&& other) noexcept
        : handle_(other.handle_)
        , data_(other.data_)
        , offset_(other.offset_)
        , valid_(other.valid_)
    {
        other.handle_ = ftl::MessagePoolType::INVALID;
        other.valid_ = false;
    }
    
    MSG_LOG_STRING_Builder& category(std::string_view value) {
        if (valid_ && data_) {
            if (!detail::write_string(data_, offset_, ftl_config::MAX_PAYLOAD_SIZE, value)) {
                valid_ = false;
            }
        }
        return *this;
    }
    MSG_LOG_STRING_Builder& message(std::string_view value) {
        if (valid_ && data_) {
            if (!detail::write_string(data_, offset_, ftl_config::MAX_PAYLOAD_SIZE, value)) {
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
 * @brief Parse MSG_LOG_STRING from MessageHandle
 */
inline MessageResult<MSG_LOG_STRING_View> parse_MSG_LOG_STRING(const ftl::MessageHandle& handle) {
    if (!handle.is_valid()) {
        return std::unexpected(MessageError::INVALID_HANDLE);
    }
    
    const uint8_t* data = handle.data();
    uint8_t length = handle.length();
    
    if (length < 1) {
        return std::unexpected(MessageError::BUFFER_TOO_SMALL);
    }
    
    MessageType type = static_cast<MessageType>(data[0]);
    if (type != MessageType::MSG_LOG_STRING) {
        return std::unexpected(MessageError::WRONG_MESSAGE_TYPE);
    }
    
    return MSG_LOG_STRING_View(data, length);
}

} // namespace messages
} // namespace ftl