/**
 * @file fat32.c
 *
 * @brief Module for implementing operations with FAT32 file system.
 *
 * The module implements basic functions for working with FAT file system - they are functions such as finding up FAT type,
 * reading and writing of FAT values, reading and writing clusters and so on. The module directly uses functions of disk.c
 * module; it can be assumed as a higher-level module.
 *
 * One another interesting thing: For reading and writing values into FAT table the cache is used that contains the whole
 * sector that includes a value that it is working with. This cache is updated if the working value is out of the range.
 * It helps to faster work of the defragmenter.
 *
 */
/* The module I've started to write at day: 1.11.2006 */

#include <stdio.h>
#include <stdlib.h>
#include <libintl.h>
#include <locale.h>

#include <entry.h>
#include <disk.h>
#include <fat32.h>

/** global variable BIOS Parameter Block */
F32_BPB bpb;
/** Informations about the system (a mix of values taken from bpb plus other values, such as the beginning of data
    area, etc.) */
F32_Info info;

unsigned long *cacheFsec; /** cache for the single sector of FAT table */
static unsigned short cacheFindex = 0; /** number of cached sector (log.LBA) */

/** The function determines the FAT type and fills up the info structure; bpb must be loaded already.
  * Type of the FAT can be correctly determined (according to Microsoft) only by the number of clusters in FAT.
  * If their number is < 4085, it is FAT12, if it is < 65525 it is FAT16, otherwise FAT32.
  * This detection can be used for real FATs only; the image file can have sometimes only 1MB and there can be only 1000
  * clusters, therefore the "correct" detection fails and the result would be FAT12. Therefore I had to do the detection
  * "incorrectly" and use the detection according to the bpb.BS_FilSysType (it contains either "FAT12   ", "FAT16   ",
  * or "FAT32   "), where the FAT type detection according to this field Microsoft denies...
  * @return type of the file system (constant defined in fat32.h)
  */
int f32_determineFATType()
{
  unsigned long rootDirSectors, totalSectors, DataSector;

  rootDirSectors = ((bpb.BPB_RootEntCnt * 32) + (bpb.BPB_BytesPerSec-1)) / (bpb.BPB_BytesPerSec);
  totalSectors = (bpb.BPB_TotSec16) ? (unsigned long)bpb.BPB_TotSec16 : bpb.BPB_TotSec32;

  info.FATstart = (unsigned long)bpb.BPB_RsvdSecCnt;
  info.BPSector = bpb.BPB_BytesPerSec;
  info.fSecClusters = info.BPSector / 4;
  info.FATsize = (bpb.BPB_FATSz16) ? (unsigned long)bpb.BPB_FATSz16 : bpb.BPB_FATSz32;
  info.firstDataSector = bpb.BPB_RsvdSecCnt + bpb.BPB_NumFATs * info.FATsize;
  info.firstRootSector = info.firstDataSector + (bpb.BPB_RootClus - 2) * bpb.BPB_SecPerClus;
  info.clusterCount = (totalSectors - (info.FATstart + (info.FATsize * bpb.BPB_NumFATs) + rootDirSectors )) / bpb.BPB_SecPerClus + 1;
                         
  if (info.clusterCount < 4085 && !strcmp(bpb.BS_FilSysType,"FAT12   ")) return FAT12;
  else if (info.clusterCount < 65525L && !strcmp(bpb.BS_FilSysType,"FAT16   ")) return FAT16;
  else if (!memcmp(bpb.BS_FilSysType,"FAT32   ",8)) return FAT32;
  else
    error(0,gettext("Can't determine FAT type (label: '%s')\n"), bpb.BS_FilSysType);
}


/** Mounting the FAT32 file system, it means actually:
 *  -# to determine if the FS is really FAT32
 *  -# to get additional information about FAT (fill F32_Info structure)
 *  -# allocate memory for cache
 *
 * @return It returns 0 if there was no error.
 */
