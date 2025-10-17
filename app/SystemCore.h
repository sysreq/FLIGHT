#pragma once

#include <atomic>

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
            static constexpr uint32_t TARGET_TIME = 1000; 
            uint32_t start = time_us_32();
            static_cast<Derived*>(this)->loop_impl();
            int32_t delta_us = 1000 - (time_us_32() - start);
            delta_us &= ~(delta_us >> 31);
            sleep_us(delta_us);
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
