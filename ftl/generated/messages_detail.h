#pragma once

#include "core/ftl_api.h"
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <string_view>
#include <functional>
#include <expected>

/**
 * @file messages_detail.h
 * @brief Common types and helpers for FTL messages
 *
 * Auto-generated from messages.yaml - Do not edit manually!
 */

namespace ftl {
namespace messages {

// =============================================================================
// Message Type Enumeration
// =============================================================================

enum class MessageType : uint8_t {
    MSG_REMOTE_LOG = 0,
    MSG_SYSTEM_STATE = 1,
    MSG_SENSOR_HX711 = 2,
    MSG_SENSOR_ADS1115 = 3,
    INVALID = 0xFF
};

const char* message_type_name(MessageType type);

// =============================================================================
// Error Types
// =============================================================================

enum class MessageError {
    INVALID_HANDLE,
    WRONG_MESSAGE_TYPE,
    BUFFER_TOO_SMALL,
    INVALID_STRING_LENGTH,
    PARSE_ERROR
};

const char* error_name(MessageError error);

template<typename T>
using MessageResult = std::expected<T, MessageError>;

// Forward declare message pool accessor
ftl::MessagePoolType& get_message_pool();

// =============================================================================
// Helper Functions
// =============================================================================

namespace detail {

// Read primitive types with memcpy (handles alignment)
template<typename T>
inline T read_primitive(const uint8_t* data, size_t& offset) {
    T value;
    std::memcpy(&value, data + offset, sizeof(T));
    offset += sizeof(T);
    return value;
}

// Write primitive types with memcpy
template<typename T>
inline void write_primitive(uint8_t* data, size_t& offset, T value) {
    std::memcpy(data + offset, &value, sizeof(T));
    offset += sizeof(T);
}

// Read length-prefixed string
inline std::string_view read_string(const uint8_t* data, size_t& offset, size_t max_len) {
    if (offset >= max_len) return {};
    
    uint8_t len = data[offset++];
    if (offset + len > max_len) return {};
    
    std::string_view result(reinterpret_cast<const char*>(data + offset), len);
    offset += len;
    return result;
}

// Write length-prefixed string
inline bool write_string(uint8_t* data, size_t& offset, size_t max_len, std::string_view str) {
    const size_t str_size = str.size() + 1; // include null terminator
    if (str_size > 255) return false;
    if (offset + 1 + str_size > max_len) return false;
    
    data[offset++] = static_cast<uint8_t>(str.size());
    std::memcpy(data + offset, str.data(), str_size);
    offset += str_size;
    data[offset - 1] = '\0'; // null terminator
    return true;
}

} // namespace detail

} // namespace messages
} // namespace ftl