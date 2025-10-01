#pragma once
// ============================================
#include <concepts>
#include <cstdint>
#include <type_traits>

#include <pico\stdlib.h>
#include <pico\stdio.h>
// ============================================
// ERROR HANDLING UTILITIES
// ============================================
template<typename T>
concept ResultType = std::same_as<T, bool> || std::same_as<T, int>;
// ============================================
template<ResultType T>
inline bool is_failure(T result) {
    if constexpr (std::same_as<T, bool>) {
        return !result;
    } else if constexpr (std::same_as<T, int>) {
        return result < 0;
    }
}
// ============================================
#define __REQUIRE_RETPARAM(expr, msg, retval)   \
    do {                                        \
        auto _result = (expr);                  \
        if (is_failure(_result)) {              \
            printf("%s", msg);                  \
            sleep_ms(100);                      \
            return retval;                      \
        }                                       \
    } while(0)
// ============================================
#define REQUIRE(expr, msg) __REQUIRE_RETPARAM(expr, msg, false)
#define REQUIREV(expr, msg) __REQUIRE_RETPARAM(expr, msg, )
#define REQUIRET(expr, msg) __REQUIRE_RETPARAM(expr, msg, true)
#define REQUIREN(expr, msg) __REQUIRE_RETPARAM(expr, msg, -1)