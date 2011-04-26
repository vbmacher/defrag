/*
 * fat32.h
 *
 * (c) Copyright 2006, vbmacher <pjakubco@gmail.com>
 * use GNU C compiler (gcc)
 *
 * Motto: Keep It Simple Stupid (KISS)
 *
 * start writing: 1.11.2006
 *
 */

#ifndef __FAT32__
#define __FAT32__

  #define FAT12 12
  #define FAT16 16
  #define FAT32 32
  
  #define F32_FREE_L   0x00000000L
  #define F32_BAD_L    0x0ffffff7L
  #define F32_LAST_L   0x0fffffffL

  #define F32_FREE(x)      ((x) == F32_FREE_L)
  #define F32_BAD(x)       ((x) == F32_BAD_L)
  #define F32_LAST(x)      (((x) >= 0xFFFFFF8L) && \
			    ((x) <= 0xFFFFFFFL))
  #define F32_RESERVED(x)  (((x) >= 0xFFFFFF0L) && \
                            ((x) <= 0xFFFFFF6L))

  typedef struct {
    unsigned char BS_jmpBoot[3];	/* The jump instruction - 3 bytes */
    unsigned char BS_OEMName[8];	/* "MSWIN4.1" */
    unsigned short BPB_BytesPerSec;	/* Number of bytes in a sector. Default 512 */
    unsigned char BPB_SecPerClus;	/* Number of sectors in cluster. It is the power of 2 */
    unsigned short BPB_RsvdSecCnt;	/* Number of reserved sectors */
    unsigned char BPB_NumFATs;		/* Number of FAT copies. Usually 2. */
    unsigned short BPB_RootEntCnt;	/* Number of files in root directory (only for FAT12/16) */
    unsigned short BPB_TotSec16;	/* Number of sectors for FAT12/16 */
    unsigned char BPB_Media;		/* Device type; diskettes 0xf0 and disks 0xf8 */
    unsigned short BPB_FATSz16;		/* For FAT32 it has to be 0 */
    unsigned short BPB_SecPerTrk;	/* Number of sectors per track (for INT 0x13) */
    unsigned short BPB_NumHeads;	/* Number of heads (for INT 0x13) */
    unsigned long BPB_HiddSec;		/* Number of hidden sectors in partition */
    unsigned long BPB_TotSec32;		/* Number of sectors in partition */
    unsigned long BPB_FATSz32;		/* Number of sectors in single FAT table */
    unsigned char BPB_ExtFlags;
    unsigned char BPB_FSVerMajor;
    unsigned short BPB_FSVerMinor;	/* Version of FAT32 partition */
    unsigned long BPB_RootClus;		/* Number of cluster where root directory is located */
    unsigned short BPB_FSInfo;		/* Number of sector where FSInfo structure is locared. */
    unsigned short BPB_BkBootSec;	/* Number of sector where the bootsector copy is located. Usually 6. */
    unsigned char BPB_Reserved2[12];
    unsigned char BS_DrvNum;		/* Number of the disk for INT13 */
    unsigned char BS_Reserved1;
    unsigned char BS_BootSig;		/* Extended flag indicating if the next 3 items are actually correct */
    unsigned char BS_VolID[4];		/* Serial number of the disk */
    unsigned char BS_VolLab[11];	/* Volume label */
    unsigned char BS_FilSysType[8];	/* It has to be "FAT32" */
    unsigned char bootSectorCode[418];
    unsigned long magicNumber;
  } __attribute__((packed)) F32_BPB;

  typedef struct {
    unsigned char fileName[8];
    unsigned char fileExt[3];
    unsigned char attributes;
    unsigned char caseFlag;
    unsigned char createTimeMS;
    unsigned short createTime;
    unsigned short createDate;
    unsigned short accessedDate;
    unsigned short startClusterH;
    unsigned short timestamp;
    unsigned short datestamp;
    unsigned short startClusterL;
    unsigned long fileSize;
  } __attribute__((packed)) F32_DirEntry;

  typedef struct {
    unsigned long FATstart;
    unsigned long FATsize;
    unsigned long clusterCount;
    unsigned char FATmirroring;
    unsigned short BPSector;
    unsigned short fSecClusters;
    unsigned long firstRootSector;	/* beginnig of root directory */
    unsigned long firstDataSector;	/* beginning of data area */
    unsigned long usedClusterCount;	/* number of used clusters */
  } __attribute__((packed)) F32_Info;

  extern F32_BPB bpb;
  extern F32_Info info;
  extern char *FATbitfield;

  int f32_mount(int);
  int f32_mounted();
  int f32_umount();
  void f32_setStartCluster(unsigned long cluster, F32_DirEntry *entry);
  unsigned long f32_getStartCluster(F32_DirEntry entry);
  unsigned long f32_getNextCluster(unsigned long cluster);
  int f32_readCluster(unsigned long, void*);
  int f32_writeCluster(unsigned long, void*);

#endif
