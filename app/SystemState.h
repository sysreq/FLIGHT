#pragma once

#include <cstdint>
#include <cstddef>
#include <atomic>
#include <array>

enum GLOBAL_VAR : uint8_t {
    ACTIVE = 0,
    HX711_POLLING = 1,
    _COUNT
};

// System state structure
namespace SystemState {
    static inline std::array<std::atomic<uint32_t>, GLOBAL_VAR::_COUNT> globals_{};
}
