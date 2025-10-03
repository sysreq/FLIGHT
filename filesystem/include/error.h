#pragma once

#include <array>
#include <cstdint>
#include <cstddef>

namespace FileSystem {
    #define ERROR_CODE_LIST(X)    \
        X(NONE = 0)               \
        X(IO_ERROR)               \
        X(NOT_FOUND)              \
        X(INVALID_PATH)           \
        X(NO_SPACE)               \
        X(NOT_MOUNTED)            \
        X(ALREADY_EXISTS)         \
        X(INVALID_NAME)           \
        X(READ_ONLY)              \
        X(NOT_EMPTY)              \
        X(EOF_REACHED)            \
        X(CORRUPT)                \
        X(NOT_INITIALIZED)        \
        X(TIMEOUT)                \
        X(INVALID_PARAMETER)

    enum class ErrorCode : uint8_t {
        #define X(name, ...) name __VA_OPT__(= __VA_ARGS__),
        ERROR_CODE_LIST(X)
        #undef X
        _Count
    };

    static constexpr std::array<const char*, static_cast<size_t>(ErrorCode::_Count)> errorMessages = {
        #define X(name, ...) #name,
        ERROR_CODE_LIST(X)
        #undef X
    };

    static_assert(static_cast<uint8_t>(ErrorCode::NONE) == 0);
    static_assert(static_cast<uint8_t>(ErrorCode::INVALID_PARAMETER) == static_cast<uint8_t>(ErrorCode::_Count) - 1);

    inline constexpr const char* ErrorCodeToString(ErrorCode code) {
        const auto index = static_cast<size_t>(code);
        if (index < errorMessages.size()) {
            return errorMessages[index];
        }
        return "Unknown";
    }
}
