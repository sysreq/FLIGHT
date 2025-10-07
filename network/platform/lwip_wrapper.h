#pragma once
#include "connection.h"
#include "error.h"
#include "../types.h"
#include <expected>
#include <span>

namespace network::platform::lwip {
    // TCP operations
    std::expected<TcpListenerHandle, ErrorCode> CreateListener(uint16_t port);
    void DestroyListener(TcpListenerHandle listener);
    void SetupHttpListener(TcpListenerHandle listener);
    void CloseConnection(TcpHandle handle);
    std::expected<size_t, ErrorCode> Send(TcpHandle handle, std::span<const char> data);

    void Poll();  // Call cyw43_arch_poll()

    // Called when HTTP request is complete
    void ProcessHttpRequest(Connection& conn);
}
