#include "transport/uart/uart_tx.h"
#include "transport/uart/dma_control.h"
#include "util/misc.h"
#include <cstring>

namespace ftl {
namespace tx {

namespace {

uint8_t g_source_id = 0;

} // anonymous namespace

void initialize(uint8_t source_id) {
    g_source_id = source_id;
}

bool send_message(ftl_internal::DmaController& dma_controller, std::span<const uint8_t> payload) {
    if (payload.size() > ftl_config::MAX_PAYLOAD_SIZE) {
        return false;
    }
    
    uint8_t frame[ftl_config::MAX_MESSAGE_SIZE];
    size_t idx = 0;
    
    frame[idx++] = (ftl_config::START_DELIMITER >> 8) & 0xFF;
    frame[idx++] = ftl_config::START_DELIMITER & 0xFF;
    
    frame[idx++] = static_cast<uint8_t>(payload.size());
    
    frame[idx++] = g_source_id;
    
    memcpy(&frame[idx], payload.data(), payload.size());
    idx += payload.size();
    
    uint16_t crc = crc16::calculate(payload.data(), payload.size());
    frame[idx++] = (crc >> 8) & 0xFF;
    frame[idx++] = crc & 0xFF;
    
    frame[idx++] = (ftl_config::END_DELIMITER >> 8) & 0xFF;
    frame[idx++] = ftl_config::END_DELIMITER & 0xFF;
    
    return dma_controller.write_data(std::span<const uint8_t>{frame, idx});
}

bool send_message(ftl_internal::DmaController& dma_controller, std::string_view message) {
    return send_message(dma_controller, std::span<const uint8_t>{
        reinterpret_cast<const uint8_t*>(message.data()), 
        message.size()
    });
}

bool is_ready(ftl_internal::DmaController& dma_controller) {
    return !dma_controller.is_write_busy();
}

uint8_t get_source_id() {
    return g_source_id;
}

}} // namespace ftl::tx