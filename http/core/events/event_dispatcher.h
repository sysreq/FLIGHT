#pragma once

#include <cstddef>
#include <array>
#include "event_types.h"
#include "../http_utils.h"

namespace http::core {

using EventHandler = void(*)(const Event& event, void* context);

class EventDispatcher {
public:
    static constexpr size_t MAX_EVENT_TYPES = constants::MAX_EVENT_TYPES;

    EventDispatcher() {
        handlers_.fill(nullptr);
        contexts_.fill(nullptr);
    }

    bool register_handler(EventType type, EventHandler handler, void* context = nullptr) {
        size_t index = static_cast<size_t>(type);
        if (index >= MAX_EVENT_TYPES) {
            return false;
        }

        handlers_[index] = handler;
        contexts_[index] = context;
        return true;
    }

    void unregister_handler(EventType type) {
        size_t index = static_cast<size_t>(type);
        if (index < MAX_EVENT_TYPES) {
            handlers_[index] = nullptr;
            contexts_[index] = nullptr;
        }
    }

    bool dispatch(const Event& event) {
        size_t index = static_cast<size_t>(event.type);
        if (index >= MAX_EVENT_TYPES || handlers_[index] == nullptr) {
            return false;
        }

        handlers_[index](event, contexts_[index]);
        return true;
    }

    bool has_handler(EventType type) const {
        size_t index = static_cast<size_t>(type);
        return index < MAX_EVENT_TYPES && handlers_[index] != nullptr;
    }

private:
    std::array<EventHandler, MAX_EVENT_TYPES> handlers_;
    std::array<void*, MAX_EVENT_TYPES> contexts_;
};

} // namespace http::core
