#pragma once
#include "handles.h"
#include <array>
#include <cstdint>
#include <cstddef>

namespace network::platform {
    // Connection pool
    inline constexpr size_t MAX_CONNECTIONS = 4;
    inline constexpr size_t REQUEST_BUFFER_SIZE = 2048;
    inline constexpr size_t RESPONSE_BUFFER_SIZE = 8192;

    struct Connection {
        TcpHandle tcp_handle;
        std::array<char, REQUEST_BUFFER_SIZE> request_buffer;
        std::array<char, RESPONSE_BUFFER_SIZE> response_buffer;
        size_t request_length;
        size_t response_length;
        size_t response_sent;  // Tracks how much of response has been sent
        bool in_use;

        void Reset() {
            tcp_handle = nullptr;
            request_length = 0;
            response_length = 0;
            response_sent = 0;
            in_use = false;
        }
    };

    inline std::array<Connection, MAX_CONNECTIONS> g_connection_pool{};

    // Find available connection
    Connection* AcquireConnection();
    void ReleaseConnection(Connection* conn);
}
