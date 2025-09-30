#pragma once

#include "..\config\http_config.h"
#include "http_events.h"

namespace http::servers {

// Application state and status response handler
class HttpStatusHandler {
public:
    HttpStatusHandler() = default;
    ~HttpStatusHandler() = default;
    
    // Process an event and update application state
    void handle_event(const Event& event);
    
    // Generate JSON status response
    size_t generate_status_response(char* buffer, size_t buffer_size);
    
    // Generate complete HTTP status response with headers
    size_t generate_http_status_response(char* buffer, size_t buffer_size);
    
    // State accessors
    bool is_timer_running() const { return timer_running_; }
    uint32_t get_elapsed_seconds() const;
    
    // Reset all state
    void reset();

private:
    // Application state
    bool timer_running_ = false;
    uint32_t timer_start_ms_ = 0;
    
    // Could add more state here like:
    // - Last event timestamp
    // - Event counters
    // - Connection statistics
    // - Error counts, etc.
    
    // Statistics
    uint32_t start_count_ = 0;
    uint32_t stop_count_ = 0;
};

} // namespace http::servers