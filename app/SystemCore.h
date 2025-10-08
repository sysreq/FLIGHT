#pragma once

#include <atomic>

/**
 * @brief A base class for a core's main controller using the CRTP pattern
 * to eliminate virtual function overhead.
 *
 * @tparam Derived The concrete core controller class (e.g., Core0SystemCore).
 */

 namespace app::SystemCore::Global {
    static inline std::atomic<bool> system_active_{true};
 }

template<typename Derived>
class SystemCore {
private:
    SystemCore(const SystemCore&) = delete;
    SystemCore& operator=(const SystemCore&) = delete;

public:
    bool init() {
        return static_cast<Derived*>(this)->init_impl();
    }

    void loop() {
        while (is_system_active()) {
            static_cast<Derived*>(this)->loop_impl();
            sleep_ms(1);
        }
        static_cast<Derived*>(this)->shutdown_impl();
    }

    static void shutdown() {
        app::SystemCore::Global::system_active_.store(false);
    }

    static bool is_system_active() { return app::SystemCore::Global::system_active_.load(); }

protected:
    SystemCore() = default;
    ~SystemCore() = default;
};
