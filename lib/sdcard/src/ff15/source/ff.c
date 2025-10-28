#include <string.h>
#include <stdarg.h>
#include <math.h>

#include "ff.h"			/* Declarations of FatFs API */

#define MAX_DIR		0x200000		/* Max size of FAT directory */
#define MAX_FAT12	0xFF5			/* Max FAT12 clusters (differs from specs, but right for real DOS/Windows behavior) */
#define MAX_FAT16	0xFFF5			/* Max FAT16 clusters (differs from specs, but right for real DOS/Windows behavior) */
#define MAX_FAT32	0x0FFFFFF5		/* Max FAT32 clusters (not specified, practical limit) */

#define IsUpper(c)		((c) >= 'A' && (c) <= 'Z')
#define IsLower(c)		((c) >= 'a' && (c) <= 'z')
#define IsDigit(c)		((c) >= '0' && (c) <= '9')
#define IsSeparator(c)	((c) == '/' || (c) == '\\')
#define IsTerminator(c)	((UINT)(c) < (FF_USE_LFN ? ' ' : '!'))
#define IsSurrogate(c)	((c) >= 0xD800 && (c) <= 0xDFFF)
#define IsSurrogateH(c)	((c) >= 0xD800 && (c) <= 0xDBFF)
#define IsSurrogateL(c)	((c) >= 0xDC00 && (c) <= 0xDFFF)

#define FA_SEEKEND	0x20	/* Seek to end of the file on file open */
#define FA_MODIFIED	0x40	/* File has been modified */
#define FA_DIRTY	0x80	/* FIL.buf[] needs to be written-back */

#define AM_VOL		0x08	/* Volume label */
#define AM_LFN		0x0F	/* LFN entry */
#define AM_MASK		0x3F	/* Mask of defined bits in FAT */

#define NSFLAG		11		/* Index of the name status byte */
#define NS_LOSS		0x01	/* Out of 8.3 format */
#define NS_LFN		0x02	/* Force to create LFN entry */
#define NS_LAST		0x04	/* Last segment */
#define NS_BODY		0x08	/* Lower case flag (body) */
#define NS_EXT		0x10	/* Lower case flag (ext) */
#define NS_DOT		0x20	/* Dot entry */
#define NS_NOLFN	0x40	/* Do not find LFN */
#define NS_NONAME	0x80	/* Not followed */

#define BS_JmpBoot			0		/* x86 jump instruction (3-byte) */
#define BPB_BytsPerSec		11		/* Sector size [byte] (WORD) */
#define BPB_SecPerClus		13		/* Cluster size [sector] (BYTE) */
#define BPB_RsvdSecCnt		14		/* Size of reserved area [sector] (WORD) */
#define BPB_NumFATs			16		/* Number of FATs (BYTE) */
#define BPB_RootEntCnt		17		/* Size of root directory area for FAT [entry] (WORD) */
#define BPB_TotSec16		19		/* Volume size (16-bit) [sector] (WORD) */
#define BPB_FATSz16			22		/* FAT size (16-bit) [sector] (WORD) */
#define BPB_TotSec32		32		/* Volume size (32-bit) [sector] (DWORD) */
#define BS_55AA				510		/* Signature word (WORD) */

#define BPB_FATSz32			36		/* FAT32: FAT size [sector] (DWORD) */
#define BPB_FSVer32			42		/* FAT32: Filesystem version (WORD) */
#define BPB_RootClus32		44		/* FAT32: Root directory cluster (DWORD) */
#define BPB_FSInfo32		48		/* FAT32: Offset of FSINFO sector (WORD) */
#define BS_FilSysType32		82		/* FAT32: Filesystem type string (8-byte) */

#define DIR_Name			0		/* Short file name (11-byte) */
#define DIR_Attr			11		/* Attribute (BYTE) */
#define DIR_NTres			12		/* Lower case flag (BYTE) */
#define DIR_CrtTime			14		/* Created time (DWORD) */
#define DIR_LstAccDate		18		/* Last accessed date (WORD) */
#define DIR_FstClusHI		20		/* Higher 16-bit of first cluster (WORD) */
#define DIR_ModTime			22		/* Modified time (DWORD) */
#define DIR_FstClusLO		26		/* Lower 16-bit of first cluster (WORD) */
#define DIR_FileSize		28		/* File size (DWORD) */
#define LDIR_Ord			0		/* LFN: LFN order and LLE flag (BYTE) */
#define LDIR_Attr			11		/* LFN: LFN attribute (BYTE) */
#define LDIR_Type			12		/* LFN: Entry type (BYTE) */
#define LDIR_Chksum			13		/* LFN: Checksum of the SFN (BYTE) */
#define LDIR_FstClusLO		26		/* LFN: MBZ field (WORD) */

#define SZDIRE				32		/* Size of a directory entry */
#define DDEM				0xE5	/* Deleted directory entry mark set to DIR_Name[0] */
#define RDDEM				0x05	/* Replacement of the character collides with DDEM */
#define LLEF				0x40	/* Last long entry flag in LDIR_Ord */

#define FSI_LeadSig			0		/* FAT32 FSI: Leading signature (DWORD) */
#define FSI_StrucSig		484		/* FAT32 FSI: Structure signature (DWORD) */
#define FSI_Free_Count		488		/* FAT32 FSI: Number of free clusters (DWORD) */
#define FSI_Nxt_Free		492		/* FAT32 FSI: Last allocated cluster (DWORD) */

#define MBR_Table			446		/* MBR: Offset of partition table in the MBR */
#define SZ_PTE				16		/* MBR: Size of a partition table entry */
#define PTE_StLba			8		/* MBR PTE: Start in LBA */

#define ABORT(fs, res)		{ fp->err = (BYTE)(res); LEAVE_FF(fs, res); }

#define LEAVE_FF(fs, res)	return res

#define SS(fs)	((UINT)FF_MAX_SS)
#define GET_FATTIME()	((DWORD)(FF_NORTC_YEAR - 1980) << 25 | (DWORD)FF_NORTC_MON << 21 | (DWORD)FF_NORTC_MDAY << 16)
#define DIR_READ_FILE(dp) dir_read(dp, 0)
#define DIR_READ_LABEL(dp) dir_read(dp, 1)

typedef struct {
	FATFS* fs;
	DWORD clu;
	DWORD ofs;
	UINT ctr;
} FILESEM;

static FATFS *FatFs[FF_VOLUMES];
static WORD Fsid;

static FILESEM Files[FF_FS_LOCK];

static const BYTE LfnOfs[] = {1,3,5,7,9,14,16,18,20,22,24,28,30};

static WCHAR LfnBuf[FF_MAX_LFN + 1];

static inline WCHAR ff_uni2oem (DWORD   uni)
{
    return (uni < 0x80) ? (WCHAR)uni : 0;
}

static inline WCHAR ff_oem2uni (WCHAR   oem)
{
    return (oem < 0x80) ? oem : 0;
}

static inline DWORD ff_wtoupper (DWORD uni)
{
    if (uni >= 'a' && uni <= 'z') {
        return uni - 32;
    }
    return uni;
}

static WORD ld_word (const BYTE* ptr)	/*	 Load a 2-byte little-endian word */
{
	WORD rv;

	rv = ptr[1];
	rv = rv << 8 | ptr[0];
	return rv;
}

static DWORD ld_dword (const BYTE* ptr)	/* Load a 4-byte little-endian word */
{
	DWORD rv;

	rv = ptr[3];
	rv = rv << 8 | ptr[2];
	rv = rv << 8 | ptr[1];
	rv = rv << 8 | ptr[0];
	return rv;
}

static void st_word (BYTE* ptr, WORD val)	/* Store a 2-byte word in little-endian */
{
	*ptr++ = (BYTE)val; val >>= 8;
	*ptr++ = (BYTE)val;
}

static void st_dword (BYTE* ptr, DWORD val)	/* Store a 4-byte word in little-endian */
{
	*ptr++ = (BYTE)val; val >>= 8;
	*ptr++ = (BYTE)val; val >>= 8;
	*ptr++ = (BYTE)val; val >>= 8;
	*ptr++ = (BYTE)val;
}

static int dbc_1st (BYTE c)
{
	if (c != 0) return 0;
	return 0;
}

static int dbc_2nd (BYTE c)
{
	if (c != 0) return 0;
	return 0;
}

static DWORD tchar2uni (const TCHAR** str)
{
	DWORD uc;
	const TCHAR *p = *str;
	BYTE b;
	WCHAR wc;

	wc = (BYTE)*p++;			/* Get a byte */
	if (dbc_1st((BYTE)wc)) {	/* Is it a DBC 1st byte? */
		b = (BYTE)*p++;			/* Get 2nd byte */
		if (!dbc_2nd(b)) return 0xFFFFFFFF;	/* Invalid code? */
		wc = (wc << 8) + b;		/* Make a DBC */
	}
	if (wc != 0) {
		wc = ff_oem2uni(wc);	/* ANSI/OEM ==> Unicode */
		if (wc == 0) return 0xFFFFFFFF;	/* Invalid code? */
	}
	uc = wc;
	*str = p;	/* Next read pointer */
	return uc;
}

static UINT put_utf (DWORD chr, TCHAR* buf, UINT szb)
{
	WCHAR wc;

	wc = ff_uni2oem(chr);
	if (wc >= 0x100) {
		if (szb < 2) return 0;
		*buf++ = (char)(wc >> 8);
		*buf++ = (TCHAR)wc;
		return 2;
	}
	if (wc == 0 || szb < 1) return 0;
	*buf++ = (TCHAR)wc;
	return 1;
}

static FRESULT chk_share (DIR* dp, int acc)
{
	UINT i, be;
	be = 0;
	for (i = 0; i < FF_FS_LOCK; i++) {
		if (Files[i].fs) {
			if (Files[i].fs == dp->obj.fs &&
				Files[i].clu == dp->obj.sclust &&
				Files[i].ofs == dp->dptr) break;
		} else {
			be = 1;
		}
	}
	if (i == FF_FS_LOCK) {
		return (!be && acc != 2) ? FR_TOO_MANY_OPEN_FILES : FR_OK;
	}

	return (acc != 0 || Files[i].ctr == 0x100) ? FR_LOCKED : FR_OK;
}

static int enq_share (void)
{
	UINT i;

	for (i = 0; i < FF_FS_LOCK && Files[i].fs; i++);
	return (i == FF_FS_LOCK) ? 0 : 1;
}


static UINT inc_share (	/* Increment object open counter and returns its index (0:Internal error) */
	DIR* dp,	/* Directory object pointing the file to register or increment */
	int acc		/* Desired access (0:Read, 1:Write, 2:Delete/Rename) */
)
{
	UINT i;


	for (i = 0; i < FF_FS_LOCK; i++) {	/* Find the object */
		if (Files[i].fs == dp->obj.fs
		 && Files[i].clu == dp->obj.sclust
		 && Files[i].ofs == dp->dptr) break;
	}

	if (i == FF_FS_LOCK) {			/* Not opened. Register it as new. */
		for (i = 0; i < FF_FS_LOCK && Files[i].fs; i++) ;	/* Find a free entry */
		if (i == FF_FS_LOCK) return 0;	/* No free entry to register (int err) */
		Files[i].fs = dp->obj.fs;
		Files[i].clu = dp->obj.sclust;
		Files[i].ofs = dp->dptr;
		Files[i].ctr = 0;
	}

	if (acc >= 1 && Files[i].ctr) return 0;

	Files[i].ctr = acc ? 0x100 : Files[i].ctr + 1;

	return i + 1;
}


static FRESULT dec_share (UINT i)
{
	UINT n;
	FRESULT res;

	if (--i < FF_FS_LOCK) {
		n = Files[i].ctr;
		if (n == 0x100) n = 0;
		if (n > 0) n--;
		Files[i].ctr = n;
		if (n == 0) {
			Files[i].fs = 0;
		}
		res = FR_OK;
	} else {
		res = FR_INT_ERR;
	}
	return res;
}


static void clear_share (FATFS* fs)
{
	UINT i;

	for (i = 0; i < FF_FS_LOCK; i++) {
		if (Files[i].fs == fs) Files[i].fs = 0;
	}
}

static FRESULT sync_window (FATFS* fs)
{
	FRESULT res = FR_OK;

	if (fs->wflag) {	/* Is the disk access window dirty? */
		if (disk_write(fs->pdrv, fs->win, fs->winsect, 1) == RES_OK) {
			fs->wflag = 0;
			if (fs->winsect - fs->fatbase < fs->fsize) {
				if (fs->n_fats == 2) disk_write(fs->pdrv, fs->win, fs->winsect + fs->fsize, 1);
			}
		} else {
			res = FR_DISK_ERR;
		}
	}
	return res;
}

static FRESULT move_window (FATFS* fs, LBA_t sect)
{
	FRESULT res = FR_OK;

	if (sect != fs->winsect) {
		res = sync_window(fs);
		if (res == FR_OK) {
			if (disk_read(fs->pdrv, fs->win, sect, 1) != RES_OK) {
				sect = (LBA_t)0 - 1;
				res = FR_DISK_ERR;
			}
			fs->winsect = sect;
		}
	}
	return res;
}

