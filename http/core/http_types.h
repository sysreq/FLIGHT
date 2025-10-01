#pragma once

#include <cstdint>
#include <cstring>
#include <algorithm>

namespace http::core {

struct HttpRequest {
    const char* method;
    const char* path;
    const char* query_string;
    const char* full_request;
    size_t request_len;
};

struct HttpResponse {
    char* buffer;
    size_t buffer_size;
    size_t content_length;

    size_t write(const char* data, size_t len) {
        size_t to_write = std::min(len, buffer_size - content_length);
        std::memcpy(buffer + content_length, data, to_write);
        content_length += to_write;
        return to_write;
    }
};

using RouteHandler = size_t(*)(const HttpRequest& req, HttpResponse& resp, void* context);

struct Route {
    const char* path;
    const char* method;
    RouteHandler handler;
    void* context;

    Route() : path(nullptr), method(nullptr), handler(nullptr), context(nullptr) {}

    Route(const char* p, const char* m, RouteHandler h, void* ctx = nullptr)
        : path(p), method(m), handler(h), context(ctx) {}

    bool matches(const char* request_path, const char* request_method) const {
        if (!path || !method) return false;
        return std::strcmp(path, request_path) == 0 &&
               std::strcmp(method, request_method) == 0;
    }
};

} // namespace http::core
