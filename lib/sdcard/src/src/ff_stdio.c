#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "f_util.h"
#include "ff_stdio.h"

static BYTE posix2mode(const char *pcMode) {
    if (0 == strcmp("r", pcMode)) return FA_READ;
    if (0 == strcmp("r+", pcMode)) return FA_READ | FA_WRITE;
    if (0 == strcmp("w", pcMode)) return FA_CREATE_ALWAYS | FA_WRITE;
    if (0 == strcmp("w+", pcMode)) return FA_CREATE_ALWAYS | FA_WRITE | FA_READ;
    if (0 == strcmp("a", pcMode)) return FA_OPEN_APPEND | FA_WRITE;
    if (0 == strcmp("a+", pcMode)) return FA_OPEN_APPEND | FA_WRITE | FA_READ;
    if (0 == strcmp("wx", pcMode)) return FA_CREATE_NEW | FA_WRITE;
    if (0 == strcmp("w+x", pcMode)) return FA_CREATE_NEW | FA_WRITE | FA_READ;
    return 0;
}

int fresult2errno(FRESULT fr) {
    switch (fr) {
        case FR_OK:
            return 0;
        case FR_DISK_ERR:
            return EIO;
        case FR_INT_ERR:
            return EIO;
        case FR_NOT_READY:
            return EIO;
        case FR_NO_FILE:
            return ENOENT;
        case FR_NO_PATH:
            return ENOENT;
        case FR_INVALID_NAME:
            return ENAMETOOLONG;
        case FR_DENIED:
            return EACCES;
        case FR_EXIST:
            return EEXIST;
        case FR_INVALID_OBJECT:
            return EIO;
        case FR_WRITE_PROTECTED:
            return EACCES;
        case FR_INVALID_DRIVE:
            return ENOENT;
        case FR_NOT_ENABLED:
            return ENOENT;
        case FR_NO_FILESYSTEM:
            return ENOENT;
        case FR_MKFS_ABORTED:
            return EIO;
        case FR_TIMEOUT:
            return EIO;
        case FR_LOCKED:
            return EACCES;
        case FR_NOT_ENOUGH_CORE:
            return ENOMEM;
        case FR_TOO_MANY_OPEN_FILES:
            return ENFILE;
        case FR_INVALID_PARAMETER:
            return ENOSYS;
        default:
            return -1;
    }
}

FF_FILE *ff_fopen(const char *pcFile, const char *pcMode) {
    FIL *fp = malloc(sizeof(FIL));
    if (!fp) {
        errno = ENOMEM;
        return NULL;
    }
    FRESULT fr = f_open(fp, pcFile, posix2mode(pcMode));
    errno = fresult2errno(fr);
    if (FR_OK != fr) {
        free(fp);
        fp = 0;
    }
    return fp;
}
int ff_fclose(FF_FILE *pxStream) {
    FRESULT fr = f_close(pxStream);
    errno = fresult2errno(fr);
    free(pxStream);
    if (FR_OK == fr)
        return 0;
    else
        return -1;
}

int ff_stat(const char *pcFileName, FF_Stat_t *pxStatBuffer) {
    FILINFO filinfo;
    FRESULT fr = f_stat(pcFileName, &filinfo);
    pxStatBuffer->st_size = filinfo.fsize;
    errno = fresult2errno(fr);
    if (FR_OK == fr)
        return 0;
    else
        return -1;
}

size_t ff_fwrite(const void *pvBuffer, size_t xSize, size_t xItems, FF_FILE *pxStream) {
    UINT bw = 0;
    FRESULT fr = f_write(pxStream, pvBuffer, xSize * xItems, &bw);
    errno = fresult2errno(fr);
    return bw / xSize;
}

size_t ff_fread(void *pvBuffer, size_t xSize, size_t xItems, FF_FILE *pxStream) {
    UINT br = 0;
    FRESULT fr = f_read(pxStream, pvBuffer, xSize * xItems, &br);
    errno = fresult2errno(fr);
    return br / xSize;
}

