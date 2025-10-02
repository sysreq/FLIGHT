#pragma once
#include "../platform/config.h"

namespace network::handlers {
    void HandleSessionStart(platform::Connection& conn);
    void HandleSessionStop(platform::Connection& conn);
    void HandleSessionStatus(platform::Connection& conn);
}
