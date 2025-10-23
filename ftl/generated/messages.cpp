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
    case MessageType::MSG_LOG_STRING: return "MSG_LOG_STRING";
    case MessageType::MSG_SENSOR_DATA: return "MSG_SENSOR_DATA";
    case MessageType::MSG_SYSTEM_STATE: return "MSG_SYSTEM_STATE";
    case MessageType::MSG_CONFIG: return "MSG_CONFIG";
    case MessageType::MSG_HEARTBEAT: return "MSG_HEARTBEAT";
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

void default_MSG_LOG_STRING_handler(const MSG_LOG_STRING_View& msg) {
    printf("MSG_LOG_STRING: ");
    printf("category='%.*s'", 
           static_cast<int>(msg.category().size()), 
           msg.category().data());
    printf(", ");
    printf("message='%.*s'", 
           static_cast<int>(msg.message().size()), 
           msg.message().data());
    printf("\n");
}

void default_MSG_SENSOR_DATA_handler(const MSG_SENSOR_DATA_View& msg) {
    printf("MSG_SENSOR_DATA: ");
    printf("sensor_id=%u", msg.sensor_id());
    printf(", ");
    printf("temperature=%.2f", msg.temperature());
    printf(", ");
    printf("pressure=%.2f", msg.pressure());
    printf(", ");
    printf("humidity=%.2f", msg.humidity());
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

void default_MSG_CONFIG_handler(const MSG_CONFIG_View& msg) {
    printf("MSG_CONFIG: ");
    printf("param_name='%.*s'", 
           static_cast<int>(msg.param_name().size()), 
           msg.param_name().data());
    printf(", ");
    printf("value=%u", msg.value());
    printf("\n");
}

void default_MSG_HEARTBEAT_handler(const MSG_HEARTBEAT_View& msg) {
    printf("MSG_HEARTBEAT: ");
    printf("sequence=%u", msg.sequence());
    printf(", ");
    printf("health_status=%u", msg.health_status());
    printf(", ");
    printf("device_name='%.*s'", 
           static_cast<int>(msg.device_name().size()), 
           msg.device_name().data());
    printf("\n");
}


// =============================================================================
// Dispatcher Implementation
// =============================================================================

Dispatcher::Dispatcher() {
    // Initialize all handlers with defaults
    msg_log_string_handler_ = default_MSG_LOG_STRING_handler;
    msg_sensor_data_handler_ = default_MSG_SENSOR_DATA_handler;
    msg_system_state_handler_ = default_MSG_SYSTEM_STATE_handler;
    msg_config_handler_ = default_MSG_CONFIG_handler;
    msg_heartbeat_handler_ = default_MSG_HEARTBEAT_handler;
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
    case MessageType::MSG_LOG_STRING: {
        auto result = parse_MSG_LOG_STRING(handle);
        if (result) {
            if (msg_log_string_handler_) {
                msg_log_string_handler_(*result);
            }
        } else {
            printf("Failed to parse MSG_LOG_STRING: %s\n", error_name(result.error()));
        }
        break;
    }
    case MessageType::MSG_SENSOR_DATA: {
        auto result = parse_MSG_SENSOR_DATA(handle);
        if (result) {
            if (msg_sensor_data_handler_) {
                msg_sensor_data_handler_(*result);
            }
        } else {
            printf("Failed to parse MSG_SENSOR_DATA: %s\n", error_name(result.error()));
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
    case MessageType::MSG_CONFIG: {
        auto result = parse_MSG_CONFIG(handle);
        if (result) {
            if (msg_config_handler_) {
                msg_config_handler_(*result);
            }
        } else {
            printf("Failed to parse MSG_CONFIG: %s\n", error_name(result.error()));
        }
        break;
    }
    case MessageType::MSG_HEARTBEAT: {
        auto result = parse_MSG_HEARTBEAT(handle);
        if (result) {
            if (msg_heartbeat_handler_) {
                msg_heartbeat_handler_(*result);
            }
        } else {
            printf("Failed to parse MSG_HEARTBEAT: %s\n", error_name(result.error()));
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

bool Dispatcher::send_msg_log_string(std::string_view category, std::string_view message) {
    auto msg = MSG_LOG_STRING::Builder()
        .category(category)
        .message(message)
        .build();
    
    if (msg.is_valid()) {
        return ftl::send_msg(msg.span());
    }
    return false;
}

bool Dispatcher::send_msg_sensor_data(uint16_t sensor_id, float temperature, float pressure, float humidity) {
    auto msg = MSG_SENSOR_DATA::Builder()
        .sensor_id(sensor_id)
        .temperature(temperature)
        .pressure(pressure)
        .humidity(humidity)
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

bool Dispatcher::send_msg_config(std::string_view param_name, uint32_t value) {
    auto msg = MSG_CONFIG::Builder()
        .param_name(param_name)
        .value(value)
        .build();
    
    if (msg.is_valid()) {
        return ftl::send_msg(msg.span());
    }
    return false;
}

bool Dispatcher::send_msg_heartbeat(uint32_t sequence, uint8_t health_status, std::string_view device_name) {
    auto msg = MSG_HEARTBEAT::Builder()
        .sequence(sequence)
        .health_status(health_status)
        .device_name(device_name)
        .build();
    
    if (msg.is_valid()) {
        return ftl::send_msg(msg.span());
    }
    return false;
}


} // namespace messages
} // namespace ftl