#pragma once

#include <array>
#include <concepts>
#include <cstdint>

namespace network {
    enum class HttpMethod : uint8_t {
        GET, POST, PUT, DELETE, PATCH, HEAD, OPTIONS
    };

    enum class StatusCode : uint16_t {
        OK = 200,
        BadRequest = 400,
        NotFound = 404,
        InternalError = 500
    };

    struct IpAddress {
        uint32_t addr;  // Network byte order

        constexpr IpAddress() : addr(0) {}
        constexpr explicit IpAddress(uint32_t a) : addr(a) {}
        constexpr IpAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
            : addr((d << 24) | (c << 16) | (b << 8) | a) {}
    };

    template<typename T>
    concept Service = requires(T t) {
        { t.Start() } -> std::same_as<bool>;
        { t.Stop() } -> std::same_as<void>;
        { t.Process() } -> std::same_as<void>;
    };
}
