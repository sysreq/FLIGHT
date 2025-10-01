#pragma once
// ============================================
#include <pico\stdlib.h>
#include <pico\stdio.h>
// ============================================
// ERROR HANDLING UTILITIES
// ============================================
#define PRINT_HEADER(text) \
    do { \
        constexpr size_t text_len = sizeof(text) - 1;                       \
        constexpr int width = 40;                                           \
        const char* eq_line = "========================================\n"; \
        const char* spaces = "                                        ";    \
        printf("%s", eq_line);                                              \
        if (text_len >= static_cast<size_t>(width)) {                       \
            printf("%s\n", text);                                           \
        } else {                                                            \
            constexpr int total_padding = width - text_len;                 \
            constexpr int left_padding = total_padding / 2;                 \
            constexpr int right_padding = total_padding - left_padding;     \
            printf("%.*s%s%.*s\n", left_padding, spaces, text,              \
                right_padding, spaces);                                     \
        }                                                                   \
        printf("%s", eq_line);                                              \
    } while (0)
// ============================================
#define PRINT_BANNER(level, text)                                           \
    do {                                                                    \
        constexpr size_t text_len = sizeof(text) - 1;                       \
        int N = 40 - static_cast<int>(text_len) - 10 * (level);             \
        int left = N / 2;                                                   \
        int right = N - left;                                               \
        int indent_size = (level) * 4;                                      \
        const char* spaces = "                                        ";    \
        const char* dashes = "----------------------------------------";    \
        printf("%.*s%.*s %s %.*s%.*s\n", indent_size, spaces, left, dashes, \
            text, right, dashes, indent_size, spaces);                      \
    } while (0)

