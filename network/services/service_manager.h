#pragma once
#include "../include/concepts.h"
#include <tuple>
#include <utility>

namespace network::services {
    template<Service... Services>
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

    private:
        std::tuple<Services...> m_services;
    };
}
