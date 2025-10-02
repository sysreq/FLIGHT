#pragma once
#include "../platform/lwip_wrapper.h"
#include "../types.h"

namespace network::services {
    class HttpService {
    public:
        bool Start();
        void Stop();
        void Process();

        // Static helper for LWIP callbacks
        static void ParseAndRespond(platform::Connection& conn);

    private:
        platform::TcpListenerHandle m_listener = nullptr;

        void AcceptNewConnections();
        void ProcessActiveConnections();
        void ParseRequest(platform::Connection& conn);
        void SendResponse(platform::Connection& conn);

        // Error response helpers
        void SendMethodNotAllowed(platform::Connection& conn);
        void SendBadRequest(platform::Connection& conn);
    };
}
