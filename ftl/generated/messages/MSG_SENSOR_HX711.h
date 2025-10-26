#pragma once

#include "messages_detail.h"
#include "serialization.h"

/**
 * @file MSG_SENSOR_HX711.h
 * @brief MSG_SENSOR_HX711 message type
 *
 * Auto-generated from messages.yaml - Do not edit manually!
 */

namespace ftl {
namespace messages {

// =============================================================================
// MSG_SENSOR_HX711
// =============================================================================

class MSG_SENSOR_HX711_View;
class MSG_SENSOR_HX711_Builder;

/**
 * @brief MSG_SENSOR_HX711 message container
 * 
 * Provides nested type access: MSG_SENSOR_HX711::View and MSG_SENSOR_HX711::Builder
 */
class MSG_SENSOR_HX711 {
public:
    using View = MSG_SENSOR_HX711_View;
    using Builder = MSG_SENSOR_HX711_Builder;
    static constexpr MessageType TYPE = MessageType::MSG_SENSOR_HX711;
};

/**
 * @brief Read-only view of MSG_SENSOR_HX711 message
 * 
 * Zero-copy accessor for received messages.
 * Lifetime is tied to underlying MessageHandle.
 */
class MSG_SENSOR_HX711_View {
private:
    serialization::Parser parser_;
    
    friend MessageResult<MSG_SENSOR_HX711_View> parse_MSG_SENSOR_HX711(const ftl::MessageHandle&);
    
    MSG_SENSOR_HX711_View(const uint8_t* data, uint8_t length)
        : parser_(data, length)
        {}

public:
    static constexpr MessageType TYPE = MessageType::MSG_SENSOR_HX711;
    
    // Field accessors
    uint32_t timestamp() const {return parser_.read<uint32_t>(1);}
    std::span<const uint32_t> samples() const {return parser_.read_array<uint32_t, 10>(5);}
    
    // Get message type
    MessageType type() const { return TYPE; }
};

/**
 * @brief Builder for MSG_SENSOR_HX711 message
 * 
 * Fluent API for constructing messages.
 * Automatically manages MessageHandle lifecycle.
 */
class MSG_SENSOR_HX711_Builder {
private:
    ftl::MessagePoolType::Handle handle_;
    uint8_t* data_;
    bool valid_;
    serialization::Serializer serializer_;
    
    ftl::MessagePoolType& get_pool() {
        return get_message_pool();
    }

public:
    MSG_SENSOR_HX711_Builder() 
        : handle_(get_pool().acquire())
        , data_(nullptr)
        , valid_(handle_ != ftl::MessagePoolType::INVALID)
        , serializer_(nullptr, 0)
    {
        if (valid_) {
            data_ = get_pool().get_ptr<uint8_t>(handle_);
            if (data_) {
                serializer_ = serialization::Serializer(data_ + 3, ftl_config::MAX_PAYLOAD_SIZE - 3);
                serializer_.write(0, static_cast<uint8_t>(MessageType::MSG_SENSOR_HX711));
                serializer_.set_dynamic_offset(45);
            } else {
                valid_ = false;
            }
        }
    }
    
    ~MSG_SENSOR_HX711_Builder() {
        if (handle_ != ftl::MessagePoolType::INVALID && valid_) {
            get_pool().release(handle_);
        }
    }
    
    // Disable copy, enable move
    MSG_SENSOR_HX711_Builder(const MSG_SENSOR_HX711_Builder&) = delete;
    MSG_SENSOR_HX711_Builder& operator=(const MSG_SENSOR_HX711_Builder&) = delete;
    
    MSG_SENSOR_HX711_Builder(MSG_SENSOR_HX711_Builder&& other) noexcept
        : handle_(other.handle_)
        , data_(other.data_)
        , valid_(other.valid_)
        , serializer_(other.serializer_)
    {
        other.handle_ = ftl::MessagePoolType::INVALID;
        other.valid_ = false;
    }
    
    MSG_SENSOR_HX711_Builder& timestamp(uint32_t value) {
        if (valid_) {if (!serializer_.write<uint32_t>(1, value)) {
                valid_ = false;
            }}
        return *this;
    }
    MSG_SENSOR_HX711_Builder& samples(std::span<const uint32_t, 10> value) {
        if (valid_) {if (!serializer_.write_array<uint32_t, 10>(5, value)) {
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
 * @brief Parse MSG_SENSOR_HX711 from MessageHandle
 */
inline MessageResult<MSG_SENSOR_HX711_View> parse_MSG_SENSOR_HX711(const ftl::MessageHandle& handle) {
    if (!handle.is_valid()) {
        return std::unexpected(MessageError::INVALID_HANDLE);
    }
    
    const uint8_t* data = handle.data();
    uint8_t length = handle.length();
    
    if (length < 1) {
        return std::unexpected(MessageError::BUFFER_TOO_SMALL);
    }
    
    MessageType type = static_cast<MessageType>(data[0]);
    if (type != MessageType::MSG_SENSOR_HX711) {
        return std::unexpected(MessageError::WRONG_MESSAGE_TYPE);
    }
    
    return MSG_SENSOR_HX711_View(data, length);
}

} // namespace messages
} // namespace ftl