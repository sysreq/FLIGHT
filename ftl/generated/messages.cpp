#include "messages.h"

/**
 * @file messages.cpp
 * @brief Implementation of message helper functions
 *
 * Auto-generated from messages.yaml - Do not edit manually!
 */

namespace ftl {
namespace messages {

// =============================================================================
// Helper Function Implementations
// =============================================================================

const char* message_type_name(MessageType type) {
    switch (type) {
    case MessageType::MSG_REMOTE_LOG: return "MSG_REMOTE_LOG";
    case MessageType::MSG_SYSTEM_STATE: return "MSG_SYSTEM_STATE";
    case MessageType::MSG_SENSOR_HX711: return "MSG_SENSOR_HX711";
    case MessageType::MSG_SENSOR_ADS1115: return "MSG_SENSOR_ADS1115";
    case MessageType::INVALID: return "INVALID";
    default: return "UNKNOWN";
    }
}

const char* error_name(MessageError error) {
    switch (error) {
    case MessageError::INVALID_HANDLE: return "INVALID_HANDLE";
    case MessageError::WRONG_MESSAGE_TYPE: return "WRONG_MESSAGE_TYPE";
    case MessageError::BUFFER_TOO_SMALL: return "BUFFER_TOO_SMALL";
    case MessageError::INVALID_STRING_LENGTH: return "INVALID_STRING_LENGTH";
    case MessageError::INVALID_ARRAY_SIZE: return "INVALID_ARRAY_SIZE";
    case MessageError::PARSE_ERROR: return "PARSE_ERROR";
    default: return "UNKNOWN_ERROR";
    }
}

// Global message pool accessor
// This must be defined in your main application or ftl core implementation
ftl::MessagePoolType& get_message_pool() {
    extern ftl::MessagePoolType g_message_pool;
    return g_message_pool;
}

// =============================================================================
// Default Print Handlers
// =============================================================================

void default_MSG_REMOTE_LOG_handler(const MSG_REMOTE_LOG_View& msg) {
    printf("MSG_REMOTE_LOG: ");
    printf("timestamp=%u", msg.timestamp());
    printf(", ");
    printf("remote_printf='%.*s'", 
           static_cast<int>(msg.remote_printf().size()), 
           msg.remote_printf().data());
    printf("\n");
}

void default_MSG_SYSTEM_STATE_handler(const MSG_SYSTEM_STATE_View& msg) {
    printf("MSG_SYSTEM_STATE: ");
    printf("state_id=%u", msg.state_id());
    printf(", ");
    printf("is_active=%s", msg.is_active() ? "true" : "false");
    printf(", ");
    printf("uptime_ms=%u", msg.uptime_ms());
    printf("\n");
}

void default_MSG_SENSOR_HX711_handler(const MSG_SENSOR_HX711_View& msg) {
    printf("MSG_SENSOR_HX711: ");
    printf("timestamp=%u", msg.timestamp());
    printf(", ");
    printf("raw_1=%u", msg.raw_1());
    printf(", ");
    printf("raw_2=%u", msg.raw_2());
    printf(", ");
    printf("raw_3=%u", msg.raw_3());
    printf(", ");
    printf("raw_4=%u", msg.raw_4());
    printf(", ");
    printf("raw_5=%u", msg.raw_5());
    printf("\n");
}

void default_MSG_SENSOR_ADS1115_handler(const MSG_SENSOR_ADS1115_View& msg) {
    printf("MSG_SENSOR_ADS1115: ");
    printf("timestamp=%u", msg.timestamp());
    printf(", ");
    printf("raw_1=%.2f", msg.raw_1());
    printf(", ");
    printf("raw_2=%.2f", msg.raw_2());
    printf(", ");
    printf("raw_3=%.2f", msg.raw_3());
    printf(", ");
    printf("raw_4=%.2f", msg.raw_4());
    printf(", ");
    printf("raw_5=%.2f", msg.raw_5());
    printf("\n");
}


// =============================================================================
// Dispatcher Implementation
// =============================================================================

Dispatcher::Dispatcher() {
    // Initialize all handlers with defaults
    msg_remote_log_handler_ = default_MSG_REMOTE_LOG_handler;
    msg_system_state_handler_ = default_MSG_SYSTEM_STATE_handler;
    msg_sensor_hx711_handler_ = default_MSG_SENSOR_HX711_handler;
    msg_sensor_ads1115_handler_ = default_MSG_SENSOR_ADS1115_handler;
}

void Dispatcher::dispatch(ftl::MessageHandle& handle) {
    // Check message validity
    const uint8_t* data = handle.data();
    if (!data || handle.length() < 1) {
        printf("Invalid message: empty payload\n");
        return;
    }
    
    // Extract message type
    MessageType type = static_cast<MessageType>(data[0]);
    
    // Dispatch to appropriate handler
    switch (type) {
    case MessageType::MSG_REMOTE_LOG: {
        auto result = parse_MSG_REMOTE_LOG(handle);
        if (result) {
            if (msg_remote_log_handler_) {
                msg_remote_log_handler_(*result);
            }
        } else {
            printf("Failed to parse MSG_REMOTE_LOG: %s\n", error_name(result.error()));
        }
        break;
    }
    case MessageType::MSG_SYSTEM_STATE: {
        auto result = parse_MSG_SYSTEM_STATE(handle);
        if (result) {
            if (msg_system_state_handler_) {
                msg_system_state_handler_(*result);
            }
        } else {
            printf("Failed to parse MSG_SYSTEM_STATE: %s\n", error_name(result.error()));
        }
        break;
    }
    case MessageType::MSG_SENSOR_HX711: {
        auto result = parse_MSG_SENSOR_HX711(handle);
        if (result) {
            if (msg_sensor_hx711_handler_) {
                msg_sensor_hx711_handler_(*result);
            }
        } else {
            printf("Failed to parse MSG_SENSOR_HX711: %s\n", error_name(result.error()));
        }
        break;
    }
    case MessageType::MSG_SENSOR_ADS1115: {
        auto result = parse_MSG_SENSOR_ADS1115(handle);
        if (result) {
            if (msg_sensor_ads1115_handler_) {
                msg_sensor_ads1115_handler_(*result);
            }
        } else {
            printf("Failed to parse MSG_SENSOR_ADS1115: %s\n", error_name(result.error()));
        }
        break;
    }
    
    default:
        printf("Unknown message type: %u\n", static_cast<uint8_t>(type));
        break;
    }
}

// =============================================================================
// Dispatcher send() Implementations
// =============================================================================

bool Dispatcher::send_msg_remote_log(uint32_t timestamp, std::string_view remote_printf) {
    auto msg = MSG_REMOTE_LOG::Builder()
        .timestamp(timestamp)
        .remote_printf(remote_printf)
        .build();
    
    if (msg.is_valid()) {
        return ftl::send_msg(msg.span());
    }
    return false;
}

bool Dispatcher::send_msg_system_state(uint8_t state_id, bool is_active, uint32_t uptime_ms) {
    auto msg = MSG_SYSTEM_STATE::Builder()
        .state_id(state_id)
        .is_active(is_active)
        .uptime_ms(uptime_ms)
        .build();
    
    if (msg.is_valid()) {
        return ftl::send_msg(msg.span());
    }
    return false;
}

bool Dispatcher::send_msg_sensor_hx711(uint32_t timestamp, uint32_t raw_1, uint32_t raw_2, uint32_t raw_3, uint32_t raw_4, uint32_t raw_5) {
    auto msg = MSG_SENSOR_HX711::Builder()
        .timestamp(timestamp)
        .raw_1(raw_1)
        .raw_2(raw_2)
        .raw_3(raw_3)
        .raw_4(raw_4)
        .raw_5(raw_5)
        .build();
    
    if (msg.is_valid()) {
        return ftl::send_msg(msg.span());
    }
    return false;
}

bool Dispatcher::send_msg_sensor_ads1115(uint32_t timestamp, float raw_1, float raw_2, float raw_3, float raw_4, float raw_5) {
    auto msg = MSG_SENSOR_ADS1115::Builder()
        .timestamp(timestamp)
        .raw_1(raw_1)
        .raw_2(raw_2)
        .raw_3(raw_3)
        .raw_4(raw_4)
        .raw_5(raw_5)
        .build();
    
    if (msg.is_valid()) {
        return ftl::send_msg(msg.span());
    }
    return false;
}


} // namespace messages
} // namespace ftl