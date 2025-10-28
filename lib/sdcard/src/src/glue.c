#include "hw_config.h"
#include "sd_card.h"

DSTATUS disk_status(BYTE pdrv) {
    sd_card_t *sd_card_p = sd_get_by_num(pdrv);
    if (!sd_card_p) return RES_PARERR;
    sd_card_detect(sd_card_p);
    return sd_card_p->state.m_Status;
}

DSTATUS disk_initialize(BYTE pdrv) {
    bool ok = sd_init_driver();
    if (!ok) return RES_NOTRDY;

    sd_card_t *sd_card_p = sd_get_by_num(pdrv);
    if (!sd_card_p) return RES_PARERR;
    DSTATUS ds = disk_status(pdrv);
    if (STA_NODISK & ds) 
        return ds;
    return sd_card_p->init(sd_card_p);  
}

static int sdrc2dresult(int sd_rc) {
    switch (sd_rc) {
        case SD_BLOCK_DEVICE_ERROR_NONE:
            return RES_OK;
        case SD_BLOCK_DEVICE_ERROR_UNUSABLE:
        case SD_BLOCK_DEVICE_ERROR_NO_RESPONSE:
        case SD_BLOCK_DEVICE_ERROR_NO_INIT:
        case SD_BLOCK_DEVICE_ERROR_NO_DEVICE:
            return RES_NOTRDY;
        case SD_BLOCK_DEVICE_ERROR_PARAMETER:
        case SD_BLOCK_DEVICE_ERROR_UNSUPPORTED:
            return RES_PARERR;
        case SD_BLOCK_DEVICE_ERROR_WRITE_PROTECTED:
            return RES_WRPRT;
        case SD_BLOCK_DEVICE_ERROR_CRC:
        case SD_BLOCK_DEVICE_ERROR_WOULD_BLOCK:
        case SD_BLOCK_DEVICE_ERROR_ERASE:
        case SD_BLOCK_DEVICE_ERROR_WRITE:
        default:
            return RES_ERROR;
    }
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count) {
    sd_card_t *sd_card_p = sd_get_by_num(pdrv);
    if (!sd_card_p) return RES_PARERR;
    int rc = sd_card_p->read_blocks(sd_card_p, buff, sector, count);
    return sdrc2dresult(rc);
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count) {
    sd_card_t *sd_card_p = sd_get_by_num(pdrv);
    if (!sd_card_p) return RES_PARERR;
    int rc = sd_card_p->write_blocks(sd_card_p, buff, sector, count);
    return sdrc2dresult(rc);
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff) {
    sd_card_t *sd_card_p = sd_get_by_num(pdrv);
    if (!sd_card_p) return RES_PARERR;
    switch (cmd) {
        case GET_SECTOR_COUNT: {
            static LBA_t n;
            n = sd_card_p->get_num_sectors(sd_card_p);
            *(LBA_t *)buff = n;
            if (!n) return RES_ERROR;
            return RES_OK;
        }
        case GET_BLOCK_SIZE: {
            static DWORD bs = 1;
            *(DWORD *)buff = bs;
            return RES_OK;
        }
        case CTRL_SYNC:
            sd_card_p->sync(sd_card_p);
            return RES_OK;
        default:
            return RES_PARERR;
    }
}
