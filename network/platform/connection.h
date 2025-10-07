#pragma once
#include <array>
#include <cstdint>
#include <cstddef>
#include <optional>

namespace network::platform {
    // Opaque handle types - hide SDK implementation
    using TcpHandle = void*;
    using TcpListenerHandle = void*;
    using UdpHandle = void*;
    using ConnectionHandle = void*;

    // Future extension points
    using DnsHandle = void*;
    using DhcpHandle = void*;

    inline constexpr size_t MAX_CONNECTIONS = 4;
    inline constexpr size_t REQUEST_BUFFER_SIZE = 2048;
    inline constexpr size_t RESPONSE_BUFFER_SIZE = 8192;
    inline constexpr size_t MAX_HTTP_PATH_LENGTH = 256;
    inline constexpr uint32_t CONNECTION_TIMEOUT_MS = 30000;

    class Connection {
    public:
        Connection(const Connection&) = delete;
        Connection& operator=(const Connection&) = delete;
        Connection(Connection&&) = delete;
        Connection& operator=(Connection&&) = delete;

        Connection() = default;
        ~Connection() = default;

        bool IsInUse() const { return in_use_; }

        TcpHandle GetTcpHandle() const { return tcp_handle_; }
        void SetTcpHandle(TcpHandle handle) { tcp_handle_ = handle; }

        char* GetRequestBuffer() { return request_buffer_.data(); }
        const char* GetRequestBuffer() const { return request_buffer_.data(); }
        size_t GetRequestLength() const { return request_length_; }
        size_t GetRequestCapacity() const { return REQUEST_BUFFER_SIZE; }
        void SetRequestLength(size_t len) { request_length_ = len; }

        char* GetResponseBuffer() { return response_buffer_.data(); }
        const char* GetResponseBuffer() const { return response_buffer_.data(); }
        size_t GetResponseLength() const { return response_length_; }
        size_t GetResponseCapacity() const { return RESPONSE_BUFFER_SIZE; }
        void SetResponseLength(size_t len) { response_length_ = len; }

        size_t GetResponseSent() const { return response_sent_; }
        void SetResponseSent(size_t sent) { response_sent_ = sent; }
        void IncrementResponseSent(size_t amount) { response_sent_ += amount; }
        bool IsResponseComplete() const { return response_sent_ >= response_length_; }

        uint32_t GetLastActivityTime() const { return last_activity_ms_; }
        void UpdateActivityTime(uint32_t time_ms) { last_activity_ms_ = time_ms; }
        bool IsTimedOut(uint32_t current_time_ms) const {
            return in_use_ && (current_time_ms - last_activity_ms_) > CONNECTION_TIMEOUT_MS;
        }

        bool SafeWriteResponse(const char* data, size_t length);
        bool SafeWriteResponseFormatted(const char* format, ...);

        void Reset() {
            tcp_handle_ = nullptr;
            request_length_ = 0;
            response_length_ = 0;
            response_sent_ = 0;
            last_activity_ms_ = 0;
            in_use_ = false;
        }

        static Connection* Acquire();
        static void Release(Connection* conn);
        static Connection* GetPool() { return g_connection_pool_.data(); }
        static size_t GetPoolSize() { return MAX_CONNECTIONS; }

    private:
        TcpHandle tcp_handle_ = nullptr;
        std::array<char, REQUEST_BUFFER_SIZE> request_buffer_{};
        std::array<char, RESPONSE_BUFFER_SIZE> response_buffer_{};
        size_t request_length_ = 0;
        size_t response_length_ = 0;
        size_t response_sent_ = 0;
        uint32_t last_activity_ms_ = 0;
        bool in_use_ = false;

        static std::array<Connection, MAX_CONNECTIONS> g_connection_pool_;
        void MarkInUse() { in_use_ = true; }
    };
}
