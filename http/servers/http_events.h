#pragma once

#include "..\config\http_config.h"

namespace http::servers {

// Simple event structure
struct Event {
    std::string name;
    int32_t value1 = 0;
    int32_t value2 = 0;
    float fvalue = 0.0f;
    uint32_t timestamp;
    
    Event() : timestamp(to_ms_since_boot(get_absolute_time())) {}
    Event(const std::string& n) : name(n), timestamp(to_ms_since_boot(get_absolute_time())) {}
};

class HttpEventHandler {
public:
    static constexpr size_t MAX_QUEUE_SIZE = 32;
    
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

} // namespace http::servers