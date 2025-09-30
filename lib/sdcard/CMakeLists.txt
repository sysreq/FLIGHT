add_library(sdcard INTERFACE)

# Generate PIO header
pico_generate_pio_header(sdcard ${CMAKE_CURRENT_LIST_DIR}/src/sd_driver/SDIO/rp2040_sdio.pio)

# Compile definitions
target_compile_definitions(sdcard INTERFACE
    PICO_MAX_SHARED_IRQ_HANDLERS=8u
)

# Source files
target_sources(sdcard INTERFACE
    # FatFS sources
    ${CMAKE_CURRENT_LIST_DIR}/src/ff15/source/ff.c
    ${CMAKE_CURRENT_LIST_DIR}/src/ff15/source/ffsystem.c
    ${CMAKE_CURRENT_LIST_DIR}/src/ff15/source/ffunicode.c
    
    # SD driver core
    ${CMAKE_CURRENT_LIST_DIR}/src/sd_driver/dma_interrupts.c
    ${CMAKE_CURRENT_LIST_DIR}/src/sd_driver/sd_card.c
    ${CMAKE_CURRENT_LIST_DIR}/src/sd_driver/sd_timeouts.c
    
    # SDIO driver
    ${CMAKE_CURRENT_LIST_DIR}/src/sd_driver/SDIO/rp2040_sdio.c
    ${CMAKE_CURRENT_LIST_DIR}/src/sd_driver/SDIO/sd_card_sdio.c
    
    # SPI driver
    ${CMAKE_CURRENT_LIST_DIR}/src/sd_driver/SPI/my_spi.c
    ${CMAKE_CURRENT_LIST_DIR}/src/sd_driver/SPI/sd_card_spi.c
    ${CMAKE_CURRENT_LIST_DIR}/src/sd_driver/SPI/sd_spi.c
    
    # Utility sources
    ${CMAKE_CURRENT_LIST_DIR}/src/src/crash.c
    ${CMAKE_CURRENT_LIST_DIR}/src/src/crc.c
    ${CMAKE_CURRENT_LIST_DIR}/src/src/f_util.c
    ${CMAKE_CURRENT_LIST_DIR}/src/src/ff_stdio.c
    ${CMAKE_CURRENT_LIST_DIR}/src/src/file_stream.c
    ${CMAKE_CURRENT_LIST_DIR}/src/src/glue.c
    ${CMAKE_CURRENT_LIST_DIR}/src/src/my_debug.c
    ${CMAKE_CURRENT_LIST_DIR}/src/src/my_rtc.c
    ${CMAKE_CURRENT_LIST_DIR}/src/src/util.c
)

# Include directories
target_include_directories(sdcard INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/src/ff15/source
    ${CMAKE_CURRENT_LIST_DIR}/src/sd_driver
    ${CMAKE_CURRENT_LIST_DIR}/src/include
    ${CMAKE_CURRENT_LIST_DIR}/include
)

# Link libraries
target_link_libraries(sdcard INTERFACE
    hardware_dma
    hardware_pio
    hardware_spi
    pico_aon_timer
    pico_stdlib
    cmsis_core
)