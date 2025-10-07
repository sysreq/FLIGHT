#pragma once

#include <functional>
#include <vector>
#include "pico/time.h"

/**
 * @brief A simple cooperative task scheduler.
 */
class Scheduler {
private:
    struct Task {
        std::function<void()> func;
        uint32_t interval_ms;
        uint32_t last_run_ms;
    };

    std::vector<Task> tasks_;

public:
    /**
     * @brief Adds a recurring task to the scheduler.
     * @param func The function to execute.
     * @param interval_ms The interval in milliseconds at which to run the task.
     */
    void add_task(std::function<void()> func, uint32_t interval_ms) {
        tasks_.push_back({std::move(func), interval_ms, 0});
    }

    /**
     * @brief Runs the scheduler. This should be called continuously in a loop.
     *
     * It checks all tasks and executes any that are due to be run.
     */
    void run() {
        uint32_t now_ms = time_us_32() / 1000;

        for (auto& task : tasks_) {
            // Correctly handle timer overflow (wraparound) by using subtraction.
            if (now_ms - task.last_run_ms >= task.interval_ms) {
                task.func();
                task.last_run_ms = now_ms;
            }
        }
    }
};
