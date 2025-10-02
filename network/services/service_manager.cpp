#include "service_manager.h"
#include "http_service.h"

namespace network::services {
    // Verify HttpService satisfies Service concept
    static_assert(Service<HttpService>, "HttpService must satisfy Service concept");

    // Test instantiation of ServiceManager with HttpService
    namespace test {
        // This ensures the template compiles correctly
        using TestManager = ServiceManager<HttpService>;

        // Test with zero services
        using EmptyManager = ServiceManager<>;

        // Test with multiple services (future)
        // using MultiManager = ServiceManager<HttpService, DnsService, DhcpService>;
    }
}
