#include "error.h"

namespace network {
    const char* ErrorCodeToString(ErrorCode code) {
        switch (code) {
            case ErrorCode::None: return "None";
            case ErrorCode::InvalidParameter: return "InvalidParameter";
            case ErrorCode::OutOfMemory: return "OutOfMemory";
            case ErrorCode::ConnectionFailed: return "ConnectionFailed";
            case ErrorCode::SendFailed: return "SendFailed";
            case ErrorCode::ReceiveFailed: return "ReceiveFailed";
            case ErrorCode::Timeout: return "Timeout";
            case ErrorCode::NotInitialized: return "NotInitialized";
            case ErrorCode::AlreadyInitialized: return "AlreadyInitialized";
            case ErrorCode::NoConnectionAvailable: return "NoConnectionAvailable";
            case ErrorCode::ParseError: return "ParseError";
            default: return "Unknown";
        }
    }
}
