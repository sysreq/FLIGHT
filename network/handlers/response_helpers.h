#pragma once
#include "../platform/connection.h"
#include <cstdio>
#include <cstring>

namespace network::handlers {
    inline bool SendJsonResponse(platform::Connection& conn, const char* json_body) {
        size_t body_len = strlen(json_body);
        return conn.SafeWriteResponseFormatted(
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %zu\r\n"
            "\r\n",
            body_len) && conn.SafeWriteResponse(json_body, body_len);
    }

    inline bool SendJsonResponse(platform::Connection& conn, const char* body, int body_len) {
        if (body_len < 0) return false;
        return conn.SafeWriteResponseFormatted(
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %d\r\n"
            "\r\n",
            body_len) && conn.SafeWriteResponse(body, body_len);
    }

    inline bool SendPlainTextResponse(platform::Connection& conn, const char* text, int status_code = 200, const char* status_text = "OK") {
        size_t text_len = strlen(text);
        return conn.SafeWriteResponseFormatted(
            "HTTP/1.1 %d %s\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: %zu\r\n"
            "\r\n",
            status_code, status_text, text_len) && conn.SafeWriteResponse(text, text_len);
    }

    inline bool SendHtmlResponse(platform::Connection& conn, const char* html) {
        size_t html_len = strlen(html);
        return conn.SafeWriteResponseFormatted(
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "Content-Length: %zu\r\n"
            "\r\n",
            html_len) && conn.SafeWriteResponse(html, html_len);
    }
}