static FRESULT sync_fs (FATFS* fs)
{
	FRESULT res;
	res = sync_window(fs);
	if (res == FR_OK) {
		if (fs->fs_type == FS_FAT32 && fs->fsi_flag == 1) {
			memset(fs->win, 0, sizeof fs->win);
			st_word(fs->win + BS_55AA, 0xAA55);
			st_dword(fs->win + FSI_LeadSig, 0x41615252);
			st_dword(fs->win + FSI_StrucSig, 0x61417272);
			st_dword(fs->win + FSI_Free_Count, fs->free_clst);
			st_dword(fs->win + FSI_Nxt_Free, fs->last_clst);
			fs->winsect = fs->volbase + 1;
			disk_write(fs->pdrv, fs->win, fs->winsect, 1);
			fs->fsi_flag = 0;
		}
		if (disk_ioctl(fs->pdrv, CTRL_SYNC, 0) != RES_OK) res = FR_DISK_ERR;
	}

	return res;
}

static LBA_t clst2sect (FATFS* fs, DWORD clst) {
	clst -= 2;
	if (clst >= fs->n_fatent - 2) return 0;
	return fs->database + (LBA_t)fs->csize * clst;
}

static DWORD get_fat (FFOBJID* obj,	DWORD clst) {
	UINT wc, bc;
	DWORD val;
	FATFS *fs = obj->fs;


	if (clst < 2 || clst >= fs->n_fatent) {
		val = 1;
	} else {
		val = 0xFFFFFFFF;

		switch (fs->fs_type) {
		case FS_FAT12 :
			bc = (UINT)clst; bc += bc / 2;
			if (move_window(fs, fs->fatbase + (bc / SS(fs))) != FR_OK) break;
			wc = fs->win[bc++ % SS(fs)];		/* Get 1st byte of the entry */
			if (move_window(fs, fs->fatbase + (bc / SS(fs))) != FR_OK) break;
			wc |= fs->win[bc % SS(fs)] << 8;	/* Merge 2nd byte of the entry */
			val = (clst & 1) ? (wc >> 4) : (wc & 0xFFF);	/* Adjust bit position */
			break;
		case FS_FAT16 :
			if (move_window(fs, fs->fatbase + (clst / (SS(fs) / 2))) != FR_OK) break;
			val = ld_word(fs->win + clst * 2 % SS(fs));		/* Simple WORD array */
			break;
		case FS_FAT32 :
			if (move_window(fs, fs->fatbase + (clst / (SS(fs) / 4))) != FR_OK) break;
			val = ld_dword(fs->win + clst * 4 % SS(fs)) & 0x0FFFFFFF;	/* Simple DWORD array but mask out upper 4 bits */
			break;
		default:
			val = 1;
		}
	}

	return val;
}

static FRESULT put_fat (FATFS* fs, DWORD clst,	DWORD val) {
	UINT bc;
	BYTE *p;
	FRESULT res = FR_INT_ERR;

	if (clst >= 2 && clst < fs->n_fatent) {
		switch (fs->fs_type) {
		case FS_FAT12:
			bc = (UINT)clst; bc += bc / 2;
			res = move_window(fs, fs->fatbase + (bc / SS(fs)));
			if (res != FR_OK) break;
			p = fs->win + bc++ % SS(fs);
			*p = (clst & 1) ? ((*p & 0x0F) | ((BYTE)val << 4)) : (BYTE)val;
			fs->wflag = 1;
			res = move_window(fs, fs->fatbase + (bc / SS(fs)));
			if (res != FR_OK) break;
			p = fs->win + bc % SS(fs);
			*p = (clst & 1) ? (BYTE)(val >> 4) : ((*p & 0xF0) | ((BYTE)(val >> 8) & 0x0F));
			fs->wflag = 1;
			break;
		case FS_FAT16:
			res = move_window(fs, fs->fatbase + (clst / (SS(fs) / 2)));
			if (res != FR_OK) break;
			st_word(fs->win + clst * 2 % SS(fs), (WORD)val);	/* Simple WORD array */
			fs->wflag = 1;
			break;
		case FS_FAT32:
			res = move_window(fs, fs->fatbase + (clst / (SS(fs) / 4)));
			if (res != FR_OK) break;
			if (!FF_FS_EXFAT || fs->fs_type != FS_EXFAT) {
				val = (val & 0x0FFFFFFF) | (ld_dword(fs->win + clst * 4 % SS(fs)) & 0xF0000000);
			}
			st_dword(fs->win + clst * 4 % SS(fs), val);
			fs->wflag = 1;
			break;
		}
	}
	return res;
}

static FRESULT remove_chain (FFOBJID* obj, DWORD clst, DWORD pclst) {
	FRESULT res = FR_OK;
	DWORD nxt;
	FATFS *fs = obj->fs;

	if (clst < 2 || clst >= fs->n_fatent) return FR_INT_ERR;
	if (pclst != 0 && (!FF_FS_EXFAT || fs->fs_type != FS_EXFAT || obj->stat != 2)) {
		res = put_fat(fs, pclst, 0xFFFFFFFF);
		if (res != FR_OK) return res;
	}

	do {
		nxt = get_fat(obj, clst);
		if (nxt == 0) break;
		if (nxt == 1) return FR_INT_ERR;
		if (nxt == 0xFFFFFFFF) return FR_DISK_ERR;
		res = put_fat(fs, clst, 0);
		if (res != FR_OK) return res;
		if (fs->free_clst < fs->n_fatent - 2) {
			fs->free_clst++;
			fs->fsi_flag |= 1;
		}
		clst = nxt;
	} while (clst < fs->n_fatent);
	return FR_OK;
}

static DWORD create_chain (FFOBJID* obj, DWORD clst)
{
	DWORD cs, ncl, scl;
	FRESULT res;
	FATFS *fs = obj->fs;

	if (clst == 0) {
		scl = fs->last_clst;
		if (scl == 0 || scl >= fs->n_fatent) scl = 1;
	} else {
		cs = get_fat(obj, clst);
		if (cs < 2) return 1;
		if (cs == 0xFFFFFFFF) return cs;
		if (cs < fs->n_fatent) return cs;
		scl = clst;
	}
	if (fs->free_clst == 0) return 0;
	ncl = 0;
	if (scl == clst) {
		ncl = scl + 1;
		if (ncl >= fs->n_fatent) ncl = 2;
		cs = get_fat(obj, ncl);
		if (cs == 1 || cs == 0xFFFFFFFF) return cs;
		if (cs != 0) {
			cs = fs->last_clst;
			if (cs >= 2 && cs < fs->n_fatent) scl = cs;
			ncl = 0;
		}
	}
	if (ncl == 0) {
		ncl = scl;
		for (;;) {
			ncl++;
			if (ncl >= fs->n_fatent) {
				ncl = 2;
				if (ncl > scl) return 0;
			}
			cs = get_fat(obj, ncl);
			if (cs == 0) break;
			if (cs == 1 || cs == 0xFFFFFFFF) return cs;
			if (ncl == scl) return 0;
		}
	}
	res = put_fat(fs, ncl, 0xFFFFFFFF);
	if (res == FR_OK && clst != 0) {
		res = put_fat(fs, clst, ncl);
	}

	if (res == FR_OK) {
		fs->last_clst = ncl;
		if (fs->free_clst <= fs->n_fatent - 2) fs->free_clst--;
		fs->fsi_flag |= 1;
	} else {
		ncl = (res == FR_DISK_ERR) ? 0xFFFFFFFF : 1;
	}

	return ncl;
}

static DWORD clmt_clust (FIL* fp, FSIZE_t ofs) {
	DWORD cl, ncl;
	DWORD *tbl;
	FATFS *fs = fp->obj.fs;

	tbl = fp->cltbl + 1;
	cl = (DWORD)(ofs / SS(fs) / fs->csize);
	for (;;) {
		ncl = *tbl++;
		if (ncl == 0) return 0;
		if (cl < ncl) break;
		cl -= ncl; tbl++;
	}
	return cl + *tbl;
}

static FRESULT dir_clear (FATFS *fs, DWORD clst) {
	LBA_t sect;
	UINT n, szb;
	BYTE *ibuf;


	if (sync_window(fs) != FR_OK) return FR_DISK_ERR;
	sect = clst2sect(fs, clst);
	fs->winsect = sect;
	memset(fs->win, 0, sizeof fs->win);
	ibuf = fs->win; szb = 1;
	for (n = 0; n < fs->csize && disk_write(fs->pdrv, ibuf, sect + n, szb) == RES_OK; n += szb) ;
	return (n == fs->csize) ? FR_OK : FR_DISK_ERR;
}

static FRESULT dir_sdi (DIR* dp, DWORD ofs)
{
	DWORD csz, clst;
	FATFS *fs = dp->obj.fs;


	if (ofs >= (DWORD)( MAX_DIR) || ofs % SZDIRE) {	/* Check range of offset and alignment */
		return FR_INT_ERR;
	}
	dp->dptr = ofs;				/* Set current offset */
	clst = dp->obj.sclust;		/* Table start cluster (0:root) */
	if (clst == 0 && fs->fs_type >= FS_FAT32) {	/* Replace cluster# 0 with root cluster# */
		clst = (DWORD)fs->dirbase;
		if (FF_FS_EXFAT) dp->obj.stat = 0;	/* exFAT: Root dir has an FAT chain */
	}

	if (clst == 0) {	/* Static table (root-directory on the FAT volume) */
		if (ofs / SZDIRE >= fs->n_rootdir) return FR_INT_ERR;	/* Is index out of range? */
		dp->sect = fs->dirbase;

	} else {			/* Dynamic table (sub-directory or root-directory on the FAT32/exFAT volume) */
		csz = (DWORD)fs->csize * SS(fs);	/* Bytes per cluster */
		while (ofs >= csz) {				/* Follow cluster chain */
			clst = get_fat(&dp->obj, clst);				/* Get next cluster */
			if (clst == 0xFFFFFFFF) return FR_DISK_ERR;	/* Disk error */
			if (clst < 2 || clst >= fs->n_fatent) return FR_INT_ERR;	/* Reached to end of table or internal error */
			ofs -= csz;
		}
		dp->sect = clst2sect(fs, clst);
	}
	dp->clust = clst;					/* Current cluster# */
	if (dp->sect == 0) return FR_INT_ERR;
	dp->sect += ofs / SS(fs);			/* Sector# of the directory entry */
	dp->dir = fs->win + (ofs % SS(fs));	/* Pointer to the entry in the win[] */

	return FR_OK;
}

static FRESULT dir_next (DIR* dp, int stretch)
{
	DWORD ofs, clst;
	FATFS *fs = dp->obj.fs;


	ofs = dp->dptr + SZDIRE;	/* Next entry */
	if (ofs >= (DWORD)(MAX_DIR)) dp->sect = 0;	/* Disable it if the offset reached the max value */
	if (dp->sect == 0) return FR_NO_FILE;	/* Report EOT if it has been disabled */

	if (ofs % SS(fs) == 0) {	/* Sector changed? */
		dp->sect++;				/* Next sector */

		if (dp->clust == 0) {	/* Static table */
			if (ofs / SZDIRE >= fs->n_rootdir) {	/* Report EOT if it reached end of static table */
				dp->sect = 0; return FR_NO_FILE;
			}
		}
		else {					/* Dynamic table */
			if ((ofs / SS(fs) & (fs->csize - 1)) == 0) {	/* Cluster changed? */
				clst = get_fat(&dp->obj, dp->clust);		/* Get next cluster */
				if (clst <= 1) return FR_INT_ERR;			/* Internal error */
				if (clst == 0xFFFFFFFF) return FR_DISK_ERR;	/* Disk error */
				if (clst >= fs->n_fatent) {					/* It reached end of dynamic table */
					if (!stretch) {								/* If no stretch, report EOT */
						dp->sect = 0; return FR_NO_FILE;
					}
					clst = create_chain(&dp->obj, dp->clust);	/* Allocate a cluster */
					if (clst == 0) return FR_DENIED;			/* No free cluster */
					if (clst == 1) return FR_INT_ERR;			/* Internal error */
					if (clst == 0xFFFFFFFF) return FR_DISK_ERR;	/* Disk error */
					if (dir_clear(fs, clst) != FR_OK) return FR_DISK_ERR;	/* Clean up the stretched table */
					if (FF_FS_EXFAT) dp->obj.stat |= 4;			/* exFAT: The directory has been stretched */
				}
				dp->clust = clst;		/* Initialize data for new cluster */
				dp->sect = clst2sect(fs, clst);
			}
		}
	}
	dp->dptr = ofs;						/* Current entry */
	dp->dir = fs->win + ofs % SS(fs);	/* Pointer to the entry in the win[] */

	return FR_OK;
}

static FRESULT dir_alloc (	/* FR_OK(0):succeeded, !=0:error */
	DIR* dp,				/* Pointer to the directory object */
	UINT n_ent				/* Number of contiguous entries to allocate */
)
{
	FRESULT res;
	UINT n;
	FATFS *fs = dp->obj.fs;


	res = dir_sdi(dp, 0);
	if (res == FR_OK) {
		n = 0;
		do {
			res = move_window(fs, dp->sect);
			if (res != FR_OK) break;
			if (dp->dir[DIR_Name] == DDEM || dp->dir[DIR_Name] == 0) {	/* Is the entry free? */
				if (++n == n_ent) break;	/* Is a block of contiguous free entries found? */
			} else {
				n = 0;				/* Not a free entry, restart to search */
			}
			res = dir_next(dp, 1);	/* Next entry with table stretch enabled */
		} while (res == FR_OK);
	}

	if (res == FR_NO_FILE) res = FR_DENIED;	/* No directory entry to allocate */
	return res;
}

static DWORD ld_clust (	/* Returns the top cluster value of the SFN entry */
	FATFS* fs,			/* Pointer to the fs object */
	const BYTE* dir		/* Pointer to the key entry */
)
{
	DWORD cl;

	cl = ld_word(dir + DIR_FstClusLO);
	if (fs->fs_type == FS_FAT32) {
		cl |= (DWORD)ld_word(dir + DIR_FstClusHI) << 16;
	}

	return cl;
}

