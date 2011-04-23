/**
 * @file fat32.c
 *
 * @brief Modul implementuje operácie so súborovým systémom FAT32
 *
 * Modul implementuje základné funkcie pre prácu so súborovým systémom FAT - sú tu funkcie ako zistenie typu FAT,
 * preèítanie a zápis hodnôt do FAT, preèítanie a zápis klastrov a podobne. Modul priamo vyu¾íva funkcie modulu disk.c,
 * dá sa poveda», ¾e je akýmsi vy¹¹ím levelom.
 *
 * E¹te jedna zaujímavos»: Pre èítanie a zápis hodnôt do FAT tabulky je pou¾itá cache, ktorý obsahuje celý sektor, v
 * ktorom sa nachádza hodnota, s ktorou sa pracuje. Táto cache sa aktualizuje, ak je pracovná hodnota mimo jej rozsahu.
 * Pomáha tak k urýchleniu práce samotného procesu defragmentácie.
 *
 */
/* Modul som zaèal písa» dòa: 1.11.2006 */

#include <stdio.h>
#include <stdlib.h>
#include <libintl.h>
#include <locale.h>

#include <entry.h>
#include <disk.h>
#include <fat32.h>

/** globálna premenná BIOS Parameter Block */
F32_BPB bpb;
/** informácie o systéme (výber hodnôt z bpb plus ïal¹ie hodnoty, ako zaèiatok dátovej oblasti, apod. ) */
F32_Info info;

unsigned long *cacheFsec; /** cache pre jeden sektor FAT tabulky */
static unsigned short cacheFindex = 0; /** èíslo cachovaného sektora (log.LBA) */

/** Funkcia zistí typ FAT a naplní ¹truktúru info; bpb musí u¾ by» naèítaný.
  * Typ FAT sa dá korektne zisti» (podla Microsoftu) jedine podla
  * poètu klastrov vo FAT. Ak je ich poèet < 4085, ide o FAT12, ak je < 65525 ide o FAT16, inak o FAT32.
  * Tato determinácia typu FAT je v¹ak urèená pre skutoèné FAT-ky; keï¾e obraz má niekedy len 1MB a tam
  * sa vojde asi okolo 1000 klastrov, "korektná" determinácia zlyháva a výsledok by bol FAT12. Preto som musel
  * determináciu urobi» "nekorektnou" a pou¾i» zistenie podla bpb.BS_FilSysType (obsahuje buï "FAT12   ", "FAT16   ",
  * alebo "FAT32   "), kde zistenie typu FAT podla tohto parametra pova¾uje Microsoft za neprípustné...
  * @return Funkcia vracia typ súborového systému (kon¹tanty definované vo fat32.h)
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


/** Namontovanie súborového systému FAT32, znamená to vlastne:
 *  -# zisti», èi je typ FS naozaj FAT32
 *  -# zisti» dodatoèné informácie o FATke (naplni» ¹truktúru F32_Info)
 *  -# alokova» pamä» pre cache
 *
 * @return ak nebola ¾iadna chyba, vracia 0.
 */
int f32_mount(int image_descriptor)
{
  int ftype;
  d_mount(image_descriptor); /* namontujem disk, aby bolo mozne pouzivat diskove operacie */

  /* Nacitanie BPB */
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
  /* kontrola, ci je to FAT32 (podla Microsoftu nespravna) */
  if ((ftype = f32_determineFATType()) != FAT32)
    error(0,gettext("File system on image isn't FAT32, but FAT%d !"),ftype);
  
  /* zisti, ci je FAT zrkadlena */
  if (!(bpb.BPB_ExtFlags & 0x80)) info.FATmirroring = 1;
  else {
    info.FATmirroring = 0;
    info.FATstart += (bpb.BPB_ExtFlags & 0x0F) * info.FATsize; /* ak nie, nastavi sa na aktivnu FAT */
  }
  if ((cacheFsec = (unsigned long *)malloc(sizeof(unsigned long) * info.fSecClusters)) == NULL)
    error(0,gettext("Out of memory !"));

  return 0;
}

/** Funkcia zistí, èi je FAT32 namontovaný; je to vtedy, keï nie je nulová FATstart a keï je namontovaný disk
 *  @return 1 ak je namontovaný FAT32, inak 0
 */
int f32_mounted()
{
  if (info.FATstart && d_mounted()) return 1;
  else return 0;
}

/** Funkcia odmontuje FAT, èi¾e vynuluje FATstart, uvolní pamä» cache a odmontuje disk */
int f32_umount()
{
  info.FATstart = 0;
  free(cacheFsec);
  d_umount();
  return 0;
}

