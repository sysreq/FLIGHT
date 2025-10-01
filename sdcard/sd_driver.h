#pragma once

#include "sd_config.h"

namespace sdcard {

// ============================================
// SD CARD DRIVER (Singleton)
// ============================================
class SDDriver {
private:
    spi_t spi_config_;
    sd_spi_if_t spi_if_;
    sd_card_t sd_card_;
    bool initialized_ = false;
    
    SDDriver() {
        memset(&spi_config_, 0, sizeof(spi_config_));
        memset(&spi_if_, 0, sizeof(spi_if_));
        memset(&sd_card_, 0, sizeof(sd_card_));
    }
    
public:
    static SDDriver& instance() {
        static SDDriver instance;
        return instance;
    }
    
    SDDriver(const SDDriver&) = delete;
    SDDriver& operator=(const SDDriver&) = delete;
    SDDriver(SDDriver&&) = delete;
    SDDriver& operator=(SDDriver&&) = delete;

    bool init() {
        if (initialized_) return true;
        
        spi_config_.hw_inst = hw::SPI_BUS;
        spi_config_.miso_gpio = hw::MISO;
        spi_config_.mosi_gpio = hw::MOSI;
        spi_config_.sck_gpio = hw::SCK;
        spi_config_.baud_rate = hw::SPI_FREQ_HZ;
        
        spi_if_.spi = &spi_config_;
        spi_if_.ss_gpio = hw::CS;
        
        sd_card_.type = SD_IF_SPI;
        sd_card_.spi_if_p = &spi_if_;
        
        initialized_ = true;
        return true;
    }
    
    void shutdown() {
        if (!initialized_) return;
        
        memset(&spi_config_, 0, sizeof(spi_config_));
        memset(&spi_if_, 0, sizeof(spi_if_));
        memset(&sd_card_, 0, sizeof(sd_card_));
        
        initialized_ = false;
    }
        
    static bool Init() { return instance().init(); }
    static void Shutdown() { instance().shutdown(); }
    static bool IsReady() { return instance().initialized_; }
    static sd_card_t* GetCard() { return instance().initialized_ ? &instance().sd_card_ : nullptr; }
};

} // namespace sdcard

// ============================================
// C INTERFACE
// ============================================
#ifdef __cplusplus
extern "C" {
#endif

size_t sd_get_num();
sd_card_t* sd_get_by_num(size_t num);

#ifdef __cplusplus
}
#endif