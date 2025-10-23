#pragma once

#include "ftl.settings"
#include <span>
#include <string_view>
#include <cstdint>

namespace ftl_internal {
    class DmaController;
}

namespace ftl {
namespace tx {

void initialize(uint8_t source_id);
bool send_message(ftl_internal::DmaController& dma_controller, std::span<const uint8_t> payload);
bool send_message(ftl_internal::DmaController& dma_controller, std::string_view message);
bool is_ready(ftl_internal::DmaController& dma_controller);
uint8_t get_source_id();

}} // namespace ftl::tx