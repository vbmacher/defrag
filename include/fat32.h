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
    unsigned char BS_jmpBoot[3];	/* Instrukcia skoku - 3 byty */
    unsigned char BS_OEMName[8];	/* "MSWIN4.1" */
    unsigned short BPB_BytesPerSec;	/* Pocet bytov v sektore. Standardne 512 */
    unsigned char BPB_SecPerClus;	/* Pocet sektorov v clusteri. Musi byt mocnina 2 */
    unsigned short BPB_RsvdSecCnt;	/* Pocet rezervovanych sektorov */
    unsigned char BPB_NumFATs;		/* Pocet kopii FAT tabuliek. Standardne 2 */
    unsigned short BPB_RootEntCnt;	/* Pocet suborov v korenovom adresari (iba pre FAT12/16) */
    unsigned short BPB_TotSec16;	/* Pocet sektorov pre FAT12/16 */
    unsigned char BPB_Media;		/* Typ zariadenia; diskety 0xf0 a disky 0xf8 */
    unsigned short BPB_FATSz16;		/* Pre FAT32 musi byt 0 */
    unsigned short BPB_SecPerTrk;	/* Pocet sektorov na stopu (pre INT 0x13) */
    unsigned short BPB_NumHeads;	/* Pocet hlav (pre INT 0x13) */
    unsigned long BPB_HiddSec;		/* Pocet skrytych sektorov v particii */
    unsigned long BPB_TotSec32;		/* Pocet sektorov v particii */
    unsigned long BPB_FATSz32;		/* Pocet sektorov v jednej FAT tabulke */
    unsigned char BPB_ExtFlags;
    unsigned char BPB_FSVerMajor;
    unsigned short BPB_FSVerMinor;	/* Verzia FAT32 particie */
    unsigned long BPB_RootClus;		/* Cislo clustera, kde sa nachadza korenovy adresar */
    unsigned short BPB_FSInfo;		/* Cislo sektora, kde sa nachadza struktura FSInfo */
    unsigned short BPB_BkBootSec;	/* Cislo sektora, kde sa nachadza kopia bootsektora. Obycajne 6 */
    unsigned char BPB_Reserved2[12];
    unsigned char BS_DrvNum;		/* Cislo disku pre INT13 */
    unsigned char BS_Reserved1;
    unsigned char BS_BootSig;		/* Rozsireny priznak indikujuci, ci sa dalej nachadzaju nasledujuce 3 polozky */
    unsigned char BS_VolID[4];		/* Seriove cislo disku */
    unsigned char BS_VolLab[11];	/* Volume label */
    unsigned char BS_FilSysType[8];	/* Musi byt "FAT32" */
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
    unsigned long firstRootSector;	/* zaciatok korenoveho adresara */
    unsigned long firstDataSector;	/* zaciatok datovej oblasti */
    unsigned long usedClusterCount;	/* pocet pouzitych clusterov */
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