/** Funkcia preèíta hodnotu klastra vo FAT tabulke (vráti ju vo value), vyu¾íva sa cache FATky.
 *  Je tu implementovaná iba F32 verzia, èi¾e funkcia nie je pou¾itelná pre FAT12/16.
 *  @param cluster èíslo klastra, ktorého hodnota sa z FAT preèíta
 *  @param value[výstup] do tohto smerníka sa ulo¾í preèítaná hodnota
 *  @return v prípade, ak nenastala chyba, fukncia vráti 0.
 */
int f32_readFAT(unsigned long cluster, unsigned long *value)
{
  unsigned long logicalLBA;
  unsigned short index;
  unsigned long val;
  
  if (!f32_mounted()) return 1;
  
  logicalLBA = info.FATstart + ((cluster * 4) / info.BPSector); /* sektor FAT, ktory obsahuje cluster */
  index = (cluster % info.fSecClusters); /* index v sektore FAT tabulky */
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

/** Funkcia zapí¹e hodnotu klastra do FAT tabulky, vyu¾íva sa cache FATky.
 *  Je tu implementovaná iba F32 verzia, èi¾e funkcia nie je pou¾itelná pre FAT12/16. V prípade, ¾e je zapnuté
 *  zrkadlenie FATky, je hodnota zapísaná aj do druhej kópie (predpokladajú sa iba dve kópie).
 *  @param cluster èíslo klastra, do ktorého sa zapí¹e hodnota vo FAT
 *  @param value urèuje hodnotu, ktorá bude zapísaná do FAT
 *  @return v prípade, ak nenastala chyba, fukncia vráti 0.
 */
int f32_writeFAT(unsigned long cluster, unsigned long value)
{
  unsigned long logicalLBA;
  unsigned short index;

  if (!f32_mounted()) return 1;
    
  value &= 0x0fffffff;
  logicalLBA = info.FATstart + ((cluster * 4) / info.BPSector);
  index = (cluster % info.fSecClusters); /* index v sektore FAT tabulky */
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
    /* predpokladam, ze su iba 2 kopie FAT */
    if (d_writeSectors(logicalLBA + info.FATsize, cacheFsec, 1, info.BPSector) != 1)
      return 1;
  
  return 0;
}

/** Funkcia vypoèíta ¹tartovací klaster z dir entry, (pozn.: Pre FAT12/16 netreba poèíta», preto¾e je vyu¾ívaná max.
  * 16 bitová hodnota, prièom vo FAT32 je ¹tartovací klaster rozdelený v ¹truktúre do dvoch 16 bitových polo¾iek a
  * je potrebné ich vhodne "spoji»").
  * @param entry ¹truktúra dir polo¾ky
  * @return vypoèítaný ¹tartovací klaster
  */
unsigned long f32_getStartCluster(F32_DirEntry entry)
{
  return ((unsigned long)entry.startClusterL + ((unsigned long)entry.startClusterH << 16));
}

/** Funkcia nastaví ¹tartovací klaster do dir entry
 *  @param cluster èíslo klastra, ktoré sa pou¾ije ako nová hodnota ¹tart. klastra
 *  @param entry[výstup] smerník na ¹truktúru dir polo¾ky, do ktorej sa zapí¹e nová hodnota ¹tart.klastra
 */
void f32_setStartCluster(unsigned long cluster, F32_DirEntry *entry)
{
  (*entry).startClusterH = (unsigned short)((unsigned long)(cluster & 0xffff0000) >> 16);
  (*entry).startClusterL = (unsigned short)(cluster & 0xffff);
}

/** Funkcia nájde ïal¹í klaster v re»azi (nasledovníka predchodcu)
 *  @param cluster èíslo klastra (predchodca)
 *  @return vráti hodnotu predchudcu z FAT
 */
unsigned long f32_getNextCluster(unsigned long cluster)
{
  unsigned long val;
  if (f32_readFAT(cluster, &val))
    error(0,gettext("Can't read from FAT !"));
  return val;
}

/** Funkcia naèíta celý klaster do pamäte (dáta, nie hodnotu FAT), potrebné údaje berie z u¾ naplnenej ¹truktúry F32_info
 *  (ako napr. sectors per cluster, atï).
 *  @param cluster èíslo klastra, ktorý sa má naèíta»
 *  @param buffer[výstup] smerník na buffer, kde sa cluster naèíta
 *  @return v prípade chyby vráti 1, inak 0
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

/* zapise cely cluster do pamate */
/** Funkcia zapí¹e celý klaster z pamäte do obrazu (dáta, nie hodnotu FAT), potrebné údaje berie z u¾ naplnenej
 *  ¹truktúry F32_info (ako napr. sectors per cluster, atï).
 *  @param cluster èíslo klastra, do ktorého sa bude zapisova»
 *  @param buffer smerník na buffer, z ktorého sa budú èíta» údaje
 *  @return v prípade chyby vráti 1, inak 0
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
