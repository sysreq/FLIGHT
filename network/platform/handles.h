#pragma once

namespace network::platform {
    // Opaque handle types - hide SDK implementation
    using TcpHandle = void*;
    using TcpListenerHandle = void*;
    using UdpHandle = void*;
    using ConnectionHandle = void*;

    // Future extension points
    using DnsHandle = void*;
    using DhcpHandle = void*;
}
