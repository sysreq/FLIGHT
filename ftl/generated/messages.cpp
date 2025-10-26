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
// Generic Print Helper
// =============================================================================

namespace detail {
template<typename T>
void print_field(const char* name, T value) {
    printf("%s=", name);
    if constexpr (std::is_same_v<T, bool>) {
        printf("%s", value ? "true" : "false");
    } else if constexpr (std::is_floating_point_v<T>) {
        printf("%.2f", value);
    } else if constexpr (std::is_integral_v<T>) {
        if constexpr (std::is_signed_v<T>) {
            printf("%lld", static_cast<long long>(value));
        } else {
            printf("%llu", static_cast<unsigned long long>(value));
        }
    } else {
        printf("?");
    }
}

template<typename T>
void print_field(const char* name, std::span<const T> value) {
    printf("%s=[", name);
    for (size_t i = 0; i < value.size(); ++i) {
        print_field("", value[i]);
        if (i < value.size() - 1) printf(", ");
    }
    printf("]");
}

void print_field(const char* name, std::string_view value) {
    printf("%s='%.*s'", name, static_cast<int>(value.size()), value.data());
}
} // namespace detail

void print_message(const ftl::MessageHandle& handle) {
    if (!handle.is_valid()) {
        printf("Invalid message\n");
        return;
    }

    MessageType type = static_cast<MessageType>(handle.data()[0]);
    printf("%s: ", message_type_name(type));

    switch (type) {
    case MessageType::MSG_REMOTE_LOG: {
        auto view_res = parse_MSG_REMOTE_LOG(handle);
        if (view_res) {
            auto& view = *view_res;
            detail::print_field("timestamp", view.timestamp());
printf(", ");            detail::print_field("remote_printf", view.remote_printf());
        }
        break;
    }
    case MessageType::MSG_SYSTEM_STATE: {
        auto view_res = parse_MSG_SYSTEM_STATE(handle);
        if (view_res) {
            auto& view = *view_res;
            detail::print_field("state_id", view.state_id());
printf(", ");            detail::print_field("is_active", view.is_active());
printf(", ");            detail::print_field("uptime_ms", view.uptime_ms());
        }
        break;
    }
    case MessageType::MSG_SENSOR_HX711: {
        auto view_res = parse_MSG_SENSOR_HX711(handle);
        if (view_res) {
            auto& view = *view_res;
            detail::print_field("timestamp", view.timestamp());
printf(", ");            detail::print_field("samples", view.samples());
        }
        break;
    }
    case MessageType::MSG_SENSOR_ADS1115: {
        auto view_res = parse_MSG_SENSOR_ADS1115(handle);
        if (view_res) {
            auto& view = *view_res;
            detail::print_field("timestamp", view.timestamp());
printf(", ");            detail::print_field("raw_1", view.raw_1());
printf(", ");            detail::print_field("raw_2", view.raw_2());
printf(", ");            detail::print_field("raw_3", view.raw_3());
printf(", ");            detail::print_field("raw_4", view.raw_4());
printf(", ");            detail::print_field("raw_5", view.raw_5());
        }
        break;
    }
    default:
        printf("Unknown message");
    }
    printf("\n");
}


// =============================================================================
// Dispatcher Implementation
// =============================================================================

Dispatcher::Dispatcher() {
    // Initialize all handlers with a generic print handler
    msg_remote_log_handler_ = [](const MSG_REMOTE_LOG_View& msg) {
        // This is a bit of a hack to get the handle back
        // A better solution would be to pass the handle to the handler
        auto& pool = get_message_pool();
        auto* handle_ptr = reinterpret_cast<const ftl::MessageHandle*>(
            reinterpret_cast<const uint8_t*>(&msg) - offsetof(ftl::MessageHandle, data_));
        print_message(*handle_ptr);
    };
    msg_system_state_handler_ = [](const MSG_SYSTEM_STATE_View& msg) {
        // This is a bit of a hack to get the handle back
        // A better solution would be to pass the handle to the handler
        auto& pool = get_message_pool();
        auto* handle_ptr = reinterpret_cast<const ftl::MessageHandle*>(
            reinterpret_cast<const uint8_t*>(&msg) - offsetof(ftl::MessageHandle, data_));
        print_message(*handle_ptr);
    };
    msg_sensor_hx711_handler_ = [](const MSG_SENSOR_HX711_View& msg) {
        // This is a bit of a hack to get the handle back
        // A better solution would be to pass the handle to the handler
        auto& pool = get_message_pool();
        auto* handle_ptr = reinterpret_cast<const ftl::MessageHandle*>(
            reinterpret_cast<const uint8_t*>(&msg) - offsetof(ftl::MessageHandle, data_));
        print_message(*handle_ptr);
    };
    msg_sensor_ads1115_handler_ = [](const MSG_SENSOR_ADS1115_View& msg) {
        // This is a bit of a hack to get the handle back
        // A better solution would be to pass the handle to the handler
        auto& pool = get_message_pool();
        auto* handle_ptr = reinterpret_cast<const ftl::MessageHandle*>(
            reinterpret_cast<const uint8_t*>(&msg) - offsetof(ftl::MessageHandle, data_));
        print_message(*handle_ptr);
    };
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

bool Dispatcher::send_msg_sensor_hx711(uint32_t timestamp, std::span<const uint32_t, 10> samples) {
    auto msg = MSG_SENSOR_HX711::Builder()
        .timestamp(timestamp)
        .samples(samples)
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