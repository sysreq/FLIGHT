add_library(network
    network.cpp
    platform/config.cpp
    platform/lwip_wrapper.cpp
    platform/pico_wrapper.cpp
    services/service_manager.cpp
    services/http_service.cpp
    services/dhcp_service.cpp
    services/dns_service.cpp
    handlers/dispatcher.cpp
    handlers/handler_index.cpp
    handlers/handler_status.cpp
)

target_include_directories(network PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(network PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/platform
)

target_link_libraries(network PRIVATE
    pico_stdlib
    pico_cyw43_arch_lwip_poll
    hardware_timer
)

target_compile_options(network PRIVATE
    -Wall -Wextra -Werror
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
)

target_compile_features(network PRIVATE cxx_std_23)
