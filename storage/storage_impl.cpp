#include "storage_config.h"
#include "storage_impl.h"

extern "C" {
    #include "sd_card.h"
}

namespace {
    static spi_t spi_config;
    static sd_spi_if_t spi_if;
    static sd_card_t sd_card;
    static bool sd_initialized = false;
}

using namespace storage::config;

static bool init_sd_card() {
    if (sd_initialized) return true;
    
    spi_config.hw_inst = SPI_BUS;
    spi_config.miso_gpio = MISO;
    spi_config.mosi_gpio = MOSI;
    spi_config.sck_gpio = SCK;
    spi_config.baud_rate = SPI_FREQ_HZ;
    
    spi_if.spi = &spi_config;
    spi_if.ss_gpio = CS;
    
    sd_card.type = SD_IF_SPI;
    sd_card.spi_if_p = &spi_if;
    
    sd_initialized = true;
    return true;
}


// extern "C" {
//     size_t sd_get_num() {
//         return sd_initialized ? 1 : 0;
//     }
    
//     sd_card_t* sd_get_by_num(size_t num) {
//         if (num != 0 || !sd_initialized) return nullptr;
//         return &sd_card;
//     }
// }

namespace storage {

bool Storage::mount() {
    if (mounted) return true;
    
    if (!init_sd_card()) return false;
    
    FRESULT res = f_mount(&fs, "", 1);
    if (res != FR_OK) return false;
    
    res = f_open(&file, FILENAME, FA_WRITE | FA_OPEN_APPEND);
    if (res == FR_NO_FILE) {
        res = f_open(&file, FILENAME, FA_WRITE | FA_CREATE_NEW);
    }
    
    mounted = (res == FR_OK);
    return mounted;
}

void Storage::unmount() {
    if (!mounted) return;
    
    flush();
    
    if (write_pending) {
        process_pending_write();
    }
    
    f_close(&file);
    f_unmount("");
    mounted = false;
}

}