int f32_mount(int image_descriptor)
{
  int ftype;
  d_mount(image_descriptor); /* mount the disk, in order we would be able to use dist operations */

  /* Loading BPB */
  if (d_readSectors(0, (char*)&bpb, 1, 512) != 1)
    error(0,gettext("Can't read BPB !"));

  if (debug_mode) {
    fprintf(output_stream, gettext("(f32_mount) BIOS Parameter Block (BPB):\n"));
    fprintf(output_stream, gettext("(f32_mount) BPB_jmpBoot: '%s', offset: %x, size: %d\n"), bpb.BS_jmpBoot, (int)((int)(&bpb.BS_jmpBoot) - (int)(&bpb)), sizeof(bpb.BS_jmpBoot));
    fprintf(output_stream, gettext("(f32_mount) BS_OEMName: '%s', offset: %x, size: %d\n"), bpb.BS_OEMName, (int)((int)(&bpb.BS_OEMName) - (int)(&bpb)), sizeof(bpb.BS_OEMName));
    fprintf(output_stream, gettext("(f32_mount) BPB_BytesPerSec: %d, offset: %x, size: %d\n"), (short)bpb.BPB_BytesPerSec, (int)((int)(&bpb.BPB_BytesPerSec) - (int)(&bpb)), sizeof(bpb.BPB_BytesPerSec));
    fprintf(output_stream, gettext("(f32_mount) BPB_SecPerClus: %u, offset: %x, size: %d\n"), bpb.BPB_SecPerClus, (int)((int)(&bpb.BPB_SecPerClus) - (int)(&bpb)), sizeof(bpb.BPB_SecPerClus));
    fprintf(output_stream, gettext("(f32_mount) BPB_RsvdSecCnt: %d, offset: %x, size: %d\n"), bpb.BPB_RsvdSecCnt, (int)((int)(&bpb.BPB_RsvdSecCnt) - (int)(&bpb)), sizeof(bpb.BPB_RsvdSecCnt));
    fprintf(output_stream, gettext("(f32_mount) BPB_NumFATs: %d, offset: %x, size: %d\n"), bpb.BPB_NumFATs, (int)((int)(&bpb.BPB_NumFATs) - (int)(&bpb)), sizeof(bpb.BPB_NumFATs));
    fprintf(output_stream, gettext("(f32_mount) BPB_RootEntCnt: %d, offset: %x, size: %d\n"), bpb.BPB_RootEntCnt, (int)((int)(&bpb.BPB_RootEntCnt) - (int)(&bpb)), sizeof(bpb.BPB_RootEntCnt));
    fprintf(output_stream, gettext("(f32_mount) BPB_TotSec16: %d, offset: %x, size: %d\n"), bpb.BPB_TotSec16, (int)((int)(&bpb.BPB_TotSec16) - (int)(&bpb)), sizeof(bpb.BPB_TotSec16));
    fprintf(output_stream, gettext("(f32_mount) BPB_Media: %x, offset: %x, size: %d\n"), bpb.BPB_Media, (int)((int)(&bpb.BPB_Media) - (int)(&bpb)), sizeof(bpb.BPB_Media));
    fprintf(output_stream, gettext("(f32_mount) BPB_FATSz16: %d, offset: %x, size: %d\n"), bpb.BPB_FATSz16, (int)((int)(&bpb.BPB_FATSz16) - (int)(&bpb)), sizeof(bpb.BPB_FATSz16));
    fprintf(output_stream, gettext("(f32_mount) BPB_SePerTrk: %d, offset: %x, size: %d\n"), bpb.BPB_SecPerTrk, (int)((int)(&bpb.BPB_SecPerTrk) - (int)(&bpb)), sizeof(bpb.BPB_SecPerTrk));
    fprintf(output_stream, gettext("(f32_mount) BPB_NumHeads: %d, offset: %x, size: %d\n"), bpb.BPB_NumHeads, (int)((int)(&bpb.BPB_NumHeads) - (int)(&bpb)), sizeof(bpb.BPB_NumHeads));
    fprintf(output_stream, gettext("(f32_mount) BPB_HiddSec: %d, offset: %x, size: %d\n"), bpb.BPB_HiddSec, (int)((int)(&bpb.BPB_HiddSec) - (int)(&bpb)), sizeof(bpb.BPB_HiddSec));
    fprintf(output_stream, gettext("(f32_mount) BPB_TotSec32: %ld, offset: %x, size: %d\n"), bpb.BPB_TotSec32, (int)((int)(&bpb.BPB_TotSec32) - (int)(&bpb)), sizeof(bpb.BPB_TotSec32));
    fprintf(output_stream, gettext("(f32_mount) BPB_FATSz32: %ld, offset: %x, size: %d\n"), bpb.BPB_FATSz32, (int)((int)(&bpb.BPB_FATSz32) - (int)(&bpb)), sizeof(bpb.BPB_FATSz32));
    fprintf(output_stream, gettext("(f32_mount) BPB_ExtFlags: %d, offset: %x, size: %d\n"), bpb.BPB_ExtFlags, (int)((int)(&bpb.BPB_ExtFlags) - (int)(&bpb)), sizeof(bpb.BPB_ExtFlags));
    fprintf(output_stream, gettext("(f32_mount) BPB_FSVer major: %d, offset: %x, size: %d\n"), bpb.BPB_FSVerMajor, (int)((int)(&bpb.BPB_FSVerMajor) - (int)(&bpb)), sizeof(bpb.BPB_FSVerMajor));
    fprintf(output_stream, gettext("(f32_mount) BPB_FSVer minor: %d, offset: %x, size: %d\n"), bpb.BPB_FSVerMinor, (int)((int)(&bpb.BPB_FSVerMinor) - (int)(&bpb)), sizeof(bpb.BPB_FSVerMinor));
    fprintf(output_stream, gettext("(f32_mount) BPB_RootClus: %ld, offset: %x, size: %d\n"), bpb.BPB_RootClus, (int)((int)(&bpb.BPB_RootClus) - (int)(&bpb)), sizeof(bpb.BPB_RootClus));
    fprintf(output_stream, gettext("(f32_mount) BPB_FSInfo: %d, offset: %x, size: %d\n"), bpb.BPB_FSInfo, (int)((int)(&bpb.BPB_FSInfo) - (int)(&bpb)), sizeof(bpb.BPB_FSInfo));
    fprintf(output_stream, gettext("(f32_mount) BPB_BkBootSec: %d, offset: %x, size: %d\n"), bpb.BPB_BkBootSec, (int)((int)(&bpb.BPB_BkBootSec) - (int)(&bpb)), sizeof(bpb.BPB_BkBootSec));
    fprintf(output_stream, gettext("(f32_mount) BPB_Reserved: '%s', offset: %x, size: %d\n"), bpb.BPB_Reserved2, (int)((int)(&bpb.BPB_Reserved2) - (int)(&bpb)), sizeof(bpb.BPB_Reserved2));
    fprintf(output_stream, gettext("(f32_mount) BS_DrvNum: %d, offset: %x, size: %d\n"), bpb.BS_DrvNum, (int)((int)(&bpb.BS_DrvNum) - (int)(&bpb)), sizeof(bpb.BS_DrvNum));
    fprintf(output_stream, gettext("(f32_mount) BS_Reserved1: %d, offset: %x, size: %d\n"), bpb.BS_Reserved1, (int)((int)(&bpb.BS_Reserved1) - (int)(&bpb)), sizeof(bpb.BS_Reserved1));
    fprintf(output_stream, gettext("(f32_mount) BS_BootSig: %d, offset: %x, size: %d\n"), bpb.BS_BootSig, (int)((int)(&bpb.BS_BootSig) - (int)(&bpb)), sizeof(bpb.BS_BootSig));
    fprintf(output_stream, gettext("(f32_mount) BS_VolID: %ld, offset: %x, size: %d\n"), bpb.BS_VolID, (int)((int)(&bpb.BS_VolID) - (int)(&bpb)), sizeof(bpb.BS_VolID));
    fprintf(output_stream, gettext("(f32_mount) BS_VolLab: '%s', offset: %x, size: %d\n"), bpb.BS_VolLab, (int)((int)(&bpb.BS_VolLab) - (int)(&bpb)), sizeof(bpb.BS_VolLab));
    fprintf(output_stream, gettext("(f32_mount) BS_FilSysType: '%s', offset: %x, size: %d\n"), bpb.BS_FilSysType, (int)((int)(&bpb.BS_FilSysType) - (int)(&bpb)), sizeof(bpb.BS_FilSysType));
  }
  /* check if it is FAT32 (wrong according to Microsoft) */
  if ((ftype = f32_determineFATType()) != FAT32)
    error(0,gettext("File system on image isn't FAT32, but FAT%d !"),ftype);
  
  /* finds out if the FAT is mirrorred */
  if (!(bpb.BPB_ExtFlags & 0x80)) info.FATmirroring = 1;
  else {
    info.FATmirroring = 0;
    info.FATstart += (bpb.BPB_ExtFlags & 0x0F) * info.FATsize; /* if not, it sets up to the active FAT */
  }
  if ((cacheFsec = (unsigned long *)malloc(sizeof(unsigned long) * info.fSecClusters)) == NULL)
    error(0,gettext("Out of memory !"));

  return 0;
}