static void st_clust (
	FATFS* fs,	/* Pointer to the fs object */
	BYTE* dir,	/* Pointer to the key entry */
	DWORD cl	/* Value to be set */
)
{
	st_word(dir + DIR_FstClusLO, (WORD)cl);
	if (fs->fs_type == FS_FAT32) {
		st_word(dir + DIR_FstClusHI, (WORD)(cl >> 16));
	}
}

static int cmp_lfn (		/* 1:matched, 0:not matched */
	const WCHAR* lfnbuf,	/* Pointer to the LFN working buffer to be compared */
	BYTE* dir				/* Pointer to the directory entry containing the part of LFN */
)
{
	UINT i, s;
	WCHAR wc, uc;


	if (ld_word(dir + LDIR_FstClusLO) != 0) return 0;	/* Check LDIR_FstClusLO */

	i = ((dir[LDIR_Ord] & 0x3F) - 1) * 13;	/* Offset in the LFN buffer */

	for (wc = 1, s = 0; s < 13; s++) {		/* Process all characters in the entry */
		uc = ld_word(dir + LfnOfs[s]);		/* Pick an LFN character */
		if (wc != 0) {
			if (i >= FF_MAX_LFN + 1 || ff_wtoupper(uc) != ff_wtoupper(lfnbuf[i++])) {	/* Compare it */
				return 0;					/* Not matched */
			}
			wc = uc;
		} else {
			if (uc != 0xFFFF) return 0;		/* Check filler */
		}
	}

	if ((dir[LDIR_Ord] & LLEF) && wc && lfnbuf[i]) return 0;	/* Last segment matched but different length */

	return 1;		/* The part of LFN matched */
}

static int pick_lfn (WCHAR* lfnbuf, BYTE* dir) {
	UINT i, s;
	WCHAR wc, uc;

	if (ld_word(dir + LDIR_FstClusLO) != 0) return 0;	/* Check LDIR_FstClusLO is 0 */
	i = ((dir[LDIR_Ord] & ~LLEF) - 1) * 13;	/* Offset in the LFN buffer */
	for (wc = 1, s = 0; s < 13; s++) {		/* Process all characters in the entry */
		uc = ld_word(dir + LfnOfs[s]);		/* Pick an LFN character */
		if (wc != 0) {
			if (i >= FF_MAX_LFN + 1) return 0;	/* Buffer overflow? */
			lfnbuf[i++] = wc = uc;			/* Store it */
		} else {
			if (uc != 0xFFFF) return 0;		/* Check filler */
		}
	}

	if (dir[LDIR_Ord] & LLEF && wc != 0) {	/* Put terminator if it is the last LFN part and not terminated */
		if (i >= FF_MAX_LFN + 1) return 0;	/* Buffer overflow? */
		lfnbuf[i] = 0;
	}

	return 1;		/* The part of LFN is valid */
}

static void put_lfn (const WCHAR* lfn, BYTE* dir, BYTE ord, BYTE sum) {
	UINT i, s;
	WCHAR wc;

	dir[LDIR_Chksum] = sum;			/* Set checksum */
	dir[LDIR_Attr] = AM_LFN;		/* Set attribute. LFN entry */
	dir[LDIR_Type] = 0;
	st_word(dir + LDIR_FstClusLO, 0);

	i = (ord - 1) * 13;				/* Get offset in the LFN working buffer */
	s = wc = 0;
	do {
		if (wc != 0xFFFF) wc = lfn[i++];	/* Get an effective character */
		st_word(dir + LfnOfs[s], wc);		/* Put it */
		if (wc == 0) wc = 0xFFFF;			/* Padding characters for following items */
	} while (++s < 13);
	if (wc == 0xFFFF || !lfn[i]) ord |= LLEF;	/* Last LFN part is the start of LFN sequence */
	dir[LDIR_Ord] = ord;
}

static void gen_numname (BYTE* dst, const BYTE* src, const WCHAR* lfn, UINT seq) {
	BYTE ns[8], c;
	UINT i, j;
	WCHAR wc;
	DWORD sreg;

	memcpy(dst, src, 11);

	if (seq > 5) {
		sreg = seq;
		while (*lfn) {
			wc = *lfn++;
			for (i = 0; i < 16; i++) {
				sreg = (sreg << 1) + (wc & 1);
				wc >>= 1;
				if (sreg & 0x10000) sreg ^= 0x11021;
			}
		}
		seq = (UINT)sreg;
	}

	i = 7;
	do {
		c = (BYTE)((seq % 16) + '0'); seq /= 16;
		if (c > '9') c += 7;
		ns[i--] = c;
	} while (i && seq);
	ns[i] = '~';

	for (j = 0; j < i && dst[j] != ' '; j++) {	/* Find the offset to append */
		if (dbc_1st(dst[j])) {	/* To avoid DBC break up */
			if (j == i - 1) break;
			j++;
		}
	}
	do {
		dst[j++] = (i < 8) ? ns[i++] : ' ';
	} while (j < 8);
}

static BYTE sum_sfn (const BYTE* dir) {
	BYTE sum = 0;
	UINT n = 11;

	do {
		sum = (sum >> 1) + (sum << 7) + *dir++;
	} while (--n);
	return sum;
}

static FRESULT dir_read (DIR* dp, int vol) {
	FRESULT res = FR_NO_FILE;
	FATFS *fs = dp->obj.fs;
	BYTE attr, b;
	BYTE ord = 0xFF, sum = 0xFF;

	while (dp->sect) {
		res = move_window(fs, dp->sect);
		if (res != FR_OK) break;
		b = dp->dir[DIR_Name];
		if (b == 0) {
			res = FR_NO_FILE; break;
		}
		dp->obj.attr = attr = dp->dir[DIR_Attr] & AM_MASK;
		if (b == DDEM || b == '.' || (int)((attr & ~AM_ARC) == AM_VOL) != vol) {
			ord = 0xFF;
		} else {
			if (attr == AM_LFN) {
				if (b & LLEF) {
					sum = dp->dir[LDIR_Chksum];
					b &= (BYTE)~LLEF; ord = b;
					dp->blk_ofs = dp->dptr;
				}
				ord = (b == ord && sum == dp->dir[LDIR_Chksum] && pick_lfn(fs->lfnbuf, dp->dir)) ? ord - 1 : 0xFF;
			} else {
				if (ord != 0 || sum != sum_sfn(dp->dir)) {
					dp->blk_ofs = 0xFFFFFFFF;
				}
				break;
			}
		}
		res = dir_next(dp, 0);
		if (res != FR_OK) break;
	}

	if (res != FR_OK) dp->sect = 0;
	return res;
}

static FRESULT dir_find (DIR* dp) {
	FRESULT res;
	FATFS *fs = dp->obj.fs;
	BYTE c;
	BYTE a, ord, sum;

	res = dir_sdi(dp, 0);			/* Rewind directory object */
	if (res != FR_OK) return res;
	/* On the FAT/FAT32 volume */
	ord = sum = 0xFF; dp->blk_ofs = 0xFFFFFFFF;	/* Reset LFN sequence */
	do {
		res = move_window(fs, dp->sect);
		if (res != FR_OK) break;
		c = dp->dir[DIR_Name];
		if (c == 0) { res = FR_NO_FILE; break; }	/* Reached to end of table */
		dp->obj.attr = a = dp->dir[DIR_Attr] & AM_MASK;
		if (c == DDEM || ((a & AM_VOL) && a != AM_LFN)) {	/* An entry without valid data */
			ord = 0xFF; dp->blk_ofs = 0xFFFFFFFF;	/* Reset LFN sequence */
		} else {
			if (a == AM_LFN) {			/* An LFN entry is found */
				if (!(dp->fn[NSFLAG] & NS_NOLFN)) {
					if (c & LLEF) {		/* Is it start of LFN sequence? */
						sum = dp->dir[LDIR_Chksum];
						c &= (BYTE)~LLEF; ord = c;	/* LFN start order */
						dp->blk_ofs = dp->dptr;	/* Start offset of LFN */
					}
					/* Check validity of the LFN entry and compare it with given name */
					ord = (c == ord && sum == dp->dir[LDIR_Chksum] && cmp_lfn(fs->lfnbuf, dp->dir)) ? ord - 1 : 0xFF;
				}
			} else {					/* An SFN entry is found */
				if (ord == 0 && sum == sum_sfn(dp->dir)) break;	/* LFN matched? */
				if (!(dp->fn[NSFLAG] & NS_LOSS) && !memcmp(dp->dir, dp->fn, 11)) break;	/* SFN matched? */
				ord = 0xFF; dp->blk_ofs = 0xFFFFFFFF;	/* Reset LFN sequence */
			}
		}
		res = dir_next(dp, 0);	/* Next entry */
	} while (res == FR_OK);

	return res;
}

static FRESULT dir_register (DIR* dp) {
	FRESULT res;
	FATFS *fs = dp->obj.fs;
	UINT n, len, n_ent;
	BYTE sn[12], sum;


	if (dp->fn[NSFLAG] & (NS_DOT | NS_NONAME)) return FR_INVALID_NAME;
	for (len = 0; fs->lfnbuf[len]; len++) ;

	memcpy(sn, dp->fn, 12);
	if (sn[NSFLAG] & NS_LOSS) {	
		dp->fn[NSFLAG] = NS_NOLFN;
		for (n = 1; n < 100; n++) {
			gen_numname(dp->fn, sn, fs->lfnbuf, n);
			res = dir_find(dp);
			if (res != FR_OK) break;
		}
		if (n == 100) return FR_DENIED;
		if (res != FR_NO_FILE) return res;
		dp->fn[NSFLAG] = sn[NSFLAG];
	}

	n_ent = (sn[NSFLAG] & NS_LFN) ? (len + 12) / 13 + 1 : 1;
	res = dir_alloc(dp, n_ent);	
	if (res == FR_OK && --n_ent) {
		res = dir_sdi(dp, dp->dptr - n_ent * SZDIRE);
		if (res == FR_OK) {
			sum = sum_sfn(dp->fn);
			do {
				res = move_window(fs, dp->sect);
				if (res != FR_OK) break;
				put_lfn(fs->lfnbuf, dp->dir, (BYTE)n_ent, sum);
				fs->wflag = 1;
				res = dir_next(dp, 0);	/* Next entry */
			} while (res == FR_OK && --n_ent);
		}
	}

	/* Set SFN entry */
	if (res == FR_OK) {
		res = move_window(fs, dp->sect);
		if (res == FR_OK) {
			memset(dp->dir, 0, SZDIRE);	/* Clean the entry */
			memcpy(dp->dir + DIR_Name, dp->fn, 11);	/* Put SFN */
			dp->dir[DIR_NTres] = dp->fn[NSFLAG] & (NS_BODY | NS_EXT);	/* Put NT flag */
			fs->wflag = 1;
		}
	}

	return res;
}

static FRESULT dir_remove (DIR* dp)
{
	FRESULT res;
	FATFS *fs = dp->obj.fs;
	DWORD last = dp->dptr;

	res = (dp->blk_ofs == 0xFFFFFFFF) ? FR_OK : dir_sdi(dp, dp->blk_ofs);	/* Goto top of the entry block if LFN is exist */
	if (res == FR_OK) {
		do {
			res = move_window(fs, dp->sect);
			if (res != FR_OK) break;
			dp->dir[DIR_Name] = DDEM;
			fs->wflag = 1;
			if (dp->dptr >= last) break;
			res = dir_next(dp, 0);
		} while (res == FR_OK);
		if (res == FR_NO_FILE) res = FR_INT_ERR;
	}

	return res;
}

static void get_fileinfo (DIR* dp, FILINFO* fno)
{
	UINT si, di;
	BYTE lcf;
	WCHAR wc, hs;
	FATFS *fs = dp->obj.fs;
	UINT nw;

	fno->fname[0] = 0;
	if (dp->sect == 0) return;
	if (dp->blk_ofs != 0xFFFFFFFF) {	/* Get LFN if available */
		si = di = 0;
		hs = 0;
		while (fs->lfnbuf[si] != 0) {
			wc = fs->lfnbuf[si++];		/* Get an LFN character (UTF-16) */
			if (hs == 0 && IsSurrogate(wc)) {	/* Is it a surrogate? */
				hs = wc; continue;		/* Get low surrogate */
			}
			nw = put_utf((DWORD)hs << 16 | wc, &fno->fname[di], FF_LFN_BUF - di);	/* Store it in API encoding */
			if (nw == 0) {				/* Buffer overflow or wrong char? */
				di = 0; break;
			}
			di += nw;
			hs = 0;
		}
		if (hs != 0) di = 0;	/* Broken surrogate pair? */
		fno->fname[di] = 0;		/* Terminate the LFN (null string means LFN is invalid) */
	}

	si = di = 0;
	while (si < 11) {		/* Get SFN from SFN entry */
		wc = dp->dir[si++];			/* Get a char */
		if (wc == ' ') continue;	/* Skip padding spaces */
		if (wc == RDDEM) wc = DDEM;	/* Restore replaced DDEM character */
		if (si == 9 && di < FF_SFN_BUF) fno->altname[di++] = '.';	/* Insert a . if extension is exist */
		fno->altname[di++] = (TCHAR)wc;	/* Store it without any conversion */
	}
	fno->altname[di] = 0;	/* Terminate the SFN  (null string means SFN is invalid) */

	if (fno->fname[0] == 0) {	/* If LFN is invalid, altname[] needs to be copied to fname[] */
		if (di == 0) {	/* If LFN and SFN both are invalid, this object is inaccessible */
			fno->fname[di++] = '\?';
		} else {
			for (si = di = 0, lcf = NS_BODY; fno->altname[si]; si++, di++) {	/* Copy altname[] to fname[] with case information */
				wc = (WCHAR)fno->altname[si];
				if (wc == '.') lcf = NS_EXT;
				if (IsUpper(wc) && (dp->dir[DIR_NTres] & lcf)) wc += 0x20;
				fno->fname[di] = (TCHAR)wc;
			}
		}
		fno->fname[di] = 0;	/* Terminate the LFN */
		if (!dp->dir[DIR_NTres]) fno->altname[0] = 0;	/* Altname is not needed if neither LFN nor case info is exist. */
	}

	fno->fattrib = dp->dir[DIR_Attr] & AM_MASK;			/* Attribute */
	fno->fsize = ld_dword(dp->dir + DIR_FileSize);		/* Size */
	fno->ftime = ld_word(dp->dir + DIR_ModTime + 0);	/* Time */
	fno->fdate = ld_word(dp->dir + DIR_ModTime + 2);	/* Date */
}

