add_library(filesystem STATIC)

# Source files for the filesystem library
target_sources(filesystem PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/filesystem.cpp
    ${CMAKE_CURRENT_LIST_DIR}/sdcard/sdcard.cpp
    ${CMAKE_CURRENT_LIST_DIR}/sdcard/spi_interface.cpp
    ${CMAKE_CURRENT_LIST_DIR}/fat32/volume.cpp
    ${CMAKE_CURRENT_LIST_DIR}/fat32/file.cpp
)

# Include directories for users of this library
target_include_directories(filesystem PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
)

# Dependencies for the filesystem library
target_link_libraries(filesystem PUBLIC
    hardware_spi
    pico_stdlib
)

# Linker optimizations for better binary size
target_link_options(filesystem PRIVATE
    -Wl,--gc-sections
    -Wl,--print-memory-usage 
)

# ========== OPTIMIZATION FLAGS FOR FILESYSTEM LIBRARY ==========
target_compile_options(filesystem PRIVATE
    -O2
    -ffast-math
    -finline-functions
    -funroll-loops

    -fomit-frame-pointer
    -ffunction-sections
    -fdata-sections
)