/** Function determines if the FAT32 is mounted; it is if the FATstart is not null and when disk is mounted.
 *  @return 1 if the FAT32 is mounted, 0 otherwise.
 */
int f32_mounted()
{
  if (info.FATstart && d_mounted()) return 1;
  else return 0;
}

/** Function un-mounts the FAT, i.e. zero-es FATstart, frees cache memory and un-mounts the disk */
int f32_umount()
{
  info.FATstart = 0;
  free(cacheFsec);
  d_umount();
  return 0;
}

/** The function reads the value of a cluster in the FAT table (returns it in value variable), it uses cache of the FAT.
 *  There is implemented only FAT32 version, i.e. the function is not usable for FAT12/16.
 *  @param cluster number of cluster that value will be read from FAT
 *  @param value[output] into this pointer the read value will be stored
 *  @return It returns 0 if there was no error.
 */
int f32_readFAT(unsigned long cluster, unsigned long *value)
{
  unsigned long logicalLBA;
  unsigned short index;
  unsigned long val;
  
  if (!f32_mounted()) return 1;
  
  logicalLBA = info.FATstart + ((cluster * 4) / info.BPSector); /* FAT sector that contains the cluster */
  index = (cluster % info.fSecClusters); /* index in the sector of FAT table */
  if (logicalLBA > (info.FATstart + info.FATsize))
    error(0,gettext("Trying to read cluster > max !"));

  if (cacheFindex != logicalLBA) {
    if (d_readSectors(logicalLBA, cacheFsec, 1, info.BPSector) != 1) error(0,gettext("Can't read from image (pos.:0x%lx)!"), logicalLBA);
    else cacheFindex = logicalLBA;
  }

  val = cacheFsec[index] & 0x0fffffff;
  *value = val;
  return 0;
}