#define FIND_RECURS	4

static DWORD get_achar (const TCHAR** ptr)
{
	DWORD chr;
	chr = (BYTE)*(*ptr)++;				/* Get a byte */
	if (IsLower(chr)) chr -= 0x20;		/* To upper ASCII char */
	if (chr >= 0x80) chr = 0;
	return chr;
}


static int pattern_match (const TCHAR* pat, const TCHAR* nam, UINT skip, UINT recur) {
	const TCHAR *pptr;
	const TCHAR *nptr;
	DWORD pchr, nchr;
	UINT sk;


	while ((skip & 0xFF) != 0) {		/* Pre-skip name chars */
		if (!get_achar(&nam)) return 0;	/* Branch mismatched if less name chars */
		skip--;
	}
	if (*pat == 0 && skip) return 1;	/* Matched? (short circuit) */

	do {
		pptr = pat; nptr = nam;			/* Top of pattern and name to match */
		for (;;) {
			if (*pptr == '\?' || *pptr == '*') {	/* Wildcard term? */
				if (recur == 0) return 0;	/* Too many wildcard terms? */
				sk = 0;
				do {	/* Analyze the wildcard term */
					if (*pptr++ == '\?') {
						sk++;
					} else {
						sk |= 0x100;
					}
				} while (*pptr == '\?' || *pptr == '*');
				if (pattern_match(pptr, nptr, sk, recur - 1)) return 1;	/* Test new branch (recursive call) */
				nchr = *nptr; break;	/* Branch mismatched */
			}
			pchr = get_achar(&pptr);	/* Get a pattern char */
			nchr = get_achar(&nptr);	/* Get a name char */
			if (pchr != nchr) break;	/* Branch mismatched? */
			if (pchr == 0) return 1;	/* Branch matched? (matched at end of both strings) */
		}
		get_achar(&nam);			/* nam++ */
	} while (skip && nchr);		/* Retry until end of name if infinite search is specified */

	return 0;
}

static WCHAR sanitize_for_sfn(WCHAR wc)
{
    if (wc >= 0x80) {
        wc = ff_uni2oem(wc);
        if (wc == 0 || wc >= 0x80) return '_';
    }
    
    if (wc < ' ' || strchr("+,;=[]\"*:<>|\?\x7F", (int)wc)) return '_';
    if (wc >= 'a' && wc <= 'z') wc -= 0x20;
    
    return wc;
}

static FRESULT create_name(DIR* dp, const TCHAR** path)
{
    const TCHAR* p = *path;
    WCHAR* lfn = dp->obj.fs->lfnbuf;
    UINT di = 0;
    DWORD uc;
    WCHAR wc;
    BYTE cf = 0;

    for (;;) {
        uc = tchar2uni(&p);
        if (uc == 0xFFFFFFFF) return FR_INVALID_NAME;
        
        if (uc >= 0x10000) lfn[di++] = (WCHAR)(uc >> 16);
        wc = (WCHAR)uc;
        
        if (wc < ' ' || IsSeparator(wc)) break;
        if (wc < 0x80 && strchr("*:<>|\"\?\x7F", (int)wc)) return FR_INVALID_NAME;
        if (di >= FF_MAX_LFN) return FR_INVALID_NAME;
        
        lfn[di++] = wc;
    }
    
    if (wc < ' ') {
        cf = NS_LAST;
    } else {
        while (IsSeparator(*p)) p++;
        cf = IsTerminator(*p) ? NS_LAST : 0;
    }
    *path = p;
    
    while (di > 0 && (lfn[di-1] == ' ' || lfn[di-1] == '.')) di--;
    
    lfn[di] = 0;
    if (di == 0) return FR_INVALID_NAME;
    
    memset(dp->fn, ' ', 11);
    
    UINT si = 0, i = 0;
    
    while (i < 6 && si < di && lfn[si] != '.') {
        wc = sanitize_for_sfn(lfn[si++]);
        if (wc != ' ' && wc != '.') dp->fn[i++] = (BYTE)wc;
    }
    
    dp->fn[i++] = '~';
    dp->fn[i++] = '1';

    UINT ext_start = di;
    while (ext_start > 0 && lfn[ext_start-1] != '.') ext_start--;

    if (ext_start > 0 && ext_start < di) {
        i = 8;
        si = ext_start;
        while (i < 11 && si < di) {
            wc = sanitize_for_sfn(lfn[si++]);
            if (wc != ' ' && wc != '.') dp->fn[i++] = (BYTE)wc;
        }
    }
    
    if (dp->fn[0] == DDEM) dp->fn[0] = RDDEM;

    dp->fn[NSFLAG] = cf | NS_LFN;
    
    return FR_OK;
}

static FRESULT follow_path (DIR* dp, const TCHAR* path)
{
	FRESULT res;
	BYTE ns;
	FATFS *fs = dp->obj.fs;

	{										/* With heading separator */
		while (IsSeparator(*path)) path++;	/* Strip separators */
		dp->obj.sclust = 0;					/* Start from the root directory */
	}

	if ((UINT)*path < ' ') {				/* Null path name is the origin directory itself */
		dp->fn[NSFLAG] = NS_NONAME;
		res = dir_sdi(dp, 0);

	} else {								/* Follow path */
		for (;;) {
			res = create_name(dp, &path);	/* Get a segment name of the path */
			if (res != FR_OK) break;
			res = dir_find(dp);				/* Find an object with the segment name */
			ns = dp->fn[NSFLAG];
			if (res != FR_OK) {				/* Failed to find the object */
				if (res == FR_NO_FILE) {	/* Object is not found */
					if (FF_FS_RPATH && (ns & NS_DOT)) {	/* If dot entry is not exist, stay there */
						if (!(ns & NS_LAST)) continue;	/* Continue to follow if not last segment */
						dp->fn[NSFLAG] = NS_NONAME;
						res = FR_OK;
					} else {							/* Could not find the object */
						if (!(ns & NS_LAST)) res = FR_NO_PATH;	/* Adjust error code if not last segment */
					}
				}
				break;
			}
			if (ns & NS_LAST) break;		/* Last segment matched. Function completed. */
			/* Get into the sub-directory */
			if (!(dp->obj.attr & AM_DIR)) {	/* It is not a sub-directory and cannot follow */
				res = FR_NO_PATH; break;
			}
			{
				dp->obj.sclust = ld_clust(fs, fs->win + dp->dptr % SS(fs));	/* Open next directory */
			}
		}
	}

	return res;
}

static int get_ldnumber (const TCHAR** path)
{
	const TCHAR *tp;
	const TCHAR *tt;
	TCHAR tc;
	int i;
	int vol = -1;

	tt = tp = *path;
	if (!tp) return vol;	/* Invalid path name? */
	do {					/* Find a colon in the path */
		tc = *tt++;
	} while (!IsTerminator(tc) && tc != ':');

	if (tc == ':') {	/* DOS/Windows style volume ID? */
		i = FF_VOLUMES;
		if (IsDigit(*tp) && tp + 2 == tt) {	/* Is there a numeric volume ID + colon? */
			i = (int)*tp - '0';	/* Get the LD number */
		}
		if (i < FF_VOLUMES) {	/* If a volume ID is found, get the drive number and strip it */
			vol = i;		/* Drive number */
			*path = tt;		/* Snip the drive prefix off */
		}
		return vol;
	}
	vol = 0;		/* Default drive is 0 */
	return vol;		/* Return the default drive */
}

static UINT check_fs (FATFS* fs, LBA_t sect)
{
	WORD w, sign;
	BYTE b;

	fs->wflag = 0; fs->winsect = (LBA_t)0 - 1;
	if (move_window(fs, sect) != FR_OK) return 4;
	sign = ld_word(fs->win + BS_55AA);
	b = fs->win[BS_JmpBoot];
	if (b == 0xEB || b == 0xE9 || b == 0xE8) {
		if (sign == 0xAA55 && !memcmp(fs->win + BS_FilSysType32, "FAT32   ", 8)) {
			return 0;
		}

		w = ld_word(fs->win + BPB_BytsPerSec);
		b = fs->win[BPB_SecPerClus];
		if ((w & (w - 1)) == 0 && w >= FF_MIN_SS && w <= FF_MAX_SS
			&& b != 0 && (b & (b - 1)) == 0
			&& ld_word(fs->win + BPB_RsvdSecCnt) != 0
			&& (UINT)fs->win[BPB_NumFATs] - 1 <= 1
			&& ld_word(fs->win + BPB_RootEntCnt) != 0
			&& (ld_word(fs->win + BPB_TotSec16) >= 128 || ld_dword(fs->win + BPB_TotSec32) >= 0x10000)
			&& ld_word(fs->win + BPB_FATSz16) != 0) {
				return 0;
		}
	}
	return sign == 0xAA55 ? 2 : 3;
}

static UINT find_volume (FATFS* fs,	UINT part)
{
	UINT fmt, i;
	DWORD mbr_pt[4];


	fmt = check_fs(fs, 0);				/* Load sector 0 and check if it is an FAT VBR as SFD format */
	if (fmt != 2 && (fmt >= 3 || part == 0)) return fmt;	/* Returns if it is an FAT VBR as auto scan, not a BS or disk error */

	if (FF_MULTI_PARTITION && part > 4) return 3;	/* MBR has 4 partitions max */
	for (i = 0; i < 4; i++) {		/* Load partition offset in the MBR */
		mbr_pt[i] = ld_dword(fs->win + MBR_Table + i * SZ_PTE + PTE_StLba);
	}
	i = part ? part - 1 : 0;		/* Table index to find first */
	do {							/* Find an FAT volume */
		fmt = mbr_pt[i] ? check_fs(fs, mbr_pt[i]) : 3;	/* Check if the partition is FAT */
	} while (part == 0 && fmt >= 2 && ++i < 4);
	return fmt;
}

