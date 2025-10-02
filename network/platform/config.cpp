#include "config.h"

namespace network::platform {
    Connection* AcquireConnection() {
        for (auto& conn : g_connection_pool) {
            if (!conn.in_use) {
                // Reset connection state but preserve the in_use flag
                conn.tcp_handle = nullptr;
                conn.request_length = 0;
                conn.response_length = 0;
                conn.in_use = true;  // Mark as in use AFTER resetting other fields
                return &conn;
            }
        }
        return nullptr;
    }

    void ReleaseConnection(Connection* conn) {
        if (conn) {
            conn->Reset();
        }
    }
}
