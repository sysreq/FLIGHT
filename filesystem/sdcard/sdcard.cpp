#include "../include/sdcard.h"

#include "configs.h"

#include "pico/time.h"

namespace FileSystem::SDCard {

using namespace Protocol;

// ========== Driver Implementation ==========

ErrorCode Driver::init() {
    if (initialized_) {
        return ErrorCode::NONE;
    }
    
    // Initialize SPI peripheral and GPIO
    init_spi();
    
    // Send 74+ clock cycles with CS high for card power-up
    sleep_ms(10);
    for (uint8_t i = 0; i < 10; i++) {
        spi_write(Constants::SPI_FILL);
    }
    sleep_ms(1);
    
    // ===== Step 1: Enter SPI Mode (CMD0) =====
    cs_select();
    uint8_t response = send_command(Command::CMD0_GO_IDLE_STATE, 0);
    cs_deselect();
    
    if (response != static_cast<uint8_t>(R1Response::IDLE)) {
        return ErrorCode::IO_ERROR;
    }
    
    // ===== Step 2: Check Card Version (CMD8) =====
    cs_select();
    response = send_command(Command::CMD8_SEND_IF_COND, 0x1AA);
    
    if (response == static_cast<uint8_t>(R1Response::IDLE)) {
        // V2.x card - read R7 response (32-bit)
        uint8_t r7[4];
        for (uint8_t i = 0; i < 4; i++) {
            r7[i] = spi_read();
        }
        cs_deselect();
        
        // Verify voltage range and check pattern
        if ((r7[2] & 0x0F) != 0x01 || r7[3] != 0xAA) {
            return ErrorCode::IO_ERROR;
        }
        
        // ===== Step 3: Initialize Card (ACMD41) =====
        absolute_time_t timeout = make_timeout_time_ms(SPIConfig::TIMEOUT_INIT_MS);
        
        do {
            cs_select();
            response = send_app_command(Command::ACMD41_SD_SEND_OP_COND, 0x40000000);  // HCS=1
            cs_deselect();
            
            if (response == static_cast<uint8_t>(R1Response::READY)) {
                break;
            }
            
            sleep_ms(10);
        } while (!time_reached(timeout));
        
        if (response != static_cast<uint8_t>(R1Response::READY)) {
            return ErrorCode::TIMEOUT;
        }
        
        // ===== Step 4: Read OCR to check card capacity (CMD58) =====
        cs_select();
        response = send_command(Command::CMD58_READ_OCR, 0);
        
        if (response == static_cast<uint8_t>(R1Response::READY)) {
            uint8_t ocr[4];
            for (uint8_t i = 0; i < 4; i++) {
                ocr[i] = spi_read();
            }
            
            // Check CCS bit (bit 30) to determine if SDHC/SDXC
            // If CCS=1, card uses block addressing (LBA)
            // If CCS=0, card uses byte addressing (need CMD16)
            bool is_sdhc = (ocr[0] & 0x40) != 0;
            
            if (!is_sdhc) {
                // Standard capacity card - set block length to 512
                cs_deselect();
                cs_select();
                response = send_command(Command::CMD16_SET_BLOCKLEN, 512);
                if (response != static_cast<uint8_t>(R1Response::READY)) {
                    cs_deselect();
                    return ErrorCode::IO_ERROR;
                }
            }
        }
        cs_deselect();
        
    } else {
        // V1.x card or not an SD card
        cs_deselect();
        return ErrorCode::NOT_FOUND;
    }
    
    // ===== Step 5: Switch to high-speed mode =====
    set_frequency(SPIConfig::FREQ_NORMAL_HZ);
    
    initialized_ = true;
    return ErrorCode::NONE;
}

ErrorCode Driver::deinit() {
    if (!initialized_) {
        return ErrorCode::NONE;
    }
    
    deinit_spi();
    initialized_ = false;
    
    return ErrorCode::NONE;
}

ErrorCode Driver::read_sector(uint32_t lba, uint8_t* buffer, size_t buffer_size) {
    if (!initialized_) {
        return ErrorCode::NOT_INITIALIZED;
    }
    
    if (!buffer || buffer_size < Constants::SECTOR_SIZE) {
        return ErrorCode::INVALID_PARAMETER;
    }
    
    cs_select();
    
    // Send CMD17 (READ_SINGLE_BLOCK)
    uint8_t response = send_command(Command::CMD17_READ_SINGLE_BLOCK, lba);
    
    if (response != static_cast<uint8_t>(R1Response::READY)) {
        cs_deselect();
        return ErrorCode::IO_ERROR;
    }
    
    // Wait for data start token (0xFE)
    if (!wait_token(Constants::DATA_START_TOKEN, SPIConfig::TIMEOUT_READ_MS)) {
        cs_deselect();
        return ErrorCode::TIMEOUT;
    }
    
    // Read 512 bytes of data
    spi_read_block(buffer, Constants::SECTOR_SIZE);
    
    // Read and discard 16-bit CRC
    spi_read();
    spi_read();
    
    cs_deselect();
    return ErrorCode::NONE;
}

ErrorCode Driver::write_sector(uint32_t lba, const uint8_t* buffer, size_t buffer_size) {
    if (!initialized_) {
        return ErrorCode::NOT_INITIALIZED;
    }
    
    if (!buffer || buffer_size < Constants::SECTOR_SIZE) {
        return ErrorCode::INVALID_PARAMETER;
    }
    
    cs_select();
    
    // Send CMD24 (WRITE_BLOCK)
    uint8_t response = send_command(Command::CMD24_WRITE_BLOCK, lba);
    
    if (response != static_cast<uint8_t>(R1Response::READY)) {
        cs_deselect();
        return ErrorCode::IO_ERROR;
    }
    
    // Wait for card ready
    if (!wait_ready(SPIConfig::TIMEOUT_COMMAND_MS)) {
        cs_deselect();
        return ErrorCode::TIMEOUT;
    }
    
    // Send data start token
    spi_write(Constants::DATA_START_TOKEN);
    
    // Write 512 bytes
    spi_write_block(buffer, Constants::SECTOR_SIZE);
    
    // Send dummy 16-bit CRC
    spi_write(0xFF);
    spi_write(0xFF);
    
    // Read data response token
    uint8_t data_response = spi_read();
    
    // Check if data accepted (bits 0-4 should be 00101)
    if ((data_response & Constants::DATA_ERROR_MASK) != Constants::DATA_ACCEPTED) {
        cs_deselect();
        return ErrorCode::IO_ERROR;
    }
    
    // Wait for card to finish programming (busy signal)
    if (!wait_ready(SPIConfig::TIMEOUT_WRITE_MS)) {
        cs_deselect();
        return ErrorCode::TIMEOUT;
    }
    
    cs_deselect();
    return ErrorCode::NONE;
}

bool Driver::is_initialized() {
    return initialized_;
}

} // namespace FileSystem::SDCard
