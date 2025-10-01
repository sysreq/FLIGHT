#pragma once

#include <cstdint>
#include <cstdio>
#include "event_types.h"
#include "../http_utils.h"

namespace http::core {

class TimerEventHandler {
public:
    TimerEventHandler() = default;

    static void handle_event(const Event& event, void* context) {
        auto* handler = static_cast<TimerEventHandler*>(context);
        if (!handler) return;

        switch (event.type) {
            case EventType::TimerStart:
                handler->handle_start();
                break;
            case EventType::TimerStop:
                handler->handle_stop();
                break;
            default:
                break;
        }
    }

    bool is_timer_running() const { return timer_running_; }
    uint32_t get_elapsed_seconds() const {
        if (!timer_running_) return 0;
        uint32_t now_ms = to_ms_since_boot(get_absolute_time());
        return utils::ms_to_seconds(now_ms - timer_start_ms_);
    }
    uint32_t get_start_count() const { return start_count_; }
    uint32_t get_stop_count() const { return stop_count_; }

    void reset() {
        timer_running_ = false;
        timer_start_ms_ = 0;
        start_count_ = 0;
        stop_count_ = 0;
    }

private:
    void handle_start() {
        if (!timer_running_) {
            timer_running_ = true;
            timer_start_ms_ = to_ms_since_boot(get_absolute_time());
            start_count_++;
            printf("Timer started (count: %u)\n", start_count_);
        }
    }

    void handle_stop() {
        if (timer_running_) {
            timer_running_ = false;
            stop_count_++;
            printf("Timer stopped after %u seconds (count: %u)\n",
                   get_elapsed_seconds(), stop_count_);
        }
    }

    bool timer_running_ = false;
    uint32_t timer_start_ms_ = 0;
    uint32_t start_count_ = 0;
    uint32_t stop_count_ = 0;
};

} // namespace http::core
