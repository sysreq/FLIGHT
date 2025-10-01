#pragma once

#include <queue>
#include <cstddef>
#include "events/event_types.h"
#include "http_utils.h"

#include "pico/critical_section.h"

namespace http::core {

class HttpEventHandler {
public:
    static constexpr size_t MAX_QUEUE_SIZE = constants::MAX_EVENT_QUEUE_SIZE;
    
    HttpEventHandler();
    ~HttpEventHandler();
    
    void initialize();
    
    bool process_request(const char* request, size_t len);
    bool pop_event(Event& event);
    bool has_events() const;
    size_t queue_size() const;
    void clear();

    Event parse_event(const char* request) const;
private:
    std::queue<Event> event_queue_;
    mutable critical_section_t mutex_;
    bool initialized_ = false;
};

} // namespace http::core