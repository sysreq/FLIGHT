#pragma once

#include <functional>
#include <array>
#include <utility>
#include "pico/time.h"

static constexpr uint8_t MAX_TASKS = 8;

using TaskFuncPtr = void (*)(void*);

class Scheduler {
private:
    struct Task {
        TaskFuncPtr func_ptr;
        uint32_t interval_ms;
        uint32_t last_run_ms;
    };

    std::array<Task, MAX_TASKS> tasks_{};
    size_t num_tasks_ = 0;
    void* context_ = nullptr;

public:
    void set_context(void* ctx) {
        context_ = ctx;
    }

bool add_task(TaskFuncPtr func, uint32_t interval_ms) {
        if (num_tasks_ >= MAX_TASKS) {
            return false; 
        }

        tasks_[num_tasks_] = {func, interval_ms, 0};
        num_tasks_++;
        return true;
    }

    void run() {
        uint32_t now_ms = time_us_32() / 1000;

        for (size_t i = 0; i < num_tasks_; ++i) {
            auto& task = tasks_[i];
            if (now_ms - task.last_run_ms >= task.interval_ms) {
                if (task.func_ptr && context_) {
                    task.func_ptr(context_);
                }
                task.last_run_ms = now_ms;
            }
        }
    }
};
