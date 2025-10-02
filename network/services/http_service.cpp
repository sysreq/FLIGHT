#include "http_service.h"
#include "../handlers/dispatcher.h"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <string_view>

namespace network::services {
    bool HttpService::Start() {
        auto result = platform::lwip::CreateListener(HTTP_PORT);
        if (!result) {
            printf("HTTP: Failed to create listener: %s\n",
                   ErrorCodeToString(result.error()));
            return false;
        }

        m_listener = result.value();

        // Setup HTTP handling callbacks
        platform::lwip::SetupHttpListener(m_listener);

        printf("HTTP: Service started on port %u\n", HTTP_PORT);
        return true;
    }

    void HttpService::Stop() {
        if (m_listener) {
            platform::lwip::DestroyListener(m_listener);
            m_listener = nullptr;

            // Close all active connections
            for (auto& conn : platform::g_connection_pool) {
                if (conn.in_use && conn.tcp_handle) {
                    platform::lwip::CloseConnection(conn.tcp_handle);
                    platform::ReleaseConnection(&conn);
                }
            }

            printf("HTTP: Service stopped\n");
        }
    }

    void HttpService::Process() {
        // LWIP handles everything via callbacks, nothing to do here
        // This function exists to satisfy the Service concept
    }

    void HttpService::AcceptNewConnections() {
        // Handled by LWIP callbacks in lwip_wrapper.cpp
    }

    void HttpService::ProcessActiveConnections() {
        // Handled by LWIP callbacks in lwip_wrapper.cpp
    }

    void HttpService::ParseRequest(platform::Connection& conn) {
        // Extract method, path, and version from first line
        // Format: "GET /path HTTP/1.1\r\n"

        const char* buf = conn.request_buffer.data();
        size_t len = conn.request_length;

        if (len == 0) {
            return;
        }

        // Find first space (after method)
        const char* method_end = static_cast<const char*>(memchr(buf, ' ', len));
        if (!method_end) {
            return;
        }

        // Extract method as string_view
        size_t method_len = method_end - buf;
        std::string_view method(buf, method_len);

        // Validate HTTP method (only support GET for now)
        HttpMethod http_method;
        if (method == "GET") {
            http_method = HttpMethod::GET;
        } else if (method == "POST") {
            http_method = HttpMethod::POST;
        } else if (method == "PUT") {
            http_method = HttpMethod::PUT;
        } else if (method == "DELETE") {
            http_method = HttpMethod::DELETE;
        } else if (method == "PATCH") {
            http_method = HttpMethod::PATCH;
        } else if (method == "HEAD") {
            http_method = HttpMethod::HEAD;
        } else if (method == "OPTIONS") {
            http_method = HttpMethod::OPTIONS;
        } else {
            // Unsupported method - send 405 Method Not Allowed
            SendMethodNotAllowed(conn);
            return;
        }

        // Find second space (after path)
        size_t remaining = len - (method_end - buf + 1);
        const char* path_start = method_end + 1;
        const char* path_end = static_cast<const char*>(memchr(path_start, ' ', remaining));
        if (!path_end) {
            return;
        }

        // Extract path as string_view
        size_t path_len = path_end - path_start;
        std::string_view path(path_start, path_len);

        // Extract HTTP version
        const char* version_start = path_end + 1;
        const char* line_end = static_cast<const char*>(memchr(version_start, '\r',
                                                                remaining - (version_start - path_start)));
        if (line_end) {
            std::string_view version(version_start, line_end - version_start);

            // Validate HTTP version
            if (version != "HTTP/1.0" && version != "HTTP/1.1") {
                SendBadRequest(conn);
                return;
            }
        }

        // For now, we only handle GET requests properly
        if (http_method != HttpMethod::GET) {
            SendMethodNotAllowed(conn);
            return;
        }

        // Dispatch to handler
        handlers::Dispatch(conn, path);
    }

    void HttpService::SendMethodNotAllowed(platform::Connection& conn) {
        const char* response =
            "HTTP/1.1 405 Method Not Allowed\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 18\r\n"
            "Allow: GET\r\n"
            "\r\n"
            "Method Not Allowed";

        size_t len = strlen(response);
        if (len <= conn.response_buffer.size()) {
            std::memcpy(conn.response_buffer.data(), response, len);
            conn.response_length = len;
        }
    }

    void HttpService::SendBadRequest(platform::Connection& conn) {
        const char* response =
            "HTTP/1.1 400 Bad Request\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 11\r\n"
            "\r\n"
            "Bad Request";

        size_t len = strlen(response);
        if (len <= conn.response_buffer.size()) {
            std::memcpy(conn.response_buffer.data(), response, len);
            conn.response_length = len;
        }
    }

    void HttpService::SendResponse(platform::Connection& conn) {
        // Response sending is now handled in lwip_wrapper.cpp
        // This function is no longer needed but kept for compatibility
        if (conn.response_length == 0 || !conn.tcp_handle) {
            return;
        }

        // Send response via LWIP wrapper
        std::span<const char> data(conn.response_buffer.data(), conn.response_length);
        auto result = platform::lwip::Send(conn.tcp_handle, data);

        if (!result) {
            printf("HTTP: Send failed: %s\n", ErrorCodeToString(result.error()));
        }
    }

    // Static helper for callback use
    void HttpService::ParseAndRespond(platform::Connection& conn) {
        HttpService service;  // Temporary instance for method calls
        service.ParseRequest(conn);
        // Response sending is handled by lwip_wrapper.cpp
    }
}

// Implementation of ProcessHttpRequest - overrides weak symbol in lwip_wrapper.cpp
namespace network::platform::lwip {
    void ProcessHttpRequest(Connection& conn) {
        // Parse the HTTP request and generate response
        network::services::HttpService::ParseAndRespond(conn);
    }
}