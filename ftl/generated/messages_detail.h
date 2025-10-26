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

} // namespace messages
} // namespace ftl