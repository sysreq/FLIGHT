#include "sd_driver.h"

// These functions need to be defined in a .cpp file for C code to link against them
// The inline versions in the header are only for C++ code

extern "C" {

size_t sd_get_num() {
    return sdcard::SDDriver::IsReady() ? 1 : 0;
}

sd_card_t* sd_get_by_num(size_t num) {
    if (num != 0) return nullptr;
    return sdcard::SDDriver::GetCard();
}

} // extern "C"