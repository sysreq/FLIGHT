#pragma once

#include <cstdio>
#include <cmath>
#include <type_traits>

template<typename T>
inline bool is_failure(T expr) {
    if constexpr (std::is_same_v<std::remove_cvref_t<T>, int>) {
        return expr < 0;
    } else if constexpr (std::is_same_v<std::remove_cvref_t<T>, bool>) {
        return expr == false;
    }
    return false;
}

#define REQUIRE(expr, msg) \
    if (is_failure(expr)) { \
        printf(msg); \
        sleep_ms(50); \
        return -1; \
    }

#define POLL_EVERY(action, timer, delay) \
        if(now - timer >= delay) { \
            action; \
            timer = now; \
        }
// done