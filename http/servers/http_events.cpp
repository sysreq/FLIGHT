#include "http_events.h"

namespace http::servers {

HttpEventHandler::HttpEventHandler() {}

HttpEventHandler::~HttpEventHandler() {
    if (initialized_) {
        critical_section_deinit(&mutex_);
    }
}

void HttpEventHandler::initialize() {
    if (!initialized_) {
        critical_section_init(&mutex_);
        initialized_ = true;
    }
}

Event HttpEventHandler::parse_event(const char* request) const {
    Event event;
    
    // Find the event parameter in the query string
    const char* query = std::strchr(request, '?');
    if (!query) return event;
    
    // Look for "e=" parameter (short for event)
    const char* e_param = std::strstr(query, "e=");
    if (!e_param) return event;
    
    e_param += 2; // Skip "e="
    
    // Extract event name (up to & or space)
    char name_buf[64] = {0};
    const char* end = std::strpbrk(e_param, "& \r\n");
    size_t name_len = end ? (end - e_param) : std::strlen(e_param);
    if (name_len > sizeof(name_buf) - 1) {
        name_len = sizeof(name_buf) - 1;
    }
    std::memcpy(name_buf, e_param, name_len);
    event.name = name_buf;
    
    // Parse optional parameters: v1, v2, f
    const char* v1 = std::strstr(query, "&v1=");
    if (v1) event.value1 = std::atoi(v1 + 4);
    
    const char* v2 = std::strstr(query, "&v2=");
    if (v2) event.value2 = std::atoi(v2 + 4);
    
    const char* f = std::strstr(query, "&f=");
    if (f) event.fvalue = std::atof(f + 3);
    
    return event;
}

bool HttpEventHandler::process_request(const char* request, size_t len) {
    if (!initialized_) return false;
    
    Event event = parse_event(request);
    if (event.name.empty()) return false;
    
    critical_section_enter_blocking(&mutex_);
    
    bool queued = false;
    if (event_queue_.size() < MAX_QUEUE_SIZE) {
        event_queue_.push(event);
        queued = true;
        printf("EVENT: Queued '%s' (v1=%d, v2=%d, f=%.2f)\n", 
               event.name.c_str(), event.value1, event.value2, event.fvalue);
    } else {
        printf("EVENT: Queue full, dropped '%s'\n", event.name.c_str());
    }
    
    critical_section_exit(&mutex_);
    return queued;
}

bool HttpEventHandler::pop_event(Event& event) {
    if (!initialized_) return false;
    
    critical_section_enter_blocking(&mutex_);
    
    bool has_event = false;
    if (!event_queue_.empty()) {
        event = event_queue_.front();
        event_queue_.pop();
        has_event = true;
    }
    
    critical_section_exit(&mutex_);
    return has_event;
}

bool HttpEventHandler::has_events() const {
    if (!initialized_) return false;
    
    critical_section_enter_blocking(&mutex_);
    bool has = !event_queue_.empty();
    critical_section_exit(&mutex_);
    return has;
}

size_t HttpEventHandler::queue_size() const {
    if (!initialized_) return 0;
    
    critical_section_enter_blocking(&mutex_);
    size_t size = event_queue_.size();
    critical_section_exit(&mutex_);
    return size;
}

void HttpEventHandler::clear() {
    if (!initialized_) return;
    
    critical_section_enter_blocking(&mutex_);
    while (!event_queue_.empty()) {
        event_queue_.pop();
    }
    critical_section_exit(&mutex_);
}

} // namespace http::servers