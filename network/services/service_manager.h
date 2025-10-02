#pragma once
#include "../types.h"
#include <tuple>
#include <utility>

namespace network::services {
    template<network::Service... Services>
    class ServiceManager {
    public:
        ServiceManager() = default;

        bool StartAll() {
            bool success = true;
            std::apply([&success](auto&... svc) {
                ((success = success && svc.Start()), ...);
            }, m_services);
            return success;
        }

        void StopAll() {
            std::apply([](auto&... svc) {
                (svc.Stop(), ...);
            }, m_services);
        }

        void ProcessAll() {
            std::apply([](auto&... svc) {
                (svc.Process(), ...);
            }, m_services);
        }

        // Access to underlying services tuple for configuration
        std::tuple<Services...>& GetServices() {
            return m_services;
        }

    private:
        std::tuple<Services...> m_services;
    };
}
