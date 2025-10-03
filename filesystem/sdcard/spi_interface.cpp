#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "pico/time.h"

#include "../include/sdcard.h"
#include "configs.h"

namespace FileSystem::SDCard::Protocol {

using namespace Constants;

// ========== Low-Level SPI Functions ==========

void init_spi() {
    // Initialize SPI peripheral at low frequency
    spi_init(SPIConfig::SPI_INSTANCE, SPIConfig::FREQ_INIT_HZ);
    
    // Configure GPIO pins for SPI function
    gpio_set_function(SPIConfig::PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(SPIConfig::PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(SPIConfig::PIN_MOSI, GPIO_FUNC_SPI);
    
    // Initialize CS pin as GPIO output
    gpio_init(SPIConfig::PIN_CS);
    gpio_set_dir(SPIConfig::PIN_CS, GPIO_OUT);
    gpio_put(SPIConfig::PIN_CS, 1);  // Deselect
}

void deinit_spi() {
    cs_deselect();
    spi_deinit(SPIConfig::SPI_INSTANCE);
}

void set_frequency(uint32_t freq_hz) {
    spi_set_baudrate(SPIConfig::SPI_INSTANCE, freq_hz);
}

void cs_select() {
    gpio_put(SPIConfig::PIN_CS, 0);
    sleep_us(1);
}

void cs_deselect() {
    sleep_us(1);
    gpio_put(SPIConfig::PIN_CS, 1);
    sleep_us(1);
}

uint8_t spi_transfer(uint8_t data) {
    uint8_t rx;
    spi_write_read_blocking(SPIConfig::SPI_INSTANCE, &data, &rx, 1);
    return rx;
}

void spi_write(uint8_t data) {
    spi_write_blocking(SPIConfig::SPI_INSTANCE, &data, 1);
}

uint8_t spi_read() {
    return spi_transfer(SPI_FILL);
}

void spi_write_block(const uint8_t* data, size_t len) {
    spi_write_blocking(SPIConfig::SPI_INSTANCE, data, len);
}

void spi_read_block(uint8_t* data, size_t len) {
    spi_read_blocking(SPIConfig::SPI_INSTANCE, SPI_FILL, data, len);
}

// ========== SD Card Protocol Functions ==========

uint8_t send_command(Command cmd, uint32_t arg) {
    // Prepare 6-byte command packet
    uint8_t packet[6] = {
        static_cast<uint8_t>(0x40 | static_cast<uint8_t>(cmd)),
        static_cast<uint8_t>(arg >> 24),
        static_cast<uint8_t>(arg >> 16),
        static_cast<uint8_t>(arg >> 8),
        static_cast<uint8_t>(arg),
        0xFF  // CRC placeholder
    };
    
    // Set correct CRC for commands that require it
    if (cmd == Command::CMD0_GO_IDLE_STATE) {
        packet[5] = 0x95;  // Pre-calculated CRC for CMD0
    } else if (cmd == Command::CMD8_SEND_IF_COND) {
        packet[5] = 0x87;  // Pre-calculated CRC for CMD8 with arg 0x1AA
    } else {
        // For other commands, calculate CRC
        packet[5] = (crc7(packet, 5) << 1) | 0x01;
    }
    
    // Send command packet
    spi_write_block(packet, 6);
    
    // Wait for R1 response (NCR = 0-8 bytes for SD cards)
    uint8_t response = 0xFF;
    for (uint8_t i = 0; i < 10; i++) {
        response = spi_read();
        if ((response & 0x80) == 0) {
            break;  // Valid response (MSB = 0)
        }
    }
    
    return response;
}

uint8_t send_app_command(Command cmd, uint32_t arg) {
    // Application commands are preceded by CMD55
    send_command(Command::CMD55_APP_CMD, 0);
    return send_command(cmd, arg);
}

bool wait_ready(uint32_t timeout_ms) {
    absolute_time_t deadline = make_timeout_time_ms(timeout_ms);
    
    while (!time_reached(deadline)) {
        if (spi_read() == SPI_FILL) {
            return true;
        }
    }
    
    return false;
}

bool wait_token(uint8_t token, uint32_t timeout_ms) {
    absolute_time_t deadline = make_timeout_time_ms(timeout_ms);
    
    while (!time_reached(deadline)) {
        uint8_t byte = spi_read();
        if (byte == token) {
            return true;
        }
        if (byte != SPI_FILL) {
            return false;  // Error token received
        }
    }
    
    return false;
}

// ========== Helper Functions ==========

uint8_t crc7(const uint8_t* data, size_t len) {
    uint8_t crc = 0;
    
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            crc = (crc & 0x80) ? ((crc << 1) ^ 0x09) : (crc << 1);
        }
    }
    
    return crc & 0x7F;
}

uint16_t crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0;
    
    for (size_t i = 0; i < len; i++) {
        crc ^= static_cast<uint16_t>(data[i]) << 8;
        for (uint8_t j = 0; j < 8; j++) {
            crc = (crc & 0x8000) ? ((crc << 1) ^ 0x1021) : (crc << 1);
        }
    }
    
    return crc;
}

} // namespace FileSystem::SDCard::Protocol
