# lib/CMakeLists.txt - Third-party library management
# ============================================================================

message(STATUS "Configuring third-party libraries...")

message(STATUS "  Adding SD Card library...")
add_subdirectory(sdcard)

# ============================================================================
# Create combined interface library for all third-party deps
# ============================================================================
add_library(third_party INTERFACE)
target_link_libraries(third_party INTERFACE
    sdcard
)

message(STATUS "Third-party libraries configured")