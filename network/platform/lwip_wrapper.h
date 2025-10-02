#pragma once
#include "handles.h"
#include "config.h"
#include "../include/types.h"
#include "../include/error.h"
#include <expected>
#include <span>

namespace network::platform::lwip {
    // TCP operations - only declarations here
    std::expected<TcpListenerHandle, ErrorCode> CreateListener(uint16_t port);
    void DestroyListener(TcpListenerHandle listener);

    // HTTP-specific helper - sets up listener with HTTP connection handling
    void SetupHttpListener(TcpListenerHandle listener);

    void CloseConnection(TcpHandle handle);

    std::expected<size_t, ErrorCode> Send(TcpHandle handle, std::span<const char> data);
    std::expected<size_t, ErrorCode> Receive(TcpHandle handle, std::span<char> buffer);

    void Poll();  // Call cyw43_arch_poll()

    // Called by lwip_wrapper.cpp when HTTP request is complete
    void ProcessHttpRequest(Connection& conn);
}
