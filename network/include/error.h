#pragma once
#include <cstdint>

namespace network {
    enum class ErrorCode : uint8_t {
        None = 0,
        InvalidParameter,
        OutOfMemory,
        ConnectionFailed,
        SendFailed,
        ReceiveFailed,
        Timeout,
        NotInitialized,
        AlreadyInitialized,
        NoConnectionAvailable,
        ParseError
    };

    const char* ErrorCodeToString(ErrorCode code);
}
