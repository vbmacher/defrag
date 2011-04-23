/**
 * @file analyze.c
 * 
 * @brief Modul na analýzu fragmentácie disku
 *
 */
/* Modul som zaèal písa» dòa: 2.11.2006 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libintl.h>
#include <locale.h>

#include <entry.h>
#include <fat32.h>
#include <analyze.h>

/** Tabulka s dôle¾itými informáciami o ka¾dej polo¾ke v celej adresárovej ¹truktúre.
 * Jednotlivé polo¾ky tejto tabulky obsahujú: ¹tartovací klaster, èíslo adresárového klastra, èíslo
 * polo¾ky v adresárovom klastri, poèet klastrov (táto hodnota sa vyplní a¾ pri håbkovej analýze).
 * Defragmentácia pracuje na základe údajov v tejto tabulke.
 */
aTableItem* aTable = NULL;

/** Poèet hodnôt v tabulke je rovný poètu v¹etkých súborov
  * a adresárov, ktoré majú alokovaný aspoò 1 klaster
  */
unsigned long tableCount = 0;

/** Percentuálna disková fragmentácia */
float diskFragmentation;

/** Poèet pou¾itých klastrov */
unsigned long usedClusters;

/** Poèet polo¾iek v jednom adresári (premenlivá hodnota podla velkosti klastra) */
unsigned short entryCount;

/** Naplnenie tabulky aTable sa deje rekurzívne, tabulka je implementovaná
  * ako dynamické pole o max. 10000 polo¾iek (èi¾e mô¾e existova» max 10000
  * súborov a adresárov spolu na disku). Vhodnej¹ia by bola mo¾no implementácia
  * pomocou spojkového zoznamu, no pre úèely zadania myslím postaèí aj dynamické pole.
  * Táto funkcia pridá novú polo¾ku do aTable, vyplní v¹ak iba niektoré informácie v polo¾ke
  * @param startCluster ¹tartovací klaster polo¾ky
  * @param entCluster klaster adresára, ktorá odkazuje na polo¾ku
  * @param ind index, poradové èíslo polo¾ky v adresárovom klastri
  */
void an_addFile(unsigned long startCluster, unsigned long entCluster, unsigned short ind)
{
  if (aTable == NULL) {
    if ((aTable = (aTableItem *)malloc(10000 * sizeof(aTableItem))) == NULL)
      error(0, gettext("Out of memory !"));
    tableCount = 0;
  } else;
  tableCount++;
  if (tableCount >= 10000) { free(aTable); error(0,gettext("Out of memory !"));}
  aTable[tableCount-1].startCluster = startCluster;
  aTable[tableCount-1].entryCluster = entCluster;
  aTable[tableCount-1].entryIndex = ind;
}

/** Funkcia uvolní pamä» pou¾itú pre aTable
  */
void an_freeTable()
{
  free(aTable);
}

/** Funkcia zistí percentuálnu fragmentáciu jednej adresárovej
  * polo¾ky (súboru / adresára). Je súèas»ou håbkovej analýzy disku.
  * Pracuje tak, ¾e pre daný ¹tartovací klaster prejde
  * celou re»azou a zis»uje, èi klastre idú za sebou, èi¾e èi rozdiel nasledujúceho a 
  * aktuálneho klastra dáva 1. V prípade, ¾e nie, inkrementuje sa poèet fragmentovaných klastrov.
  * Poèas prechodu celej re»aze sa uchováva celkový poèet klastrov, ktorými sa pre¹lo. Po opustení
  * cyklu prechodu re»azou udáva táto premenná poèet klastrov súboru/adresára a je ulo¾ená do aTable.
  * Percentuálna fragmentácia sa vypoèíta podla vz»ahu:
  * \code
  *   (pocet frag.klastrov polozky) / (pocet vsetkych pouzitych klastrov polozky) * 100
  * \endcode
  * @param startCluster ¹tartovací klaster polo¾ky
  * @param aTIndex index v tabulke aTable - do tabulky sa zapí¹e poèet klastrov adresárovej polo¾ky
  * @return vracia fragmentáciu polo¾ky v percentách
*/
float an_getFileFragmentation(unsigned long startCluster, unsigned long aTIndex)
{
  unsigned long cluster; /* pomocny cluster */
  int fragmentCount = 0; /* pocet fragmentovanych clusterov */
  int count = 0;	 /* pocet clusterov suboru; neratam start.cluster */
  
  for (cluster = startCluster, count=0; !F32_LAST(cluster); cluster = f32_getNextCluster(cluster),count++) {
    if ((startCluster != cluster) && (startCluster+1 != cluster))
      fragmentCount++;
    startCluster = cluster;
  }
  if (F32_LAST(cluster)) count++;
  usedClusters += count;
  aTable[aTIndex].clusterCount = count;
  return (float)(((float)fragmentCount / (float)count) * 100.0);
}