static FRESULT mount_volume (const TCHAR** path, FATFS** rfs, BYTE mode)
{
	int vol;
	FATFS *fs;
	DSTATUS stat;
	LBA_t bsect;
	DWORD tsect, sysect, fasize, nclst, szbfat;
	WORD nrsv;
	UINT fmt;

	*rfs = 0;
	vol = get_ldnumber(path);
	if (vol < 0) return FR_INVALID_DRIVE;

	/* Check if the filesystem object is valid or not */
	fs = FatFs[vol];					/* Get pointer to the filesystem object */
	if (!fs) return FR_NOT_ENABLED;		/* Is the filesystem object available? */
	*rfs = fs;							/* Return pointer to the filesystem object */

	mode &= (BYTE)~FA_READ;				/* Desired access mode, write access or not */
	if (fs->fs_type != 0) {				/* If the volume has been mounted */
		stat = disk_status(fs->pdrv);
		if (!(stat & STA_NOINIT)) {		/* and the physical drive is kept initialized */
			if (!FF_FS_READONLY && mode && (stat & STA_PROTECT)) {	/* Check write protection if needed */
				return FR_WRITE_PROTECTED;
			}
			return FR_OK;				/* The filesystem object is already valid */
		}
	}

	fs->fs_type = 0;					/* Invalidate the filesystem object */
	stat = disk_initialize(fs->pdrv);	/* Initialize the volume hosting physical drive */
	if (stat & STA_NOINIT) { 			/* Check if the initialization succeeded */
		return FR_NOT_READY;			/* Failed to initialize due to no medium or hard error */
	}
	if (!FF_FS_READONLY && mode && (stat & STA_PROTECT)) { /* Check disk write protection if needed */
		return FR_WRITE_PROTECTED;
	}

	fmt = find_volume(fs, 0);
	if (fmt == 4) return FR_DISK_ERR;		/* An error occurred in the disk I/O layer */
	if (fmt >= 2) return FR_NO_FILESYSTEM;	/* No FAT volume is found */
	bsect = fs->winsect;					/* Volume offset in the hosting physical drive */
	{
		if (ld_word(fs->win + BPB_BytsPerSec) != SS(fs)) return FR_NO_FILESYSTEM;	/* (BPB_BytsPerSec must be equal to the physical sector size) */

		fasize = ld_word(fs->win + BPB_FATSz16);		/* Number of sectors per FAT */
		if (fasize == 0) fasize = ld_dword(fs->win + BPB_FATSz32);
		fs->fsize = fasize;

		fs->n_fats = fs->win[BPB_NumFATs];				/* Number of FATs */
		if (fs->n_fats != 1 && fs->n_fats != 2) return FR_NO_FILESYSTEM;	/* (Must be 1 or 2) */
		fasize *= fs->n_fats;							/* Number of sectors for FAT area */

		fs->csize = fs->win[BPB_SecPerClus];			/* Cluster size */
		if (fs->csize == 0 || (fs->csize & (fs->csize - 1))) return FR_NO_FILESYSTEM;	/* (Must be power of 2) */

		fs->n_rootdir = ld_word(fs->win + BPB_RootEntCnt);	/* Number of root directory entries */
		if (fs->n_rootdir % (SS(fs) / SZDIRE)) return FR_NO_FILESYSTEM;	/* (Must be sector aligned) */

		tsect = ld_word(fs->win + BPB_TotSec16);		/* Number of sectors on the volume */
		if (tsect == 0) tsect = ld_dword(fs->win + BPB_TotSec32);

		nrsv = ld_word(fs->win + BPB_RsvdSecCnt);		/* Number of reserved sectors */
		if (nrsv == 0) return FR_NO_FILESYSTEM;			/* (Must not be 0) */

		/* Determine the FAT sub type */
		sysect = nrsv + fasize + fs->n_rootdir / (SS(fs) / SZDIRE);	/* RSV + FAT + DIR */
		if (tsect < sysect) return FR_NO_FILESYSTEM;	/* (Invalid volume size) */
		nclst = (tsect - sysect) / fs->csize;			/* Number of clusters */
		if (nclst == 0) return FR_NO_FILESYSTEM;		/* (Invalid volume size) */
		fmt = 0;
		if (nclst <= MAX_FAT32) fmt = FS_FAT32;
		if (nclst <= MAX_FAT16) fmt = FS_FAT16;
		if (nclst <= MAX_FAT12) fmt = FS_FAT12;
		if (fmt == 0) return FR_NO_FILESYSTEM;

		fs->n_fatent = nclst + 2;
		fs->volbase = bsect;
		fs->fatbase = bsect + nrsv;
		fs->database = bsect + sysect;
		if (fmt == FS_FAT32) {
			if (ld_word(fs->win + BPB_FSVer32) != 0) return FR_NO_FILESYSTEM;
			if (fs->n_rootdir != 0) return FR_NO_FILESYSTEM;
			fs->dirbase = ld_dword(fs->win + BPB_RootClus32);
			szbfat = fs->n_fatent * 4;
		} else {
			if (fs->n_rootdir == 0)	return FR_NO_FILESYSTEM;
			fs->dirbase = fs->fatbase + fasize;
			szbfat = (fmt == FS_FAT16) ?
				fs->n_fatent * 2 : fs->n_fatent * 3 / 2 + (fs->n_fatent & 1);
		}
		if (fs->fsize < (szbfat + (SS(fs) - 1)) / SS(fs)) return FR_NO_FILESYSTEM;

		fs->last_clst = fs->free_clst = 0xFFFFFFFF;
		fs->fsi_flag = 0x80;
		if (fmt == FS_FAT32
			&& ld_word(fs->win + BPB_FSInfo32) == 1
			&& move_window(fs, bsect + 1) == FR_OK)
		{
			fs->fsi_flag = 0;
			if (ld_word(fs->win + BS_55AA) == 0xAA55
				&& ld_dword(fs->win + FSI_LeadSig) == 0x41615252
				&& ld_dword(fs->win + FSI_StrucSig) == 0x61417272)
			{
				fs->free_clst = ld_dword(fs->win + FSI_Free_Count);
				fs->last_clst = ld_dword(fs->win + FSI_Nxt_Free);
			}
		}
	}

	fs->fs_type = (BYTE)fmt;
	fs->id = ++Fsid;
	fs->lfnbuf = LfnBuf;
	clear_share(fs);
	return FR_OK;
}

static FRESULT validate (FFOBJID* obj, FATFS** rfs)
{
	FRESULT res = FR_INVALID_OBJECT;

	if (obj && obj->fs && obj->fs->fs_type && obj->id == obj->fs->id) {	/* Test if the object is valid */
		if (!(disk_status(obj->fs->pdrv) & STA_NOINIT)) { /* Test if the hosting phsical drive is kept initialized */
			res = FR_OK;
		}
	}
	*rfs = (res == FR_OK) ? obj->fs : 0;	/* Return corresponding filesystem object if it is valid */
	return res;
}

FRESULT f_mount (FATFS* fs, const TCHAR* path, BYTE opt) {
	FATFS *cfs;
	int vol;
	FRESULT res;
	const TCHAR *rp = path;


	/* Get volume ID (logical drive number) */
	vol = get_ldnumber(&rp);
	if (vol < 0) return FR_INVALID_DRIVE;
	cfs = FatFs[vol];			/* Pointer to the filesystem object of the volume */

	if (cfs) {					/* Unregister current filesystem object if regsitered */
		FatFs[vol] = 0;
		clear_share(cfs);
		cfs->fs_type = 0;		/* Invalidate the filesystem object to be unregistered */
	}

	if (fs) {
		fs->pdrv = (BYTE)(vol);
		fs->fs_type = 0;
		FatFs[vol] = fs;
	}

	if (opt == 0) return FR_OK;	/* Do not mount now, it will be mounted in subsequent file functions */

	res = mount_volume(&path, &fs, 0);	/* Force mounted the volume */
	LEAVE_FF(fs, res);
}

FRESULT f_open (
	FIL* fp,			/* Pointer to the blank file object */
	const TCHAR* path,	/* Pointer to the file name */
	BYTE mode			/* Access mode and open mode flags */
)
{
	FRESULT res;
	DIR dj;
	FATFS *fs;
	DWORD cl, bcs, clst, tm;
	LBA_t sc;
	FSIZE_t ofs;

	if (!fp) return FR_INVALID_OBJECT;

	/* Get logical drive number */
	mode &= FF_FS_READONLY ? FA_READ : FA_READ | FA_WRITE | FA_CREATE_ALWAYS | FA_CREATE_NEW | FA_OPEN_ALWAYS | FA_OPEN_APPEND;
	res = mount_volume(&path, &fs, mode);
	if (res == FR_OK) {
		dj.obj.fs = fs;
		res = follow_path(&dj, path);	/* Follow the file path */
		if (res == FR_OK) {
			if (dj.fn[NSFLAG] & NS_NONAME) {	/* Origin directory itself? */
				res = FR_INVALID_NAME;
			}
			else {
				res = chk_share(&dj, (mode & ~FA_READ) ? 1 : 0);	/* Check if the file can be used */
			}
		}
		/* Create or Open a file */
		if (mode & (FA_CREATE_ALWAYS | FA_OPEN_ALWAYS | FA_CREATE_NEW)) {
			if (res != FR_OK) {					/* No file, create new */
				if (res == FR_NO_FILE) {		/* There is no file to open, create a new entry */
					res = enq_share() ? dir_register(&dj) : FR_TOO_MANY_OPEN_FILES;
				}
				mode |= FA_CREATE_ALWAYS;		/* File is created */
			}
			else {								/* Any object with the same name is already existing */
				if (dj.obj.attr & (AM_RDO | AM_DIR)) {	/* Cannot overwrite it (R/O or DIR) */
					res = FR_DENIED;
				} else {
					if (mode & FA_CREATE_NEW) res = FR_EXIST;	/* Cannot create as new file */
				}
			}
			if (res == FR_OK && (mode & FA_CREATE_ALWAYS)) {	/* Truncate the file if overwrite mode */
				{
					/* Set directory entry initial state */
					tm = GET_FATTIME();					/* Set created time */
					st_dword(dj.dir + DIR_CrtTime, tm);
					st_dword(dj.dir + DIR_ModTime, tm);
					cl = ld_clust(fs, dj.dir);			/* Get current cluster chain */
					dj.dir[DIR_Attr] = AM_ARC;			/* Reset attribute */
					st_clust(fs, dj.dir, 0);			/* Reset file allocation info */
					st_dword(dj.dir + DIR_FileSize, 0);
					fs->wflag = 1;
					if (cl != 0) {						/* Remove the cluster chain if exist */
						sc = fs->winsect;
						res = remove_chain(&dj.obj, cl, 0);
						if (res == FR_OK) {
							res = move_window(fs, sc);
							fs->last_clst = cl - 1;		/* Reuse the cluster hole */
						}
					}
				}
			}
		}
		else {
			if (res == FR_OK) {
				if (dj.obj.attr & AM_DIR) {
					res = FR_NO_FILE;
				} else {
					if ((mode & FA_WRITE) && (dj.obj.attr & AM_RDO)) {
						res = FR_DENIED;
					}
				}
			}
		}
		if (res == FR_OK) {
			if (mode & FA_CREATE_ALWAYS) mode |= FA_MODIFIED;	/* Set file change flag if created or overwritten */
			fp->dir_sect = fs->winsect;			/* Pointer to the directory entry */
			fp->dir_ptr = dj.dir;
			fp->obj.lockid = inc_share(&dj, (mode & ~FA_READ) ? 1 : 0);	/* Lock the file for this session */
			if (fp->obj.lockid == 0) res = FR_INT_ERR;
		}

		if (res == FR_OK) {
			{
				fp->obj.sclust = ld_clust(fs, dj.dir);
				fp->obj.objsize = ld_dword(dj.dir + DIR_FileSize);
			}
			fp->cltbl = 0;		/* Disable fast seek mode */
			fp->obj.fs = fs;	/* Validate the file object */
			fp->obj.id = fs->id;
			fp->flag = mode;	/* Set file access mode */
			fp->err = 0;		/* Clear error flag */
			fp->sect = 0;		/* Invalidate current data sector */
			fp->fptr = 0;		/* Set file pointer top of the file */
			memset(fp->buf, 0, sizeof fp->buf);	/* Clear sector buffer */
			if ((mode & FA_SEEKEND) && fp->obj.objsize > 0) {	/* Seek to end of file if FA_OPEN_APPEND is specified */
				fp->fptr = fp->obj.objsize;			/* Offset to seek */
				bcs = (DWORD)fs->csize * SS(fs);	/* Cluster size in byte */
				clst = fp->obj.sclust;				/* Follow the cluster chain */
				for (ofs = fp->obj.objsize; res == FR_OK && ofs > bcs; ofs -= bcs) {
					clst = get_fat(&fp->obj, clst);
					if (clst <= 1) res = FR_INT_ERR;
					if (clst == 0xFFFFFFFF) res = FR_DISK_ERR;
				}
				fp->clust = clst;
				if (res == FR_OK && ofs % SS(fs)) {	/* Fill sector buffer if not on the sector boundary */
					sc = clst2sect(fs, clst);
					if (sc == 0) {
						res = FR_INT_ERR;
					} else {
						fp->sect = sc + (DWORD)(ofs / SS(fs));
						if (disk_read(fs->pdrv, fp->buf, fp->sect, 1) != RES_OK) res = FR_DISK_ERR;
					}
				}
				if (res != FR_OK) dec_share(fp->obj.lockid); /* Decrement file open counter if seek failed */
			}
		}
	}

	if (res != FR_OK) fp->obj.fs = 0;	/* Invalidate file object on error */

	LEAVE_FF(fs, res);
}

FRESULT f_read (
	FIL* fp, 	/* Open file to be read */
	void* buff,	/* Data buffer to store the read data */
	UINT btr,	/* Number of bytes to read */
	UINT* br	/* Number of bytes read */
)
{
	FRESULT res;
	FATFS *fs;
	DWORD clst;
	LBA_t sect;
	FSIZE_t remain;
	UINT rcnt, cc, csect;
	BYTE *rbuff = (BYTE*)buff;


	*br = 0;	/* Clear read byte counter */
	res = validate(&fp->obj, &fs);				/* Check validity of the file object */
	if (res != FR_OK || (res = (FRESULT)fp->err) != FR_OK) LEAVE_FF(fs, res);	/* Check validity */
	if (!(fp->flag & FA_READ)) LEAVE_FF(fs, FR_DENIED); /* Check access mode */
	remain = fp->obj.objsize - fp->fptr;
	if (btr > remain) btr = (UINT)remain;		/* Truncate btr by remaining bytes */

	for ( ; btr > 0; btr -= rcnt, *br += rcnt, rbuff += rcnt, fp->fptr += rcnt) {	/* Repeat until btr bytes read */
		if (fp->fptr % SS(fs) == 0) {			/* On the sector boundary? */
			csect = (UINT)(fp->fptr / SS(fs) & (fs->csize - 1));	/* Sector offset in the cluster */
			if (csect == 0) {					/* On the cluster boundary? */
				if (fp->fptr == 0) {			/* On the top of the file? */
					clst = fp->obj.sclust;		/* Follow cluster chain from the origin */
				} else {						/* Middle or end of the file */
					if (fp->cltbl) {
						clst = clmt_clust(fp, fp->fptr);	/* Get cluster# from the CLMT */
					} else
					{
						clst = get_fat(&fp->obj, fp->clust);	/* Follow cluster chain on the FAT */
					}
				}
				if (clst < 2) ABORT(fs, FR_INT_ERR);
				if (clst == 0xFFFFFFFF) ABORT(fs, FR_DISK_ERR);
				fp->clust = clst;				/* Update current cluster */
			}
			sect = clst2sect(fs, fp->clust);	/* Get current sector */
			if (sect == 0) ABORT(fs, FR_INT_ERR);
			sect += csect;
			cc = btr / SS(fs);					/* When remaining bytes >= sector size, */
			if (cc > 0) {						/* Read maximum contiguous sectors directly */
				if (csect + cc > fs->csize) {	/* Clip at cluster boundary */
					cc = fs->csize - csect;
				}
				if (disk_read(fs->pdrv, rbuff, sect, cc) != RES_OK) ABORT(fs, FR_DISK_ERR);
				if ((fp->flag & FA_DIRTY) && fp->sect - sect < cc) {
					memcpy(rbuff + ((fp->sect - sect) * SS(fs)), fp->buf, SS(fs));
				}
				rcnt = SS(fs) * cc;				/* Number of bytes transferred */
				continue;
			}
			if (fp->sect != sect) {			/* Load data sector if not in cache */
				if (fp->flag & FA_DIRTY) {		/* Write-back dirty sector cache */
					if (disk_write(fs->pdrv, fp->buf, fp->sect, 1) != RES_OK) ABORT(fs, FR_DISK_ERR);
					fp->flag &= (BYTE)~FA_DIRTY;
				}
				if (disk_read(fs->pdrv, fp->buf, sect, 1) != RES_OK) ABORT(fs, FR_DISK_ERR);	/* Fill sector cache */
			}
			fp->sect = sect;
		}
		rcnt = SS(fs) - (UINT)fp->fptr % SS(fs);	/* Number of bytes remains in the sector */
		if (rcnt > btr) rcnt = btr;					/* Clip it by btr if needed */
		memcpy(rbuff, fp->buf + fp->fptr % SS(fs), rcnt);	/* Extract partial sector */
	}

	LEAVE_FF(fs, FR_OK);
}

