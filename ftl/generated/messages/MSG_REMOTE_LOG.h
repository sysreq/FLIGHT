#pragma once

#include "messages_detail.h"

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
    const uint8_t* data_;
    uint8_t length_;
    
    friend MessageResult<MSG_REMOTE_LOG_View> parse_MSG_REMOTE_LOG(const ftl::MessageHandle&);
    
    MSG_REMOTE_LOG_View(const uint8_t* data, uint8_t length)
        : data_(data), length_(length) {}

public:
    static constexpr MessageType TYPE = MessageType::MSG_REMOTE_LOG;
    
    // Field accessors
    uint32_t timestamp() const {
        size_t offset = 1;  // Skip message type byte
        return detail::read_primitive<uint32_t>(data_, offset);
    }
    std::string_view remote_printf() const {
        size_t offset = 1;  // Skip message type byte
        offset += 4;
        return detail::read_string(data_, offset, length_);
    }
    
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
    size_t offset_;
    bool valid_;
    
    ftl::MessagePoolType& get_pool() {
        return get_message_pool();
    }

public:
    MSG_REMOTE_LOG_Builder() 
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
                data_[2] = static_cast<uint8_t>(MessageType::MSG_REMOTE_LOG);
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
        , offset_(other.offset_)
        , valid_(other.valid_)
    {
        other.handle_ = ftl::MessagePoolType::INVALID;
        other.valid_ = false;
    }
    
    MSG_REMOTE_LOG_Builder& timestamp(uint32_t value) {
        if (valid_ && data_) {
            if (offset_ + sizeof(uint32_t) <= ftl_config::MAX_PAYLOAD_SIZE) {
                detail::write_primitive(data_, offset_, value);
            } else {
                valid_ = false;
            }
        }
        return *this;
    }
    MSG_REMOTE_LOG_Builder& remote_printf(std::string_view value) {
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