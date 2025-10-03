#pragma once

#include <cstdint>
#include <cstddef>

#include "include/error.h"

namespace FileSystem::Fat32::Utils {

    static constexpr bool is_valid_8_3_char(char c) {
        return (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '_' || c == '~' || c == '!' ||
            c == '#' || c == '$' || c == '%' ||
            c == '&' || c == '\'' || c == '(' ||
            c == ')' || c == '-' || c == '@' ||
            c == '^' || c == '`' || c == '{' ||
            c == '}';
    }

    static char to_upper(char c) {
        return (c >= 'a' && c <= 'z') ? (c - 32) : c;
    }

    // ========== Byte Utility Functions ==========

    static inline uint16_t read_u16(const uint8_t* buffer) {
        return buffer[0] | (buffer[1] << 8);
    }

    static inline uint32_t read_u32(const uint8_t* buffer) {
        return buffer[0] | (buffer[1] << 8) | (buffer[2] << 16) | (buffer[3] << 24);
    }

    static inline void write_u16(uint8_t* buffer, uint16_t value) {
        buffer[0] = (uint8_t)value;
        buffer[1] = (uint8_t)(value >> 8);
    }

    static inline void write_u32(uint8_t* buffer, uint32_t value) {
        buffer[0] = (uint8_t)value;
        buffer[1] = (uint8_t)(value >> 8);
        buffer[2] = (uint8_t)(value >> 16);
        buffer[3] = (uint8_t)(value >> 24);
    }


    ErrorCode parse_8_3_name(const char* filename, char out_name[11]) {
        if (!filename || !out_name) {
            return ErrorCode::INVALID_PARAMETER;
        }

        // Initialize output with spaces
        for (int i = 0; i < 11; i++) {
            out_name[i] = ' ';
        }

        const char* dot = nullptr;
        size_t len = 0;

        // Find dot and length
        for (const char* p = filename; *p; p++) {
            if (*p == '.') {
                if (dot != nullptr) {
                    return ErrorCode::INVALID_NAME;  // Multiple dots
                }
                dot = p;
            }
            len++;
        }

        // Validate total length
        if (len == 0 || len > 12) {
            return ErrorCode::INVALID_NAME;
        }

        // Parse base name (up to 8 chars)
        size_t base_len = dot ? (size_t)(dot - filename) : len;
        if (base_len == 0 || base_len > 8) {
            return ErrorCode::INVALID_NAME;
        }

        for (size_t i = 0; i < base_len; i++) {
            char c = to_upper(filename[i]);
            if (!is_valid_8_3_char(c)) {
                return ErrorCode::INVALID_NAME;
            }
            out_name[i] = c;
        }

        // Parse extension (up to 3 chars)
        if (dot) {
            const char* ext = dot + 1;
            size_t ext_len = len - base_len - 1;

            if (ext_len == 0 || ext_len > 3) {
                return ErrorCode::INVALID_NAME;
            }

            for (size_t i = 0; i < ext_len; i++) {
                char c = to_upper(ext[i]);
                if (!is_valid_8_3_char(c)) {
                    return ErrorCode::INVALID_NAME;
                }
                out_name[8 + i] = c;
            }
        }

        return ErrorCode::NONE;
    }

} // namespace Utils