FRESULT f_write (
	FIL* fp,			/* Open file to be written */
	const void* buff,	/* Data to be written */
	UINT btw,			/* Number of bytes to write */
	UINT* bw			/* Number of bytes written */
)
{
	FRESULT res;
	FATFS *fs;
	DWORD clst;
	LBA_t sect;
	UINT wcnt, cc, csect;
	const BYTE *wbuff = (const BYTE*)buff;


	*bw = 0;	/* Clear write byte counter */
	res = validate(&fp->obj, &fs);			/* Check validity of the file object */
	if (res != FR_OK || (res = (FRESULT)fp->err) != FR_OK) LEAVE_FF(fs, res);	/* Check validity */
	if (!(fp->flag & FA_WRITE)) LEAVE_FF(fs, FR_DENIED);	/* Check access mode */

	/* Check fptr wrap-around (file size cannot reach 4 GiB at FAT volume) */
	if ((!FF_FS_EXFAT || fs->fs_type != FS_EXFAT) && (DWORD)(fp->fptr + btw) < (DWORD)fp->fptr) {
		btw = (UINT)(0xFFFFFFFF - (DWORD)fp->fptr);
	}

	for ( ; btw > 0; btw -= wcnt, *bw += wcnt, wbuff += wcnt, fp->fptr += wcnt, fp->obj.objsize = (fp->fptr > fp->obj.objsize) ? fp->fptr : fp->obj.objsize) {	/* Repeat until all data written */
		if (fp->fptr % SS(fs) == 0) {		/* On the sector boundary? */
			csect = (UINT)(fp->fptr / SS(fs)) & (fs->csize - 1);	/* Sector offset in the cluster */
			if (csect == 0) {				/* On the cluster boundary? */
				if (fp->fptr == 0) {		/* On the top of the file? */
					clst = fp->obj.sclust;	/* Follow from the origin */
					if (clst == 0) {		/* If no cluster is allocated, */
						clst = create_chain(&fp->obj, 0);	/* create a new cluster chain */
					}
				} else {					/* On the middle or end of the file */
					if (fp->cltbl) {
						clst = clmt_clust(fp, fp->fptr);	/* Get cluster# from the CLMT */
					} else
					{
						clst = create_chain(&fp->obj, fp->clust);	/* Follow or stretch cluster chain on the FAT */
					}
				}
				if (clst == 0) break;		/* Could not allocate a new cluster (disk full) */
				if (clst == 1) ABORT(fs, FR_INT_ERR);
				if (clst == 0xFFFFFFFF) ABORT(fs, FR_DISK_ERR);
				fp->clust = clst;			/* Update current cluster */
				if (fp->obj.sclust == 0) fp->obj.sclust = clst;	/* Set start cluster if the first write */
			}
			if (fp->flag & FA_DIRTY) {		/* Write-back sector cache */
				if (disk_write(fs->pdrv, fp->buf, fp->sect, 1) != RES_OK) ABORT(fs, FR_DISK_ERR);
				fp->flag &= (BYTE)~FA_DIRTY;
			}
			sect = clst2sect(fs, fp->clust);	/* Get current sector */
			if (sect == 0) ABORT(fs, FR_INT_ERR);
			sect += csect;
			cc = btw / SS(fs);				/* When remaining bytes >= sector size, */
			if (cc > 0) {					/* Write maximum contiguous sectors directly */
				if (csect + cc > fs->csize) {	/* Clip at cluster boundary */
					cc = fs->csize - csect;
				}
				if (disk_write(fs->pdrv, wbuff, sect, cc) != RES_OK) ABORT(fs, FR_DISK_ERR);
				if (fp->sect - sect < cc) { /* Refill sector cache if it gets invalidated by the direct write */
					memcpy(fp->buf, wbuff + ((fp->sect - sect) * SS(fs)), SS(fs));
					fp->flag &= (BYTE)~FA_DIRTY;
				}
				wcnt = SS(fs) * cc;		/* Number of bytes transferred */
				continue;
			}
			if (fp->sect != sect && 		/* Fill sector cache with file data */
				fp->fptr < fp->obj.objsize &&
				disk_read(fs->pdrv, fp->buf, sect, 1) != RES_OK) {
					ABORT(fs, FR_DISK_ERR);
			}
			fp->sect = sect;
		}
		wcnt = SS(fs) - (UINT)fp->fptr % SS(fs);	/* Number of bytes remains in the sector */
		if (wcnt > btw) wcnt = btw;					/* Clip it by btw if needed */
		memcpy(fp->buf + fp->fptr % SS(fs), wbuff, wcnt);	/* Fit data to the sector */
		fp->flag |= FA_DIRTY;
	}

	fp->flag |= FA_MODIFIED;				/* Set file change flag */

	LEAVE_FF(fs, FR_OK);
}

FRESULT f_sync (FIL* fp)
{
	FRESULT res;
	FATFS *fs;
	DWORD tm;
	BYTE *dir;


	res = validate(&fp->obj, &fs);
	if (res == FR_OK) {
		if (fp->flag & FA_MODIFIED) {
			if (fp->flag & FA_DIRTY) {
				if (disk_write(fs->pdrv, fp->buf, fp->sect, 1) != RES_OK) LEAVE_FF(fs, FR_DISK_ERR);
				fp->flag &= (BYTE)~FA_DIRTY;
			}
			tm = GET_FATTIME();
			{
				res = move_window(fs, fp->dir_sect);
				if (res == FR_OK) {
					dir = fp->dir_ptr;
					dir[DIR_Attr] |= AM_ARC;						/* Set archive attribute to indicate that the file has been changed */
					st_clust(fp->obj.fs, dir, fp->obj.sclust);		/* Update file allocation information  */
					st_dword(dir + DIR_FileSize, (DWORD)fp->obj.objsize);	/* Update file size */
					st_dword(dir + DIR_ModTime, tm);				/* Update modified time */
					st_word(dir + DIR_LstAccDate, 0);
					fs->wflag = 1;
					res = sync_fs(fs);					/* Restore it to the directory */
					fp->flag &= (BYTE)~FA_MODIFIED;
				}
			}
		}
	}

	LEAVE_FF(fs, res);
}

FRESULT f_close (FIL* fp)
{
	FRESULT res;
	FATFS *fs;

	res = f_sync(fp);					/* Flush cached data */
	if (res == FR_OK)
	{
		res = validate(&fp->obj, &fs);	/* Lock volume */
		if (res == FR_OK) {
			res = dec_share(fp->obj.lockid);		/* Decrement file open counter */
			if (res == FR_OK) fp->obj.fs = 0;	/* Invalidate file object */
		}
	}
	return res;
}

FRESULT f_lseek (FIL* fp, FSIZE_t ofs)
{
	FRESULT res;
	FATFS *fs;
	DWORD clst, bcs;
	LBA_t nsect;
	FSIZE_t ifptr;
	DWORD cl, pcl, ncl, tcl, tlen, ulen;
	DWORD *tbl;
	LBA_t dsc;

	res = validate(&fp->obj, &fs);		/* Check validity of the file object */
	if (res == FR_OK) res = (FRESULT)fp->err;
	if (res != FR_OK) LEAVE_FF(fs, res);

	if (fp->cltbl) {	/* Fast seek */
		if (ofs == CREATE_LINKMAP) {	/* Create CLMT */
			tbl = fp->cltbl;
			tlen = *tbl++; ulen = 2;	/* Given table size and required table size */
			cl = fp->obj.sclust;		/* Origin of the chain */
			if (cl != 0) {
				do {
					/* Get a fragment */
					tcl = cl; ncl = 0; ulen += 2;	/* Top, length and used items */
					do {
						pcl = cl; ncl++;
						cl = get_fat(&fp->obj, cl);
						if (cl <= 1) ABORT(fs, FR_INT_ERR);
						if (cl == 0xFFFFFFFF) ABORT(fs, FR_DISK_ERR);
					} while (cl == pcl + 1);
					if (ulen <= tlen) {		/* Store the length and top of the fragment */
						*tbl++ = ncl; *tbl++ = tcl;
					}
				} while (cl < fs->n_fatent);	/* Repeat until end of chain */
			}
			*fp->cltbl = ulen;	/* Number of items used */
			if (ulen <= tlen) {
				*tbl = 0;		/* Terminate table */
			} else {
				res = FR_NOT_ENOUGH_CORE;	/* Given table size is smaller than required */
			}
		} else {						/* Fast seek */
			if (ofs > fp->obj.objsize) ofs = fp->obj.objsize;	/* Clip offset at the file size */
			fp->fptr = ofs;				/* Set file pointer */
			if (ofs > 0) {
				fp->clust = clmt_clust(fp, ofs - 1);
				dsc = clst2sect(fs, fp->clust);
				if (dsc == 0) ABORT(fs, FR_INT_ERR);
				dsc += (DWORD)((ofs - 1) / SS(fs)) & (fs->csize - 1);
				if (fp->fptr % SS(fs) && dsc != fp->sect) {	/* Refill sector cache if needed */
					if (fp->flag & FA_DIRTY) {		/* Write-back dirty sector cache */
						if (disk_write(fs->pdrv, fp->buf, fp->sect, 1) != RES_OK) ABORT(fs, FR_DISK_ERR);
						fp->flag &= (BYTE)~FA_DIRTY;
					}
					if (disk_read(fs->pdrv, fp->buf, dsc, 1) != RES_OK) ABORT(fs, FR_DISK_ERR);	/* Load current sector */
					fp->sect = dsc;
				}
			}
		}
	} else
	{
		if (ofs > fp->obj.objsize && (FF_FS_READONLY || !(fp->flag & FA_WRITE))) {	/* In read-only mode, clip offset with the file size */
			ofs = fp->obj.objsize;
		}
		ifptr = fp->fptr;
		fp->fptr = nsect = 0;
		if (ofs > 0) {
			bcs = (DWORD)fs->csize * SS(fs);	/* Cluster size (byte) */
			if (ifptr > 0 &&
				(ofs - 1) / bcs >= (ifptr - 1) / bcs) {	/* When seek to same or following cluster, */
				fp->fptr = (ifptr - 1) & ~(FSIZE_t)(bcs - 1);	/* start from the current cluster */
				ofs -= fp->fptr;
				clst = fp->clust;
			} else {									/* When seek to back cluster, */
				clst = fp->obj.sclust;					/* start from the first cluster */
				if (clst == 0) {						/* If no cluster chain, create a new chain */
					clst = create_chain(&fp->obj, 0);
					if (clst == 1) ABORT(fs, FR_INT_ERR);
					if (clst == 0xFFFFFFFF) ABORT(fs, FR_DISK_ERR);
					fp->obj.sclust = clst;
				}
				fp->clust = clst;
			}
			if (clst != 0) {
				while (ofs > bcs) {						/* Cluster following loop */
					ofs -= bcs; fp->fptr += bcs;
					if (fp->flag & FA_WRITE) {			/* Check if in write mode or not */
						if (FF_FS_EXFAT && fp->fptr > fp->obj.objsize) {	/* No FAT chain object needs correct objsize to generate FAT value */
							fp->obj.objsize = fp->fptr;
							fp->flag |= FA_MODIFIED;
						}
						clst = create_chain(&fp->obj, clst);	/* Follow chain with forceed stretch */
						if (clst == 0) {				/* Clip file size in case of disk full */
							ofs = 0; break;
						}
					} else
					{
						clst = get_fat(&fp->obj, clst);	/* Follow cluster chain if not in write mode */
					}
					if (clst == 0xFFFFFFFF) ABORT(fs, FR_DISK_ERR);
					if (clst <= 1 || clst >= fs->n_fatent) ABORT(fs, FR_INT_ERR);
					fp->clust = clst;
				}
				fp->fptr += ofs;
				if (ofs % SS(fs)) {
					nsect = clst2sect(fs, clst);	/* Current sector */
					if (nsect == 0) ABORT(fs, FR_INT_ERR);
					nsect += (DWORD)(ofs / SS(fs));
				}
			}
		}
		if (!FF_FS_READONLY && fp->fptr > fp->obj.objsize) {	/* Set file change flag if the file size is extended */
			fp->obj.objsize = fp->fptr;
			fp->flag |= FA_MODIFIED;
		}
		if (fp->fptr % SS(fs) && nsect != fp->sect) {	/* Fill sector cache if needed */
			if (fp->flag & FA_DIRTY) {			/* Write-back dirty sector cache */
				if (disk_write(fs->pdrv, fp->buf, fp->sect, 1) != RES_OK) ABORT(fs, FR_DISK_ERR);
				fp->flag &= (BYTE)~FA_DIRTY;
			}
			if (disk_read(fs->pdrv, fp->buf, nsect, 1) != RES_OK) ABORT(fs, FR_DISK_ERR);	/* Fill sector cache */
			fp->sect = nsect;
		}
	}

	LEAVE_FF(fs, res);
}

