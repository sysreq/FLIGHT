#include "http_status.h"
#include "..\ui\http_generator.h"  // Include to access telemetry values

namespace http::servers {

void HttpStatusHandler::handle_event(const Event& event) {
    if (event.name == "start") {
        if (!timer_running_) {
            timer_running_ = true;
            timer_start_ms_ = to_ms_since_boot(get_absolute_time());
            start_count_++;
            printf("Timer started (count: %u)\n", start_count_);
        }
    } else if (event.name == "stop") {
        if (timer_running_) {
            timer_running_ = false;
            stop_count_++;
            printf("Timer stopped after %u seconds (count: %u)\n", 
                   get_elapsed_seconds(), stop_count_);
        }
    }
    // Add more event handlers as needed
}

uint32_t HttpStatusHandler::get_elapsed_seconds() const {
    if (!timer_running_) {
        return 0;
    }
    uint32_t now_ms = to_ms_since_boot(get_absolute_time());
    return (now_ms - timer_start_ms_) / 1000;
}

size_t HttpStatusHandler::generate_status_response(char* buffer, size_t buffer_size) {
    printf("Status'd.\n");
    return snprintf(buffer, buffer_size,
        "{\"running\":%s,\"elapsed\":%u,\"start_count\":%u,\"stop_count\":%u,"
        "\"speed\":%.2f,\"altitude\":%.1f,\"force\":%.3f}",
        timer_running_ ? "true" : "false",
        get_elapsed_seconds(),
        start_count_,
        stop_count_,
        HttpGenerator::speed,
        HttpGenerator::altitude,
        HttpGenerator::force
    );
}

size_t HttpStatusHandler::generate_http_status_response(char* buffer, size_t buffer_size) {
    char json[256];
    size_t json_len = generate_status_response(json, sizeof(json));
    
    // Then wrap with HTTP headers
    return snprintf(buffer, buffer_size,
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: %zu\r\n"
        "Content-Type: application/json\r\n"
        "Cache-Control: no-cache, no-store, must-revalidate\r\n"
        "Connection: close\r\n"
        "\r\n%s",
        json_len, json
    );
}

void HttpStatusHandler::reset() {
    timer_running_ = false;
    timer_start_ms_ = 0;
    start_count_ = 0;
    stop_count_ = 0;
}

} // namespace http::servers