#pragma once
#include <cstdint>
#include <array>

namespace network {
    #define ERROR_CODE_LIST(X)  \
        X(None, 0)              \
        X(InvalidParameter)     \
        X(OutOfMemory)          \
        X(ConnectionFailed)     \
        X(SendFailed)           \
        X(ReceiveFailed)        \
        X(Timeout)              \
        X(NotInitialized)       \
        X(AlreadyInitialized)   \
        X(NoConnectionAvailable)\
        X(ParseError)

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

    static_assert(static_cast<uint8_t>(ErrorCode::None) == 0);
    static_assert(static_cast<uint8_t>(ErrorCode::ParseError) == static_cast<uint8_t>(ErrorCode::_Count) - 1);

    inline constexpr const char* ErrorCodeToString(ErrorCode code) {
        const auto index = static_cast<size_t>(code);
        if (index < errorMessages.size()) {
            return errorMessages[index];
        }
        return "Unknown";
    }
}