FRESULT f_opendir (
	DIR* dp,			/* Pointer to directory object to create */
	const TCHAR* path	/* Pointer to the directory path */
)
{
	FRESULT res;
	FATFS *fs;


	if (!dp) return FR_INVALID_OBJECT;

	res = mount_volume(&path, &fs, 0);
	if (res == FR_OK) {
		dp->obj.fs = fs;
		res = follow_path(dp, path);			/* Follow the path to the directory */
		if (res == FR_OK) {						/* Follow completed */
			if (!(dp->fn[NSFLAG] & NS_NONAME)) {	/* It is not the origin directory itself */
				if (dp->obj.attr & AM_DIR) {		/* This object is a sub-directory */
					{
						dp->obj.sclust = ld_clust(fs, dp->dir);	/* Get object allocation info */
					}
				} else {						/* This object is a file */
					res = FR_NO_PATH;
				}
			}
			if (res == FR_OK) {
				dp->obj.id = fs->id;
				res = dir_sdi(dp, 0);
				if (res == FR_OK) {
					if (dp->obj.sclust != 0) {
						dp->obj.lockid = inc_share(dp, 0);
						if (!dp->obj.lockid) res = FR_TOO_MANY_OPEN_FILES;
					} else {
						dp->obj.lockid = 0;
					}
				}
			}
		}
		if (res == FR_NO_FILE) res = FR_NO_PATH;
	}
	if (res != FR_OK) dp->obj.fs = 0;

	LEAVE_FF(fs, res);
}

FRESULT f_closedir (DIR *dp)
{
	FRESULT res;
	FATFS *fs;
	res = validate(&dp->obj, &fs);
	if (res == FR_OK) {
		if (dp->obj.lockid) res = dec_share(dp->obj.lockid);
		if (res == FR_OK) dp->obj.fs = 0;
	}
	return res;
}

FRESULT f_readdir (DIR* dp, FILINFO* fno) {
	FRESULT res;
	FATFS *fs;

	res = validate(&dp->obj, &fs);
	if (res == FR_OK) {
		if (!fno) {
			res = dir_sdi(dp, 0);
		} else {
			res = DIR_READ_FILE(dp);
			if (res == FR_NO_FILE) res = FR_OK;
			if (res == FR_OK) {
				get_fileinfo(dp, fno);
				res = dir_next(dp, 0);
				if (res == FR_NO_FILE) res = FR_OK;
			}
		}
	}
	LEAVE_FF(fs, res);
}

FRESULT f_findnext (DIR* dp, FILINFO* fno) {
	FRESULT res;

	for (;;) {
		res = f_readdir(dp, fno);
		if (res != FR_OK || !fno || !fno->fname[0]) break;
		if (pattern_match(dp->pat, fno->fname, 0, FIND_RECURS)) break;
	}
	return res;
}

FRESULT f_findfirst (DIR* dp, FILINFO* fno, const TCHAR* path, const TCHAR* pattern)
{
	FRESULT res;
	dp->pat = pattern;
	res = f_opendir(dp, path);
	if (res == FR_OK) {
		res = f_findnext(dp, fno);
	}
	return res;
}

FRESULT f_stat (const TCHAR* path, FILINFO* fno) {
	FRESULT res;
	DIR dj;

	res = mount_volume(&path, &dj.obj.fs, 0);
	if (res == FR_OK) {
		res = follow_path(&dj, path);
		if (res == FR_OK) {
			if (dj.fn[NSFLAG] & NS_NONAME) {
				res = FR_INVALID_NAME;
			} else {
				if (fno) get_fileinfo(&dj, fno);
			}
		}
	}

	LEAVE_FF(dj.obj.fs, res);
}

FRESULT f_getfree (const TCHAR* path, DWORD* nclst, FATFS** fatfs)
{
	FRESULT res;
	FATFS *fs;
	DWORD nfree, clst, stat;
	LBA_t sect;
	UINT i;
	FFOBJID obj;

	res = mount_volume(&path, &fs, 0);
	if (res == FR_OK) {
		*fatfs = fs;
		if (fs->free_clst <= fs->n_fatent - 2) {
			*nclst = fs->free_clst;
		} else {
			nfree = 0;
			if (fs->fs_type == FS_FAT12) {
				clst = 2; obj.fs = fs;
				do {
					stat = get_fat(&obj, clst);
					if (stat == 0xFFFFFFFF) {
						res = FR_DISK_ERR; break;
					}
					if (stat == 1) {
						res = FR_INT_ERR; break;
					}
					if (stat == 0) nfree++;
				} while (++clst < fs->n_fatent);
			} else {
				{	/* FAT16/32: Scan WORD/DWORD FAT entries */
					clst = fs->n_fatent;	/* Number of entries */
					sect = fs->fatbase;		/* Top of the FAT */
					i = 0;					/* Offset in the sector */
					do {	/* Counts numbuer of entries with zero in the FAT */
						if (i == 0) {	/* New sector? */
							res = move_window(fs, sect++);
							if (res != FR_OK) break;
						}
						if (fs->fs_type == FS_FAT16) {
							if (ld_word(fs->win + i) == 0) nfree++;
							i += 2;
						} else {
							if ((ld_dword(fs->win + i) & 0x0FFFFFFF) == 0) nfree++;
							i += 4;
						}
						i %= SS(fs);
					} while (--clst);
				}
			}
			if (res == FR_OK) {
				*nclst = nfree;
				fs->free_clst = nfree;
				fs->fsi_flag |= 1;
			}
		}
	}

	LEAVE_FF(fs, res);
}

FRESULT f_truncate (FIL* fp)
{
	FRESULT res;
	FATFS *fs;
	DWORD ncl;


	res = validate(&fp->obj, &fs);
	if (res != FR_OK || (res = (FRESULT)fp->err) != FR_OK) LEAVE_FF(fs, res);
	if (!(fp->flag & FA_WRITE)) LEAVE_FF(fs, FR_DENIED);

	if (fp->fptr < fp->obj.objsize) {
		if (fp->fptr == 0) {
			res = remove_chain(&fp->obj, fp->obj.sclust, 0);
			fp->obj.sclust = 0;
		} else {
			ncl = get_fat(&fp->obj, fp->clust);
			res = FR_OK;
			if (ncl == 0xFFFFFFFF) res = FR_DISK_ERR;
			if (ncl == 1) res = FR_INT_ERR;
			if (res == FR_OK && ncl < fs->n_fatent) {
				res = remove_chain(&fp->obj, ncl, fp->clust);
			}
		}
		fp->obj.objsize = fp->fptr;
		fp->flag |= FA_MODIFIED;
		if (res == FR_OK && (fp->flag & FA_DIRTY)) {
			if (disk_write(fs->pdrv, fp->buf, fp->sect, 1) != RES_OK) {
				res = FR_DISK_ERR;
			} else {
				fp->flag &= (BYTE)~FA_DIRTY;
			}
		}
		if (res != FR_OK) ABORT(fs, res);
	}

	LEAVE_FF(fs, res);
}

FRESULT f_unlink (const TCHAR* path)
{
	FRESULT res;
	FATFS *fs;
	DIR dj, sdj;
	DWORD dclst = 0;

	res = mount_volume(&path, &fs, FA_WRITE);
	if (res == FR_OK) {
		dj.obj.fs = fs;
		res = follow_path(&dj, path);
		if (FF_FS_RPATH && res == FR_OK && (dj.fn[NSFLAG] & NS_DOT)) {
			res = FR_INVALID_NAME;
		}
		if (res == FR_OK) res = chk_share(&dj, 2);
		if (res == FR_OK) {
			if (dj.fn[NSFLAG] & NS_NONAME) {
				res = FR_INVALID_NAME;
			} else {
				if (dj.obj.attr & AM_RDO) {
					res = FR_DENIED;
				}
			}
			if (res == FR_OK) {
				{
					dclst = ld_clust(fs, dj.dir);
				}
				if (dj.obj.attr & AM_DIR) {
					{
						sdj.obj.fs = fs;
						sdj.obj.sclust = dclst;
						res = dir_sdi(&sdj, 0);
						if (res == FR_OK) {
							res = DIR_READ_FILE(&sdj);
							if (res == FR_OK) res = FR_DENIED;
							if (res == FR_NO_FILE) res = FR_OK;
						}
					}
				}
			}
			if (res == FR_OK) {
				res = dir_remove(&dj);
				if (res == FR_OK && dclst != 0) {
					res = remove_chain(&dj.obj, dclst, 0);
				}
				if (res == FR_OK) res = sync_fs(fs);
			}
		}
	}

	LEAVE_FF(fs, res);
}

FRESULT f_mkdir (const TCHAR* path)
{
	FRESULT res;
	FATFS *fs;
	DIR dj;
	FFOBJID sobj;
	DWORD dcl, pcl, tm;

	res = mount_volume(&path, &fs, FA_WRITE);
	if (res == FR_OK) {
		dj.obj.fs = fs;
		res = follow_path(&dj, path);
		if (res == FR_OK) res = FR_EXIST;
		if (FF_FS_RPATH && res == FR_NO_FILE && (dj.fn[NSFLAG] & NS_DOT)) {
			res = FR_INVALID_NAME;
		}
		if (res == FR_NO_FILE) {				/* It is clear to create a new directory */
			sobj.fs = fs;						/* New object id to create a new chain */
			dcl = create_chain(&sobj, 0);		/* Allocate a cluster for the new directory */
			res = FR_OK;
			if (dcl == 0) res = FR_DENIED;		/* No space to allocate a new cluster? */
			if (dcl == 1) res = FR_INT_ERR;		/* Any insanity? */
			if (dcl == 0xFFFFFFFF) res = FR_DISK_ERR;	/* Disk error? */
			tm = GET_FATTIME();
			if (res == FR_OK) {
				res = dir_clear(fs, dcl);		/* Clean up the new table */
				if (res == FR_OK) {
					if (!FF_FS_EXFAT || fs->fs_type != FS_EXFAT) {	/* Create dot entries (FAT only) */
						memset(fs->win + DIR_Name, ' ', 11);	/* Create "." entry */
						fs->win[DIR_Name] = '.';
						fs->win[DIR_Attr] = AM_DIR;
						st_dword(fs->win + DIR_ModTime, tm);
						st_clust(fs, fs->win, dcl);
						memcpy(fs->win + SZDIRE, fs->win, SZDIRE);	/* Create ".." entry */
						fs->win[SZDIRE + 1] = '.'; pcl = dj.obj.sclust;
						st_clust(fs, fs->win + SZDIRE, pcl);
						fs->wflag = 1;
					}
					res = dir_register(&dj);	/* Register the object to the parent directoy */
				}
			}
			if (res == FR_OK) {
				{
					st_dword(dj.dir + DIR_ModTime, tm);	/* Created time */
					st_clust(fs, dj.dir, dcl);			/* Table start cluster */
					dj.dir[DIR_Attr] = AM_DIR;			/* Attribute */
					fs->wflag = 1;
				}
				if (res == FR_OK) {
					res = sync_fs(fs);
				}
			} else {
				remove_chain(&sobj, dcl, 0);		/* Could not register, remove the allocated cluster */
			}
		}
	}

	LEAVE_FF(fs, res);
}

FRESULT f_rename (const TCHAR* path_old, const TCHAR* path_new)
{
	FRESULT res;
	FATFS *fs;
	DIR djo, djn;
	BYTE buf[SZDIRE], *dir;
	LBA_t sect;

	get_ldnumber(&path_new);						/* Snip the drive number of new name off */
	res = mount_volume(&path_old, &fs, FA_WRITE);	/* Get logical drive of the old object */
	if (res == FR_OK) {
		djo.obj.fs = fs;
		res = follow_path(&djo, path_old);			/* Check old object */
		if (res == FR_OK && (djo.fn[NSFLAG] & (NS_DOT | NS_NONAME))) res = FR_INVALID_NAME;	/* Check validity of name */
		if (res == FR_OK) {
			res = chk_share(&djo, 2);
		}
		if (res == FR_OK) {					/* Object to be renamed is found */
			{	/* At FAT/FAT32 volume */
				memcpy(buf, djo.dir, SZDIRE);			/* Save directory entry of the object */
				memcpy(&djn, &djo, sizeof (DIR));		/* Duplicate the directory object */
				res = follow_path(&djn, path_new);		/* Make sure if new object name is not in use */
				if (res == FR_OK) {						/* Is new name already in use by any other object? */
					res = (djn.obj.sclust == djo.obj.sclust && djn.dptr == djo.dptr) ? FR_NO_FILE : FR_EXIST;
				}
				if (res == FR_NO_FILE) { 				/* It is a valid path and no name collision */
					res = dir_register(&djn);			/* Register the new entry */
					if (res == FR_OK) {
						dir = djn.dir;					/* Copy directory entry of the object except name */
						memcpy(dir + 13, buf + 13, SZDIRE - 13);
						dir[DIR_Attr] = buf[DIR_Attr];
						if (!(dir[DIR_Attr] & AM_DIR)) dir[DIR_Attr] |= AM_ARC;	/* Set archive attribute if it is a file */
						fs->wflag = 1;
						if ((dir[DIR_Attr] & AM_DIR) && djo.obj.sclust != djn.obj.sclust) {	/* Update .. entry in the sub-directory if needed */
							sect = clst2sect(fs, ld_clust(fs, dir));
							if (sect == 0) {
								res = FR_INT_ERR;
							} else {
								res = move_window(fs, sect);
								dir = fs->win + SZDIRE * 1;	/* Ptr to .. entry */
								if (res == FR_OK && dir[1] == '.') {
									st_clust(fs, dir, djn.obj.sclust);
									fs->wflag = 1;
								}
							}
						}
					}
				}
			}
			if (res == FR_OK) {
				res = dir_remove(&djo);
				if (res == FR_OK) {
					res = sync_fs(fs);
				}
			}
		}
	}

	LEAVE_FF(fs, res);
}

