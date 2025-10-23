#include "transport/uart/uart_rx.h"
#include "core/ftl_api.h"
#include "transport/uart/dma_control.h"
#include "util/allocator.h"
#include "util/cqueue.h"
#include "util/crc16.h"
#include <cstring>
#include <algorithm>

#include "pico/printf.h"

namespace ftl {
namespace messages {
    extern MessagePoolType g_message_pool;
}

namespace rx {

namespace {

constexpr size_t READ_CHUNK_SIZE = 64;
constexpr size_t MSG_LENGTH_OFFSET = 0;
constexpr size_t MSG_SOURCE_OFFSET = 1;
constexpr size_t MSG_PAYLOAD_OFFSET = 2;

enum class State {
    WAIT_START_1,
    WAIT_START_2,
    READ_LENGTH,
    READ_SOURCE,
    READ_PAYLOAD,
    READ_CRC_HIGH,
    READ_CRC_LOW,
    WAIT_END_1,
    WAIT_END_2
};

CircularQueue<PoolHandle, ftl_config::MESSAGE_QUEUE_DEPTH, false> g_handle_queue;

State g_rx_state = State::WAIT_START_1;
PoolHandle g_current_rx_handle = MessagePoolType::INVALID;
uint8_t g_expected_length = 0;
uint8_t g_bytes_received = 0;
uint16_t g_received_crc = 0;

uint32_t g_total_bytes_received = 0;
uint32_t g_total_messages_received = 0;
uint32_t g_crc_errors = 0;
uint32_t g_framing_errors = 0;

uint8_t* get_rx_buffer_ptr() {
    if (g_current_rx_handle == MessagePoolType::INVALID) {
        return nullptr;
    }
    return messages::g_message_pool.get_ptr<uint8_t>(g_current_rx_handle);
}

void reset_state() {
    if (g_current_rx_handle != MessagePoolType::INVALID) {
        messages::g_message_pool.release(g_current_rx_handle);
        g_current_rx_handle = MessagePoolType::INVALID;
    }
    g_rx_state = State::WAIT_START_1;
    g_expected_length = 0;
    g_bytes_received = 0;
    g_received_crc = 0;
}

bool validate_and_enqueue_message() {
    uint8_t* ptr = get_rx_buffer_ptr();
    if (!ptr) {
        reset_state();
        return false;
    }
    
    uint8_t payload_length = ptr[MSG_LENGTH_OFFSET];
    uint16_t calculated_crc = crc16::calculate(&ptr[MSG_PAYLOAD_OFFSET], payload_length);
    
    if (calculated_crc != g_received_crc) {
        printf("CRC error: expected 0x%04X, got 0x%04X", calculated_crc, g_received_crc);
        g_crc_errors++;
        reset_state();
        return false;
    }
    
    ptr[MSG_PAYLOAD_OFFSET + payload_length] = (g_received_crc >> 8) & 0xFF;
    ptr[MSG_PAYLOAD_OFFSET + payload_length + 1] = g_received_crc & 0xFF;
    
    if (!g_handle_queue.enqueue(g_current_rx_handle)) {
        PoolHandle old_handle;
        if (g_handle_queue.dequeue(old_handle)) {
            messages::g_message_pool.release(old_handle);
        }
        
        if (!g_handle_queue.enqueue(g_current_rx_handle)) {
            printf("Failed to enqueue message");
            reset_state();
            return false;
        }
    }
    
    g_total_messages_received++;
    
    g_current_rx_handle = MessagePoolType::INVALID;
    g_rx_state = State::WAIT_START_1;
    g_expected_length = 0;
    g_bytes_received = 0;
    g_received_crc = 0;
    
    return true;
}

void process_byte(uint8_t byte) {
    switch (g_rx_state) {
        case State::WAIT_START_1:
            if (byte == ((ftl_config::START_DELIMITER >> 8) & 0xFF)) {
                g_rx_state = State::WAIT_START_2;
            }
            break;
            
        case State::WAIT_START_2:
            if (byte == (ftl_config::START_DELIMITER & 0xFF)) {
                g_current_rx_handle = messages::g_message_pool.acquire();
                if (g_current_rx_handle == MessagePoolType::INVALID) {
                    printf("Pool exhausted - cannot start message");
                    g_rx_state = State::WAIT_START_1;
                } else {
                    g_rx_state = State::READ_LENGTH;
                }
            } else if (byte == ((ftl_config::START_DELIMITER >> 8) & 0xFF)) {
                g_rx_state = State::WAIT_START_2;
            } else {
                g_rx_state = State::WAIT_START_1;
            }
            break;
            
        case State::READ_LENGTH:
            if (byte == 0 || byte > ftl_config::MAX_PAYLOAD_SIZE) {
                printf("Invalid length: %d", byte);
                g_framing_errors++;
                reset_state();
            } else {
                g_expected_length = byte;
                uint8_t* ptr = get_rx_buffer_ptr();
                if (ptr) {
                    ptr[MSG_LENGTH_OFFSET] = byte;
                    g_rx_state = State::READ_SOURCE;
                } else {
                    reset_state();
                }
            }
            break;
            
        case State::READ_SOURCE:
            {
                uint8_t* ptr = get_rx_buffer_ptr();
                if (ptr) {
                    ptr[MSG_SOURCE_OFFSET] = byte;
                    g_bytes_received = 0;
                    g_rx_state = State::READ_PAYLOAD;
                } else {
                    reset_state();
                }
            }
            break;
            
        case State::READ_PAYLOAD:
            {
                uint8_t* ptr = get_rx_buffer_ptr();
                if (ptr) {
                    ptr[MSG_PAYLOAD_OFFSET + g_bytes_received] = byte;
                    g_bytes_received++;
                    
                    if (g_bytes_received >= g_expected_length) {
                        g_rx_state = State::READ_CRC_HIGH;
                    }
                } else {
                    reset_state();
                }
            }
            break;
            
        case State::READ_CRC_HIGH:
            g_received_crc = static_cast<uint16_t>(byte) << 8;
            g_rx_state = State::READ_CRC_LOW;
            break;
            
        case State::READ_CRC_LOW:
            g_received_crc |= byte;
            g_rx_state = State::WAIT_END_1;
            break;
            
        case State::WAIT_END_1:
            if (byte == ((ftl_config::END_DELIMITER >> 8) & 0xFF)) {
                g_rx_state = State::WAIT_END_2;
            } else {
                printf("Expected 0xDE, got 0x%02X", byte);
                g_framing_errors++;
                reset_state();
            }
            break;
            
        case State::WAIT_END_2:
            if (byte == (ftl_config::END_DELIMITER & 0xFF)) {
                validate_and_enqueue_message();
            } else {
                printf("Expected 0xFA, got 0x%02X", byte);
                g_framing_errors++;
                reset_state();
            }
            break;
    }
}

void process_data(ftl_internal::DmaController& dma_controller) {
    uint8_t read_buffer[READ_CHUNK_SIZE];
    size_t bytes_read = dma_controller.read_from_circular_buffer(
        std::span<uint8_t>{read_buffer, READ_CHUNK_SIZE}
    );
    
    if (bytes_read == 0) {
        return;
    }
    
    g_total_bytes_received += bytes_read;
    
    for (size_t i = 0; i < bytes_read; i++) {
        process_byte(read_buffer[i]);
    }
}

} // anonymous namespace

void initialize() {
    reset_state();
    g_handle_queue.clear();
    g_total_bytes_received = 0;
    g_total_messages_received = 0;
    g_crc_errors = 0;
    g_framing_errors = 0;
}

void process(ftl_internal::DmaController& dma_controller) {
    process_data(dma_controller);
}

bool has_message() {
    return !g_handle_queue.is_empty();
}

MessageHandle get_message() {
    if (g_handle_queue.is_empty()) {
        return MessageHandle{};
    }
    
    PoolHandle handle;
    if (!g_handle_queue.dequeue(handle)) {
        return MessageHandle{};
    }
    
    MsgHandle<MessagePoolType> msg_handle(messages::g_message_pool, handle);
    return MessageHandle(std::move(msg_handle));
}

Statistics get_statistics() {
    return Statistics{
        g_total_bytes_received,
        g_total_messages_received,
        g_crc_errors,
        g_framing_errors
    };
}

uint32_t get_pool_allocated_count() {
    uint32_t count = 0;
    for (size_t i = 0; i < ftl_config::MESSAGE_POOL_SIZE; i++) {
        if (messages::g_message_pool.is_valid(static_cast<PoolHandle>(i))) {
            count++;
        }
    }
    return count;
}

uint32_t get_queue_count() {
    return g_handle_queue.count();
}

}} // namespace ftl::rx