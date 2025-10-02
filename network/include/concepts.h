#pragma once
#include <concepts>

namespace network {
    template<typename T>
    concept Service = requires(T t) {
        { t.Start() } -> std::same_as<bool>;
        { t.Stop() } -> std::same_as<void>;
        { t.Process() } -> std::same_as<void>;
    };
}
