# lib/CMakeLists.txt - Third-party library management
# ============================================================================

message(STATUS "Configuring third-party libraries...")

message(STATUS "  Adding SH2 sensor library...")
add_subdirectory(sh2)

# ============================================================================
# Create combined interface library for all third-party deps
# ============================================================================
add_library(third_party INTERFACE)
target_link_libraries(third_party INTERFACE
    sh2
)

message(STATUS "Third-party libraries configured")