#include "service_manager.h"
#include "http_service.h"
#include "dhcp_service.h"
#include "dns_service.h"

namespace network::services {
    // Verify services satisfy Service concept
    static_assert(Service<HttpService>, "HttpService must satisfy Service concept");
    static_assert(Service<DhcpService>, "DhcpService must satisfy Service concept");
    static_assert(Service<DnsService>, "DnsService must satisfy Service concept");

    // Test instantiation of ServiceManager with services
    namespace test {
        // This ensures the template compiles correctly
        using TestManager = ServiceManager<HttpService>;

        // Test with zero services
        using EmptyManager = ServiceManager<>;

        // Test with multiple services
        using MultiManager = ServiceManager<HttpService, DhcpService, DnsService>;
    }
}