/** Funkcia rekurzívne traverzuje celou adresárovou ¹truktúrou. Je súèas»ou prvej fázy analýzy disku (základná).
  * Poèas prechodu ukladá do aTable dôle¾ité informácie o adresárovej polo¾ke, ako je ¹tartovací klaster,
  * èíslo klastra adresára, ktorý obsahuje odkaz na danú polo¾ku a index v adresárovom klastri. Okrem toho volá
  * pre ka¾dú polo¾ku zistenie jej percentuálnej fragmentácie. Táto sa pripoèíta ku globálnej premennej diskFragmentation.
  * Pozor! FATka musí by» v poriadku, lebo inak mô¾e vzniknú» zacyklenie (pri krí¾ových referenciách) !
  * @param startCluster èíslo root klastra (odkial sa bude rekurzívne traverzova»)
*/
void an_scanDisk(unsigned long startCluster)
{
  unsigned short index;
  unsigned long cluster, tmpCluster;
  unsigned char tmpAttr;
  F32_DirEntry *entries;
  
  /* v chybnej FATke musim ratat s clusterCount miesto 0xffffff0 */
  if (startCluster > info.clusterCount) return;

  if ((entries = (F32_DirEntry *)malloc(entryCount * sizeof(F32_DirEntry))) == NULL)
    error(0, gettext("Out of memory !"));

  for (cluster = startCluster; !F32_LAST(cluster); cluster = f32_getNextCluster(cluster)) {
    f32_readCluster(cluster, entries);
    for (index = 0; index < entryCount; index++) {
      if (!entries[index].fileName[0]) { free(entries); return; }
      /* dalej pracujem iba s polozkami, ktore:
           1. nie su zmazane,
	   2. nie su to sloty (dlhe nazvy)
	   3. neodkazuju na rodica alebo na korenovy adresar
      */
      if ((entries[index].fileName[0] != 0xe5) && 
          entries[index].attributes != 0x0f &&
          memcmp(entries[index].fileName,".       ",8) &&
	  memcmp(entries[index].fileName,"..      ",8)) {
	tmpAttr = entries[index].attributes & 0x10;
        tmpCluster = f32_getStartCluster(entries[index]);
	if (tmpCluster != 0) {
	  /* ak je polozka podadresar, rekurzivne sa vola funkcia */
          if (tmpAttr == 0x10) {
	    /* ochrana proti zacykleniu */
	    if (tmpCluster != startCluster)
	      an_scanDisk(tmpCluster);
	  }
          /* ak je startovaci cluster chybny, ignoruje polozku */
          if (tmpCluster <= info.clusterCount) {
            an_addFile(tmpCluster,cluster,index);
            diskFragmentation += an_getFileFragmentation(tmpCluster, tableCount-1);
          }
	}
      }
    }
  }
  free(entries);
}

/** Hlavná funkcia pre analýzu disku; predtým, ako zavolá funkciu an_scanDisk vykoná
  * nejaké prípravné operácie, ako zistí poèet polo¾iek v direntry a pridá prvú hodnotu
  * do aTable - root klaster, pre ktorý tie¾ vypoèíta jeho fragmentáciu. Po ukonèení
  * rekurzívneho traverzovania adresárovej ¹truktúry vypoèíta celkovú percentuálnu
  * fragmentáciu disku vydelením premennej diskFragmentation poètom polo¾iek v aTable.
  */
int an_analyze()
{
  fprintf(output_stream, gettext("Analysing disk...\n"));

  entryCount = (bpb.BPB_SecPerClus * info.BPSector) / sizeof(F32_DirEntry);
  /* prva faza analyzy zacina root clusterom */
  aTable = NULL;
  tableCount = 0;
  
  /* tabulka obsahuje aj root cluster */
  an_addFile(bpb.BPB_RootClus, 0, 0);
  usedClusters = 0;
  diskFragmentation = an_getFileFragmentation(bpb.BPB_RootClus, 0);
  an_scanDisk(bpb.BPB_RootClus);
  diskFragmentation /= (tableCount - 1);

  fprintf(output_stream, gettext("Disk is fragmented for: %.2f%%\n"), diskFragmentation);

  /*POZOR! Neuvolnujem pamat tabulky teraz, ale AZ PO defragmentacii,
    inak by vznikla chyba "Segmentation fault" vzhladom na to, ze sa
    tabulka bude pouzivat neskor.
  */
  return 0;
}
