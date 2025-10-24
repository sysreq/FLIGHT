#pragma once

#include "core/ftl_api.h"
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <string_view>
#include <span>
#include <array>
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
    INVALID_ARRAY_SIZE,
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

// Write length-prefixed string (FIXED: corrected null terminator handling)
inline bool write_string(uint8_t* data, size_t& offset, size_t max_len, std::string_view str) {
    if (str.size() > 255) return false;
    if (offset + 1 + str.size() + 1 > max_len) return false;  // +1 for length, +1 for null
    
    data[offset++] = static_cast<uint8_t>(str.size());
    std::memcpy(data + offset, str.data(), str.size());
    offset += str.size();
    data[offset++] = '\0';  // null terminator
    return true;
}

// Read fixed-size array as span (zero-copy view)
template<typename T, size_t N>
inline std::span<const T> read_array(const uint8_t* data, size_t& offset, size_t max_len) {
    constexpr size_t array_bytes = sizeof(T) * N;
    
    if (offset + array_bytes > max_len) {
        return {};  // Return empty span on error
    }
    
    // Cast to properly aligned pointer and create span
    const T* array_ptr = reinterpret_cast<const T*>(data + offset);
    offset += array_bytes;
    
    return std::span<const T>(array_ptr, N);
}

// Write fixed-size array from span
template<typename T, size_t N>
inline bool write_array(uint8_t* data, size_t& offset, size_t max_len, std::span<const T> values) {
    constexpr size_t array_bytes = sizeof(T) * N;
    
    // Validate size matches
    if (values.size() != N) return false;
    if (offset + array_bytes > max_len) return false;
    
    std::memcpy(data + offset, values.data(), array_bytes);
    offset += array_bytes;
    return true;
}

// Write fixed-size array from std::array
template<typename T, size_t N>
inline bool write_array(uint8_t* data, size_t& offset, size_t max_len, const std::array<T, N>& values) {
    return write_array<T, N>(data, offset, max_len, std::span<const T>(values.data(), N));
}

} // namespace detail

} // namespace messages
} // namespace ftl