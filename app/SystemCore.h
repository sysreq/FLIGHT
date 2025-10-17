#pragma once

#include <atomic>
#include <array>
#include "pico/time.h"

namespace app::SystemCore::Global {
    static inline std::atomic<bool> system_active_{true};
}

template<typename Derived, size_t MaxTasks>
class SystemCore {
public:
    using TaskFuncPtr = void (Derived::*)();

private:
    struct Task {
        TaskFuncPtr func_ptr;
        uint32_t    interval_ms;
        uint32_t    last_run_ms = 0;
    };

    std::array<Task, MaxTasks> tasks_{};
    size_t num_tasks_ = 0;

    SystemCore(const SystemCore&) = delete;
    SystemCore& operator=(const SystemCore&) = delete;

    void run_scheduled_tasks() {
        uint32_t now_ms = time_us_32() / 1000;
        Derived* self = static_cast<Derived*>(this);

        for (size_t i = 0; i < num_tasks_; ++i) {
            auto& task = tasks_[i];
            if (now_ms - task.last_run_ms >= task.interval_ms) {
                if (task.func_ptr) {
                    (self->*task.func_ptr)();
                }
                task.last_run_ms = now_ms;
            }
        }
    }

public:
    bool init() {
        return static_cast<Derived*>(this)->init_impl();
    }

    void loop() {
        while (is_system_active()) {
            run_scheduled_tasks();
            static_cast<Derived*>(this)->loop_impl();
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

    // The derived class uses this to schedule its own member functions.
    bool add_task(TaskFuncPtr func, uint32_t interval_ms) {
        if (num_tasks_ >= MaxTasks) {
            return false;
        }
        tasks_[num_tasks_] = {func, interval_ms, 0};
        num_tasks_++;
        return true;
    }
};