/** The function writes the value of a cluster into FAT table, it uses cache of the FAT.
 *  It is implemented only FAT32 version, i.e. the function is not usable for FAT12/16. If FAT mirrorring
 *  is turned on, the value is also written into the second FAT copy (there are assumed only two copies).
 *  @param cluster number of a cluster
 *  @param value the data that will be written into the FAT
 *  @return Returns 0 if there was no error.
 */
int f32_writeFAT(unsigned long cluster, unsigned long value)
{
  unsigned long logicalLBA;
  unsigned short index;

  if (!f32_mounted()) return 1;
    
  value &= 0x0fffffff;
  logicalLBA = info.FATstart + ((cluster * 4) / info.BPSector);
  index = (cluster % info.fSecClusters); /* index in FAT table sector */
  if (logicalLBA > (info.FATstart + info.FATsize))
    error(0,gettext("Trying to write cluster > max !"));
  
  if (cacheFindex != logicalLBA) {
    if (d_readSectors(logicalLBA, cacheFsec, 1, info.BPSector) != 1) error(0,gettext("Can't read from image (pos.:0x%lx) !"), logicalLBA);
    else cacheFindex = logicalLBA;
  }
  cacheFsec[index] = cacheFsec[index] & 0xf0000000;
  cacheFsec[index] = cacheFsec[index] | value;

  if (d_writeSectors(logicalLBA, cacheFsec, 1, info.BPSector) != 1)
    error(0,gettext("Can't write to image (pos.:0x%lx) !"), logicalLBA);

  if (info.FATmirroring)
    /* there is assumed only 2 copies of FAT */
    if (d_writeSectors(logicalLBA + info.FATsize, cacheFsec, 1, info.BPSector) != 1)
      return 1;
  
  return 0;
}

