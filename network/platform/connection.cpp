#include "connection.h"
#include <cstring>
#include <cstdio>
#include <cstdarg>

namespace network::platform {
    std::array<Connection, MAX_CONNECTIONS> Connection::g_connection_pool_{};

    Connection* Connection::Acquire() {
        for (auto& conn : g_connection_pool_) {
            if (!conn.in_use_) {
                conn.Reset();
                conn.MarkInUse();
                return &conn;
            }
        }
        return nullptr;
    }

    void Connection::Release(Connection* conn) {
        if (conn) {
            conn->Reset();
        }
    }

    bool Connection::SafeWriteResponse(const char* data, size_t length) {
        if (!data || length == 0) {
            return true;
        }

        if (response_length_ + length > RESPONSE_BUFFER_SIZE) {
            printf("Connection: Response overflow prevented (%zu + %zu > %zu)\n",
                   response_length_, length, RESPONSE_BUFFER_SIZE);
            return false;
        }

        std::memcpy(response_buffer_.data() + response_length_, data, length);
        response_length_ += length;
        return true;
    }

    bool Connection::SafeWriteResponseFormatted(const char* format, ...) {
        if (!format) {
            return false;
        }

        // Calculate available space
        size_t available = RESPONSE_BUFFER_SIZE - response_length_;
        if (available == 0) {
            printf("Connection: Response buffer full\n");
            return false;
        }

        // Format into remaining buffer space
        va_list args;
        va_start(args, format);
        int written = vsnprintf(response_buffer_.data() + response_length_,
                                available, format, args);
        va_end(args);

        if (written < 0) {
            printf("Connection: Format error\n");
            return false;
        }

        if (static_cast<size_t>(written) >= available) {
            printf("Connection: Response overflow prevented (formatted %d bytes, available %zu)\n",
                   written, available);
            return false;
        }

        response_length_ += written;
        return true;
    }
}
