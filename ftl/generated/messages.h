#pragma once

/**
 * @file messages.h
 * @brief Main header for FTL protocol messages
 *
 * Auto-generated from messages.yaml - Do not edit manually!
 *
 * This file includes all message type definitions.
 * Individual message headers are in the messages/ directory.
 */

// Include common types and helpers
#include "messages_detail.h"

// Include all message type definitions
#include "messages/MSG_LOG_STRING.h"
#include "messages/MSG_SENSOR_DATA.h"
#include "messages/MSG_SYSTEM_STATE.h"
#include "messages/MSG_CONFIG.h"
#include "messages/MSG_HEARTBEAT.h"

namespace ftl {
namespace messages {

// =============================================================================
// Message Dispatcher
// =============================================================================

/**
 * @brief Automatic message dispatcher with customizable handlers
 * 
 * Provides default print handlers for all message types.
 * Users can override with custom handlers using set_handler().
 * 
 * Example:
 *   Dispatcher dispatcher;
 *   
 *   // Override sensor data handler
 *   dispatcher.set_handler([](const MSG_SENSOR_DATA_View& msg) {
 *       // Custom handling
 *   });
 *   
 *   // Dispatch received messages
 *   while (ftl::has_msg()) {
 *       auto msg = ftl::get_msg();
 *       dispatcher.dispatch(msg);
 *   }
 *   
 *   // Send messages with type-safe interface
 *   dispatcher.send<MSG_LOG_STRING>("SYSTEM", "Hello World");
 *   dispatcher.send<MSG_SENSOR_DATA>(sensor_id, temp, pressure, humidity);
 */
class Dispatcher {
private:
    std::function<void(const MSG_LOG_STRING_View&)> msg_log_string_handler_;
    std::function<void(const MSG_SENSOR_DATA_View&)> msg_sensor_data_handler_;
    std::function<void(const MSG_SYSTEM_STATE_View&)> msg_system_state_handler_;
    std::function<void(const MSG_CONFIG_View&)> msg_config_handler_;
    std::function<void(const MSG_HEARTBEAT_View&)> msg_heartbeat_handler_;

public:
    /**
     * @brief Constructor - initializes with default print handlers
     */
    Dispatcher();
    
    /**
     * @brief Dispatch a received message to appropriate handler
     *
     * @param handle MessageHandle containing the received message
     */
    void dispatch(ftl::MessageHandle& handle);
    
    // Handler setters (one for each message type)
    void set_handler(std::function<void(const MSG_LOG_STRING_View&)> handler) {
        msg_log_string_handler_ = std::move(handler);
    }
    void set_handler(std::function<void(const MSG_SENSOR_DATA_View&)> handler) {
        msg_sensor_data_handler_ = std::move(handler);
    }
    void set_handler(std::function<void(const MSG_SYSTEM_STATE_View&)> handler) {
        msg_system_state_handler_ = std::move(handler);
    }
    void set_handler(std::function<void(const MSG_CONFIG_View&)> handler) {
        msg_config_handler_ = std::move(handler);
    }
    void set_handler(std::function<void(const MSG_HEARTBEAT_View&)> handler) {
        msg_heartbeat_handler_ = std::move(handler);
    }

    // =============================================================================
    // Type-safe Message Sending
    // =============================================================================
    
    /**
     * @brief Send a typed message with compile-time dispatch
     * 
     * Uses if constexpr for zero-overhead, compile-time message type selection.
     * Ensures correct argument types and count for each message.
     * 
     * Example:
     *   dispatcher.send<MSG_LOG_STRING>("SYSTEM", "Hello");
     *   dispatcher.send<MSG_SENSOR_DATA>(1, 25.5f, 101.3f, 60.0f);
     * 
     * @tparam MsgType Message type class (e.g., MSG_LOG_STRING)
     * @param args Message field values in declaration order
     * @return true if message was sent successfully
     */
    template<typename MsgType>
    bool send(auto&&... args) {
if constexpr (std::is_same_v<MsgType, MSG_LOG_STRING>) {
            return send_msg_log_string(std::forward<decltype(args)>(args)...);
        }
else if constexpr (std::is_same_v<MsgType, MSG_SENSOR_DATA>) {
            return send_msg_sensor_data(std::forward<decltype(args)>(args)...);
        }
else if constexpr (std::is_same_v<MsgType, MSG_SYSTEM_STATE>) {
            return send_msg_system_state(std::forward<decltype(args)>(args)...);
        }
else if constexpr (std::is_same_v<MsgType, MSG_CONFIG>) {
            return send_msg_config(std::forward<decltype(args)>(args)...);
        }
else if constexpr (std::is_same_v<MsgType, MSG_HEARTBEAT>) {
            return send_msg_heartbeat(std::forward<decltype(args)>(args)...);
        }
        else {
            static_assert(sizeof(MsgType) == 0, "Unknown message type");
            return false;
        }
    }

private:
    // Internal send implementations (one per message type)
    bool send_msg_log_string(std::string_view category, std::string_view message);
    bool send_msg_sensor_data(uint16_t sensor_id, float temperature, float pressure, float humidity);
    bool send_msg_system_state(uint8_t state_id, bool is_active, uint32_t uptime_ms);
    bool send_msg_config(std::string_view param_name, uint32_t value);
    bool send_msg_heartbeat(uint32_t sequence, uint8_t health_status, std::string_view device_name);
};

// =============================================================================
// Default Print Handlers
// =============================================================================

/**
 * @brief Default print handler for MSG_LOG_STRING
 */
void default_MSG_LOG_STRING_handler(const MSG_LOG_STRING_View& msg);

/**
 * @brief Default print handler for MSG_SENSOR_DATA
 */
void default_MSG_SENSOR_DATA_handler(const MSG_SENSOR_DATA_View& msg);

/**
 * @brief Default print handler for MSG_SYSTEM_STATE
 */
void default_MSG_SYSTEM_STATE_handler(const MSG_SYSTEM_STATE_View& msg);

/**
 * @brief Default print handler for MSG_CONFIG
 */
void default_MSG_CONFIG_handler(const MSG_CONFIG_View& msg);

/**
 * @brief Default print handler for MSG_HEARTBEAT
 */
void default_MSG_HEARTBEAT_handler(const MSG_HEARTBEAT_View& msg);


} // namespace messages
} // namespace ftl

namespace ftl {

/**
 * @brief Extended MessageHandle with typed message access
 */
class TypedMessageHandle : public MessageHandle {
public:
    using MessageHandle::MessageHandle;
    
    /**
     * @brief Cast message to specific type
     * 
     * @tparam T Message view type (e.g., MSG_SENSOR_DATA_View)
     * @return MessageResult<T> with typed view or error
     */
    template<typename T>
    messages::MessageResult<T> as() const {
        if (!is_valid()) {
            return std::unexpected(messages::MessageError::INVALID_HANDLE);
        }
        
        const uint8_t* data = this->data();
        uint8_t length = this->length();
        
        if (length < 1) {
            return std::unexpected(messages::MessageError::BUFFER_TOO_SMALL);
        }
        
        messages::MessageType type = static_cast<messages::MessageType>(data[0]);
        if (type != T::TYPE) {
            return std::unexpected(messages::MessageError::WRONG_MESSAGE_TYPE);
        }
        
        // Use the parse function
        return messages::MessageResult<T>(T(data, length));
    }
};

} // namespace ftl