/** The function computes starting cluster from the dir entry, (note: For FAT12/16 this does not need to be computed, because there is used maximum 16-bit value. Within FAT32 the starting cluster is split into a structure of two 16-bit items and they need to be "concatenated" in appropriate way).
  * @param entry structure of dir entry
  * @return computed starting cluster
  */
unsigned long f32_getStartCluster(F32_DirEntry entry)
{
  return ((unsigned long)entry.startClusterL + ((unsigned long)entry.startClusterH << 16));
}

/** The function sets the starting cluster into the dir entry
 *  @param cluster number of a cluster that will be used as a new value of the starting cluster
 *  @param entry[output] pointer to structure of the dir entry into that the new value will be written of the starting cluster
 */
void f32_setStartCluster(unsigned long cluster, F32_DirEntry *entry)
{
  (*entry).startClusterH = (unsigned short)((unsigned long)(cluster & 0xffff0000) >> 16);
  (*entry).startClusterL = (unsigned short)(cluster & 0xffff);
}

/** The function finds out the next cluster in the chain (the follower of the predecessor)
 *  @param cluster number of the cluster (predecessor)
 *  @return returns a value of the predecessor cluster from FAT
 */
unsigned long f32_getNextCluster(unsigned long cluster)
{
  unsigned long val;
  if (f32_readFAT(cluster, &val))
    error(0,gettext("Can't read from FAT !"));
  return val;
}

/** The function reads all cluster into memory (data, not the FAT value). Needed information it takes from already
 *  filled F32_info structure (e.g. sectors per cluster, etc.).
 *  @param cluster number of cluster that should be read
 *  @param buffer[output] pointer to the buffer where the data would be pushed
 *  @return In a case of error, it returns 1; 0 otherwise.
 */
int f32_readCluster(unsigned long cluster, void *buffer)
{
  unsigned long logicalLBA;
  if (!f32_mounted()) return 1;
  
  if (cluster > info.clusterCount)
    error(0,gettext("Trying to read cluster > max !"));

  logicalLBA = info.firstDataSector + (cluster - 2) * bpb.BPB_SecPerClus;
  if (d_readSectors(logicalLBA, buffer, bpb.BPB_SecPerClus, info.BPSector) != bpb.BPB_SecPerClus)
    return 1;
  else
    return 0;
}

/** The function writes all the cluster from the memory into the disk image (data, not FAT value). Required information it takes from already filled structure F32_info (e.g. sectors per cluster, etc.).
 *  @param cluster number of cluster for writing
 *  @param buffer pointer to the buffer from what the data will be read
 *  @return In a case of error, it returns 1; 0 otherwise.
 */
int f32_writeCluster(unsigned long cluster, void *buffer)
{
  unsigned long logicalLBA;
  if (!f32_mounted()) return 1;
 
  if (cluster > info.clusterCount)
    error(0,gettext("Trying to write cluster > max !"));
  
  logicalLBA = info.firstDataSector + (cluster - 2) * bpb.BPB_SecPerClus;
  if (d_writeSectors(logicalLBA, buffer, bpb.BPB_SecPerClus, info.BPSector) != bpb.BPB_SecPerClus)
    return 1;
  else
    return 0;
}