FRESULT f_expand (FIL* fp, FSIZE_t fsz, BYTE opt) {
	FRESULT res;
	FATFS *fs;
	DWORD n, clst, stcl, scl, ncl, tcl, lclst;

	res = validate(&fp->obj, &fs);
	if (res != FR_OK || (res = (FRESULT)fp->err) != FR_OK) LEAVE_FF(fs, res);
	if (fsz == 0 || fp->obj.objsize != 0 || !(fp->flag & FA_WRITE)) LEAVE_FF(fs, FR_DENIED);
	n = (DWORD)fs->csize * SS(fs);
	tcl = (DWORD)(fsz / n) + ((fsz & (n - 1)) ? 1 : 0);
	stcl = fs->last_clst; lclst = 0;
	if (stcl < 2 || stcl >= fs->n_fatent) stcl = 2;
	scl = clst = stcl; ncl = 0;
	for (;;) {
		n = get_fat(&fp->obj, clst);
		if (++clst >= fs->n_fatent) clst = 2;
		if (n == 1) {
			res = FR_INT_ERR; break;
		}
		if (n == 0xFFFFFFFF) {
			res = FR_DISK_ERR; break;
		}
		if (n == 0) {
			if (++ncl == tcl) break;
		} else {
			scl = clst; ncl = 0;
		}
		if (clst == stcl) {
			res = FR_DENIED; break;
		}
	}
	if (res == FR_OK) {
		if (opt) {
			for (clst = scl, n = tcl; n; clst++, n--) {
				res = put_fat(fs, clst, (n == 1) ? 0xFFFFFFFF : clst + 1);
				if (res != FR_OK) break;
				lclst = clst;
			}
		} else {
			lclst = scl - 1;
		}
	}

	if (res == FR_OK) {
		fs->last_clst = lclst;		/* Set suggested start cluster to start next */
		if (opt) {	/* Is it allocated now? */
			fp->obj.sclust = scl;		/* Update object allocation information */
			fp->obj.objsize = fsz;
			fp->flag |= FA_MODIFIED;
			if (fs->free_clst <= fs->n_fatent - 2) {	/* Update FSINFO */
				fs->free_clst -= tcl;
				fs->fsi_flag |= 1;
			}
		}
	}

	LEAVE_FF(fs, res);
}

TCHAR* f_gets (TCHAR* buff, int len, FIL* fp) {
	int nc = 0;
	TCHAR *p = buff;
	BYTE s[4];
	UINT rc;
	DWORD dc;

	len -= 1;
	while (nc < len) {
		f_read(fp, s, 1, &rc);
		if (rc != 1) break;
		dc = s[0];
		if (FF_USE_STRFUNC == 2 && dc == '\r') continue;
		*p++ = (TCHAR)dc; nc++;
		if (dc == '\n') break;
	}

	*p = 0;
	return nc ? buff : 0;
}

#define SZ_PUTC_BUF	64
#define SZ_NUM_BUF	32

typedef struct {
	FIL *fp;
	int idx, nchr;
	BYTE buf[SZ_PUTC_BUF];
} putbuff;

static void putc_bfd (putbuff* pb, TCHAR c) {
	UINT n;
	int i, nc;
	if (FF_USE_STRFUNC == 2 && c == '\n') {	 /* LF -> CRLF conversion */
		putc_bfd(pb, '\r');
	}

	i = pb->idx;			/* Write index of pb->buf[] */
	if (i < 0) return;		/* In write error? */
	nc = pb->nchr;			/* Write unit counter */


	pb->buf[i++] = (BYTE)c;

	if (i >= (int)(sizeof pb->buf) - 4) {	/* Write buffered characters to the file */
		f_write(pb->fp, pb->buf, (UINT)i, &n);
		i = (n == (UINT)i) ? 0 : -1;
	}
	pb->idx = i;
	pb->nchr = nc + 1;
}

static int putc_flush (putbuff* pb) {
	UINT nw;

	if (   pb->idx >= 0	/* Flush buffered characters to the file */
		&& f_write(pb->fp, pb->buf, (UINT)pb->idx, &nw) == FR_OK
		&& (UINT)pb->idx == nw) return pb->nchr;
	return -1;
}

static void putc_init (putbuff* pb, FIL* fp) {
	memset(pb, 0, sizeof (putbuff));
	pb->fp = fp;
}

int f_putc (TCHAR c, FIL* fp) {
	putbuff pb;
	putc_init(&pb, fp);
	putc_bfd(&pb, c);	
	return putc_flush(&pb);
}

int f_puts (const TCHAR* str, FIL* fp) {
	putbuff pb;
	putc_init(&pb, fp);
	while (*str) putc_bfd(&pb, *str++);
	return putc_flush(&pb);
}

static int ilog10 (double n) {
	int rv = 0;

	while (n >= 10) {
		if (n >= 100000) {
			n /= 100000; rv += 5;
		} else {
			n /= 10; rv++;
		}
	}
	while (n < 1) {
		if (n < 0.00001) {
			n *= 100000; rv -= 5;
		} else {
			n *= 10; rv--;
		}
	}
	return rv;
}


static double i10x (int n) {
	double rv = 1;

	while (n > 0) {
		if (n >= 5) {
			rv *= 100000; n -= 5;
		} else {
			rv *= 10; n--;
		}
	}
	while (n < 0) {
		if (n <= -5) {
			rv /= 100000; n += 5;
		} else {
			rv /= 10; n++;
		}
	}
	return rv;
}


static void ftoa (char* buf, double val, int prec, TCHAR fmt) {
	int d;
	int e = 0, m = 0;
	char sign = 0;
	double w;
	const char *er = 0;
	const char ds = FF_PRINT_FLOAT == 2 ? ',' : '.';


	if (isnan(val)) {			/* Not a number? */
		er = "NaN";
	} else {
		if (prec < 0) prec = 6;	/* Default precision? (6 fractional digits) */
		if (val < 0) {			/* Negative? */
			val = 0 - val; sign = '-';
		} else {
			sign = '+';
		}
		if (isinf(val)) {		/* Infinite? */
			er = "INF";
		} else {
			if (fmt == 'f') {	/* Decimal notation? */
				val += i10x(0 - prec) / 2;	/* Round (nearest) */
				m = ilog10(val);
				if (m < 0) m = 0;
				if (m + prec + 3 >= SZ_NUM_BUF) er = "OV";	/* Buffer overflow? */
			} else {			/* E notation */
				if (val != 0) {		/* Not a true zero? */
					val += i10x(ilog10(val) - prec) / 2;	/* Round (nearest) */
					e = ilog10(val);
					if (e > 99 || prec + 7 >= SZ_NUM_BUF) {	/* Buffer overflow or E > +99? */
						er = "OV";
					} else {
						if (e < -99) e = -99;
						val /= i10x(e);	/* Normalize */
					}
				}
			}
		}
		if (!er) {	/* Not error condition */
			if (sign == '-') *buf++ = sign;	/* Add a - if negative value */
			do {				/* Put decimal number */
				if (m == -1) *buf++ = ds;	/* Insert a decimal separator when get into fractional part */
				w = i10x(m);				/* Snip the highest digit d */
				d = (int)(val / w); val -= d * w;
				*buf++ = (char)('0' + d);	/* Put the digit */
			} while (--m >= -prec);			/* Output all digits specified by prec */
			if (fmt != 'f') {	/* Put exponent if needed */
				*buf++ = (char)fmt;
				if (e < 0) {
					e = 0 - e; *buf++ = '-';
				} else {
					*buf++ = '+';
				}
				*buf++ = (char)('0' + e / 10);
				*buf++ = (char)('0' + e % 10);
			}
		}
	}
	if (er) {	/* Error condition */
		if (sign) *buf++ = sign;		/* Add sign if needed */
		do {		/* Put error symbol */
			*buf++ = *er++;
		} while (*er);
	}
	*buf = 0;	/* Term */
}

int f_printf (
	FIL* fp,			/* Pointer to the file object */
	const TCHAR* fmt,	/* Pointer to the format string */
	...					/* Optional arguments... */
)
{
	va_list arp;
	putbuff pb;
	UINT i, j, w, f, r;
	int prec;
	QWORD v;
	TCHAR *tp;
	TCHAR tc, pad;
	TCHAR nul = 0;
	char d, str[SZ_NUM_BUF];


	putc_init(&pb, fp);

	va_start(arp, fmt);

	for (;;) {
		tc = *fmt++;
		if (tc == 0) break;			/* End of format string */
		if (tc != '%') {			/* Not an escape character (pass-through) */
			putc_bfd(&pb, tc);
			continue;
		}
		f = w = 0; pad = ' '; prec = -1;	/* Initialize parms */
		tc = *fmt++;
		if (tc == '0') {			/* Flag: '0' padded */
			pad = '0'; tc = *fmt++;
		} else if (tc == '-') {		/* Flag: Left aligned */
			f = 2; tc = *fmt++;
		}
		if (tc == '*') {			/* Minimum width from an argument */
			w = va_arg(arp, int);
			tc = *fmt++;
		} else {
			while (IsDigit(tc)) {	/* Minimum width */
				w = w * 10 + tc - '0';
				tc = *fmt++;
			}
		}
		if (tc == '.') {			/* Precision */
			tc = *fmt++;
			if (tc == '*') {		/* Precision from an argument */
				prec = va_arg(arp, int);
				tc = *fmt++;
			} else {
				prec = 0;
				while (IsDigit(tc)) {	/* Precision */
					prec = prec * 10 + tc - '0';
					tc = *fmt++;
				}
			}
		}
		if (tc == 'l') {			/* Size: long int */
			f |= 4; tc = *fmt++;
			if (tc == 'l') {		/* Size: long long int */
				f |= 8; tc = *fmt++;
			}
		}
		if (tc == 0) break;			/* End of format string */
		switch (tc) {				/* Atgument type is... */
		case 'b':					/* Unsigned binary */
			r = 2; break;

		case 'o':					/* Unsigned octal */
			r = 8; break;

		case 'd':					/* Signed decimal */
		case 'u': 					/* Unsigned decimal */
			r = 10; break;

		case 'x':					/* Unsigned hexadecimal (lower case) */
		case 'X': 					/* Unsigned hexadecimal (upper case) */
			r = 16; break;

		case 'c':					/* Character */
			putc_bfd(&pb, (TCHAR)va_arg(arp, int));
			continue;

		case 's':					/* String */
			tp = va_arg(arp, TCHAR*);	/* Get a pointer argument */
			if (!tp) tp = &nul;		/* Null ptr generates a null string */
			for (j = 0; tp[j]; j++) ;	/* j = tcslen(tp) */
			if (prec >= 0 && j > (UINT)prec) j = prec;	/* Limited length of string body */
			for ( ; !(f & 2) && j < w; j++) putc_bfd(&pb, pad);	/* Left pads */
			while (*tp && prec--) putc_bfd(&pb, *tp++);	/* Body */
			while (j++ < w) putc_bfd(&pb, ' ');			/* Right pads */
			continue;
		case 'f':					/* Floating point (decimal) */
		case 'e':					/* Floating point (e) */
		case 'E':					/* Floating point (E) */
			ftoa(str, va_arg(arp, double), prec, tc);	/* Make a floating point string */
			for (j = strlen(str); !(f & 2) && j < w; j++) putc_bfd(&pb, pad);	/* Left pads */
			for (i = 0; str[i]; putc_bfd(&pb, str[i++])) ;	/* Body */
			while (j++ < w) putc_bfd(&pb, ' ');	/* Right pads */
			continue;
		default:					/* Unknown type (pass-through) */
			putc_bfd(&pb, tc); continue;
		}

		/* Get an integer argument and put it in numeral */
		if (f & 8) {		/* long long argument? */
			v = (QWORD)va_arg(arp, long long);
		} else if (f & 4) {	/* long argument? */
			v = (tc == 'd') ? (QWORD)(long long)va_arg(arp, long) : (QWORD)va_arg(arp, unsigned long);
		} else {			/* int/short/char argument */
			v = (tc == 'd') ? (QWORD)(long long)va_arg(arp, int) : (QWORD)va_arg(arp, unsigned int);
		}
		if (tc == 'd' && (v & 0x8000000000000000)) {	/* Negative value? */
			v = 0 - v; f |= 1;
		}
		i = 0;
		do {	/* Make an integer number string */
			d = (char)(v % r); v /= r;
			if (d > 9) d += (tc == 'x') ? 0x27 : 0x07;
			str[i++] = d + '0';
		} while (v && i < SZ_NUM_BUF);
		if (f & 1) str[i++] = '-';	/* Sign */
		/* Write it */
		for (j = i; !(f & 2) && j < w; j++) {	/* Left pads */
			putc_bfd(&pb, pad);
		}
		do {				/* Body */
			putc_bfd(&pb, (TCHAR)str[--i]);
		} while (i);
		while (j++ < w) {	/* Right pads */
			putc_bfd(&pb, ' ');
		}
	}

	va_end(arp);

	return putc_flush(&pb);
}