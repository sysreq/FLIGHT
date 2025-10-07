#include "http_service.h"
#include "../handlers/dispatcher.h"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <string_view>

namespace network::services {
    // HTTP configuration constants
    inline constexpr uint16_t HTTP_PORT = 80;
    inline constexpr size_t MAX_REQUEST_LINE_LENGTH = 512;
    inline constexpr size_t MAX_HEADER_COUNT = 16;
    bool HttpService::Start() {
        auto result = platform::lwip::CreateListener(HTTP_PORT);
        if (!result) {
            printf("HTTP: Failed to create listener: %s\n",
                   ErrorCodeToString(result.error()));
            return false;
        }

        m_listener = result.value();
        platform::lwip::SetupHttpListener(m_listener);
        printf("HTTP: Service started on port %u\n", HTTP_PORT);
        return true;
    }

    void HttpService::Stop() {
        if (m_listener) {
            platform::lwip::DestroyListener(m_listener);
            m_listener = nullptr;

            for (size_t i = 0; i < platform::Connection::GetPoolSize(); ++i) {
                auto& conn = platform::Connection::GetPool()[i];
                if (conn.IsInUse() && conn.GetTcpHandle()) {
                    platform::lwip::CloseConnection(conn.GetTcpHandle());
                    platform::Connection::Release(&conn);
                }
            }

            printf("HTTP: Service stopped\n");
        }
    }

    void HttpService::Process() {
    }

    void HttpService::AcceptNewConnections() {
    }

    void HttpService::ProcessActiveConnections() {
    }

    void HttpService::ParseRequest(platform::Connection& conn) {
        const char* buf = conn.GetRequestBuffer();
        size_t len = conn.GetRequestLength();

        if (len == 0) return;

        const char* method_end = static_cast<const char*>(memchr(buf, ' ', len));
        if (!method_end) return;

        std::string_view method(buf, method_end - buf);
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
            SendMethodNotAllowed(conn);
            return;
        }

        size_t remaining = len - (method_end - buf + 1);
        const char* path_start = method_end + 1;
        const char* path_end = static_cast<const char*>(memchr(path_start, ' ', remaining));
        if (!path_end) return;

        size_t path_len = path_end - path_start;
        if (path_len > platform::MAX_HTTP_PATH_LENGTH) {
            SendBadRequest(conn);
            return;
        }
        std::string_view path(path_start, path_len);

        const char* version_start = path_end + 1;
        const char* line_end = static_cast<const char*>(memchr(version_start, '\r',
                                                                remaining - (version_start - path_start)));
        if (line_end) {
            std::string_view version(version_start, line_end - version_start);
            if (version != "HTTP/1.0" && version != "HTTP/1.1") {
                SendBadRequest(conn);
                return;
            }
        }

        handlers::Dispatch(conn, path, http_method);
    }

    void HttpService::SendMethodNotAllowed(platform::Connection& conn) {
        const char* response =
            "HTTP/1.1 405 Method Not Allowed\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 18\r\n"
            "Allow: GET, POST, DELETE\r\n"
            "\r\n"
            "Method Not Allowed";
        conn.SafeWriteResponse(response, strlen(response));
    }

    void HttpService::SendBadRequest(platform::Connection& conn) {
        const char* response =
            "HTTP/1.1 400 Bad Request\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 11\r\n"
            "\r\n"
            "Bad Request";
        conn.SafeWriteResponse(response, strlen(response));
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