int ff_chdir(const char *pcDirectoryName) {
    FRESULT fr = f_chdir(pcDirectoryName);
    errno = fresult2errno(fr);
    if (FR_OK == fr)
        return 0;
    else
        return -1;
}
char *ff_getcwd(char *pcBuffer, size_t xBufferLength) {
    char buf[ffconfigMAX_FILENAME] = {0};
    FRESULT fr = f_getcwd(buf, sizeof buf);
    errno = fresult2errno(fr);
    if (FR_OK == fr) {
        if ('/' != buf[0]) {
            char *p = strchr(buf, ':');
            if (p)
                ++p;
            else
                p = buf;
            int rc = snprintf(pcBuffer, xBufferLength, "%s", p);
            if (!(0 <= rc && (size_t)rc < xBufferLength))
                return NULL;
        }
        return pcBuffer;
    } else {
        return NULL;
    }
}
int ff_mkdir(const char *pcDirectoryName) {
    FRESULT fr = f_mkdir(pcDirectoryName);
    errno = fresult2errno(fr);
    if (FR_OK == fr || FR_EXIST == fr)
        return 0;
    else
        return -1;
}
int ff_fputc(int iChar, FF_FILE *pxStream) {
    UINT bw = 0;
    uint8_t buff[1];
    buff[0] = iChar;
    FRESULT fr = f_write(pxStream, buff, 1, &bw);
    errno = fresult2errno(fr);
    if (1 == bw)
        return iChar;
    else {
        return -1;
    }
}
int ff_fgetc(FF_FILE *pxStream) {
    uint8_t buff[1] = {0};
    UINT br;
    FRESULT fr = f_read(pxStream, buff, 1, &br);
    errno = fresult2errno(fr);
    if (1 == br)
        return buff[0];
    else
        return FF_EOF;
}
int ff_rmdir(const char *pcDirectory) {
    FRESULT fr = f_unlink(pcDirectory);
    errno = fresult2errno(fr);
    if (FR_OK == fr)
        return 0;
    else
        return -1;
}
int ff_remove(const char *pcPath) {
    FRESULT fr = f_unlink(pcPath);
    errno = fresult2errno(fr);
    if (FR_OK == fr)
        return 0;
    else
        return -1;
}
long ff_ftell(FF_FILE *pxStream) {
    FSIZE_t pos = f_tell(pxStream);
    return pos;
}
int ff_fseek(FF_FILE *pxStream, int iOffset, int iWhence) {
    FRESULT fr = -1;
    switch (iWhence) {
        case FF_SEEK_CUR:  // The current file position.
            if ((int)f_tell(pxStream) + iOffset < 0) return -1;
            fr = f_lseek(pxStream, f_tell(pxStream) + iOffset);
            break;
        case FF_SEEK_END:  // The end of the file.
            if ((int)f_size(pxStream) + iOffset < 0) return -1;
            fr = f_lseek(pxStream, f_size(pxStream) + iOffset);
            break;
        case FF_SEEK_SET:  // The beginning of the file.
            if (iOffset < 0) return -1;
            fr = f_lseek(pxStream, iOffset);
            break;
        default:
            assert(false);
    }
    errno = fresult2errno(fr);
    if (FR_OK == fr)
        return 0;
    else
        return -1;
}
int ff_findfirst(const char *pcDirectory, FF_FindData_t *pxFindData) {
    char buf1[ffconfigMAX_FILENAME] = {0};
    if (pcDirectory[0]) {
        FRESULT fr = f_getcwd(buf1, sizeof buf1);
        errno = fresult2errno(fr);
        if (FR_OK != fr) return -1;
        fr = f_chdir(pcDirectory);
        errno = fresult2errno(fr);
        if (FR_OK != fr) return -1;
    }
    char buf2[ffconfigMAX_FILENAME] = {0};
    FRESULT fr = f_getcwd(buf2, sizeof buf2);
    fr = f_findfirst(&pxFindData->dir, &pxFindData->fileinfo, buf2, "*");
    errno = fresult2errno(fr);
    pxFindData->pcFileName = pxFindData->fileinfo.fname;
    pxFindData->ulFileSize = pxFindData->fileinfo.fsize;
    if (pcDirectory[0]) {
        FRESULT fr2 = f_chdir(buf1);
        errno = fresult2errno(fr2);
        if (FR_OK != fr2) return -1;
    }
    if (FR_OK == fr)
        return 0;
    else
        return -1;
}
int ff_findnext(FF_FindData_t *pxFindData) {
    FRESULT fr = f_findnext(&pxFindData->dir, &pxFindData->fileinfo);
    errno = fresult2errno(fr);
    pxFindData->pcFileName = pxFindData->fileinfo.fname;
    pxFindData->ulFileSize = pxFindData->fileinfo.fsize;
    if (FR_OK == fr && pxFindData->fileinfo.fname[0]) {
        return 0;
    } else {
        return -1;
    }
}
FF_FILE *ff_truncate(const char *pcFileName, long lTruncateSize) {
    FIL *fp = malloc(sizeof(FIL));
    if (!fp) {
        errno = ENOMEM;
        return NULL;
    }
    FRESULT fr = f_open(fp, pcFileName, FA_OPEN_APPEND | FA_WRITE);
    errno = fresult2errno(fr);
    if (FR_OK != fr) return NULL;
    while (f_tell(fp) < (FSIZE_t)lTruncateSize) {
        UINT bw = 0;
        char c = 0;
        fr = f_write(fp, &c, 1, &bw);
        errno = fresult2errno(fr);
        if (1 != bw) return NULL;
    }
    fr = f_lseek(fp, lTruncateSize);
    errno = fresult2errno(fr);
    if (FR_OK != fr) return NULL;
    fr = f_truncate(fp);
    errno = fresult2errno(fr);
    if (FR_OK == fr)
        return fp;
    else
        return NULL;
}
int ff_seteof(FF_FILE *pxStream) {
    FRESULT fr = f_truncate(pxStream);
    errno = fresult2errno(fr);
    if (FR_OK == fr)
        return 0;
    else
        return FF_EOF;
}
int ff_rename(const char *pcOldName, const char *pcNewName, int bDeleteIfExists) {
    if (bDeleteIfExists) f_unlink(pcNewName);
    FRESULT fr = f_rename(pcOldName, pcNewName);
    errno = fresult2errno(fr);
    if (FR_OK == fr)
        return 0;
    else
        return -1;
}
char *ff_fgets(char *pcBuffer, size_t xCount, FF_FILE *pxStream) {
    TCHAR *p = f_gets(pcBuffer, xCount, pxStream);
    if (p == pcBuffer)
        return pcBuffer;
    else {
        errno = EIO;
        return NULL;
    }
}
