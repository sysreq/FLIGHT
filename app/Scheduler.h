#pragma once

#include <functional>
#include <array>
#include <utility>
#include "pico/time.h"

static constexpr uint8_t MAX_TASKS = 8;

class Scheduler {
private:
    struct Task {
        std::function<void()> func;
        uint32_t interval_ms;
        uint32_t last_run_ms;
    };

    std::array<Task, MAX_TASKS> tasks_{};
    size_t num_tasks_ = 0;

public:
    bool add_task(std::function<void()> func, uint32_t interval_ms) {
        if (num_tasks_ >= MAX_TASKS) {
            return false; 
        }

        tasks_[num_tasks_] = {std::move(func), interval_ms, 0};
        num_tasks_++;
        return true;
    }

    void run() {
        uint32_t now_ms = time_us_32() / 1000;

        for (size_t i = 0; i < num_tasks_; ++i) {
            auto& task = tasks_[i];
            if (now_ms - task.last_run_ms >= task.interval_ms) {
                task.func();
                task.last_run_ms = now_ms;
            }
        }
    }
};
