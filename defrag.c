/**
 * @file defrag.c
 * 
 * @brief Modul vykonáva defragmentáciu disku
 *
 */
/* Modul som zaèal písa» dòa: 3.11.2006 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libintl.h>
#include <locale.h>

#include <entry.h>
#include <analyze.h>
#include <fat32.h>


/** pomocný buffer pre adresárové polo¾ky (ak sa updatuje direntry) */
F32_DirEntry *entries = NULL;
unsigned short entryCount;

/** 1. cache klastra */
unsigned char *cacheCluster1 = NULL;
/** 2. cache klastra */
unsigned char *cacheCluster2 = NULL;

/** poradie klastra, ktorý sa defragmentuje (pou¾íva sa pri výpoète % )*/
unsigned long clusterIndex;

/** Funkcia nájde rodièa klastra z FAT
 *  Ak má parameter hodnotu 0, rodiè sa nehladá. V opaènom prípade sa zaène prehladáva»
 *  celá FAT, èi nejaký klaster odkazuje na klaster daný parametrom.
 * @param cluster èíslo klastra, ktorého rodiè sa hladá
 * @return funkcia vráti èíslo klastra rodièa, alebo 0 v prípade, ¾e sa nena¹iel.
 */
unsigned long def_findParent(unsigned long cluster)
{
  unsigned long i, val;
  if (!cluster) return 0;
  for (i = 2; i <= info.clusterCount; i++) {
    if (f32_readFAT(i, &val)) error(0,gettext("Can't read from FAT !"));
    if (val == cluster) return i;
  }
  return 0;
}

/**
 * Funkcia zistí, èi je klaster ¹tartovací (prehladá sa tabulka aTable)
 * @param cluster testovací klaster
 * @param index výstupná premenná bude obsahova» inkrementované poradie v aTable, ak bol
 *              klaster ¹tartovací. Ak nie je, výstupom bude 0.
 * @return funkcia vráti 1, ak klaster je ¹tartovací a 0 ak nie
*/
int def_isStarting(unsigned long cluster, unsigned long *index)
{
  unsigned long i;
  for (i = 0; i < tableCount; i++)
    if (aTable[i].startCluster == cluster) {
      *index = (i+1);
      return 1;
    }
  *index = 0;
  return 0;
}

/** Funkcia nájde prvý pou¾itelný klaster (výstup do outCluster) a jeho hodnotu (výstup do outValue)
  * Pou¾itelný klaster je taký, ktorý sa dá prepísa» (je v "intervale platných klastrov") a nie je chybný
  * @param beginCluster odkial sa má hlada»
  * @param outCluster výstup - nájdený pou¾itelný klaster
  * @param outValue výstup - hodnota nájdeného klastra
  * @return v prípade chyby vracia funkcia 1, inak 0.
  */
int def_findFirstUsable(unsigned long beginCluster, unsigned long *outCluster, unsigned long *outValue)
{
  unsigned long cluster;
  unsigned long value = 0;
  char found = 0;
 
  if (debug_mode) 
    fprintf(output_stream,gettext("(def_findFirstUsable) searching for first usable cluster (from %lx)..."), beginCluster);

  for (cluster = beginCluster; cluster <= info.clusterCount; cluster++) {
    if (f32_readFAT(cluster, &value)) error(0,gettext("Can't read from FAT !"));
    if (value != F32_BAD_L) {
      found = 1;
      break;
    }
  }
  if (!found) {
    if (debug_mode)
      fprintf(output_stream,gettext("not found!\n"));
    return 1;
  }
  if (debug_mode)
    fprintf(output_stream,gettext("found (%lx)\n"), cluster);
  *outCluster = cluster;
  *outValue = value;
  return 0;
}

/**
 * Je to najdôle¾itej¹ia funkcia - vymení 2 navzájom klastre (ako vo FAT, tak aj v aTable a aj fyzicky).
 * Pri výmene dvoch klastrov postupujem nasledovne:
 * 
 * -# zistenie, èi sú klastre ¹tartovacie. Ak áno, update dir entry. V prípade,
 *    ¾e niektorý z klastrov je root, tak sa treba patriène postara» aj o neho,
 *    èi¾e je potrebné updatova» bpb (FAT32 mô¾e ma» root klaster hocikde).
 * -# update FAT - výmena hodnôt vo FAT tabulke.
 *    Ak bol niektorý/obidva klastre èas»ou re»aze, treba updatova» jeho/ich
 *    rodièov vo FAT - èi¾e vymeni» rodièovské odkazy. V prípade, ¾e bude chybná
 *    FAT a nejaký klaster odkazuje na volný klaster, nastane velmi vá¾na chyba,
 *    ktorá vedie k ïal¹iemu hor¹iemu po¹kodeniu FAT, lebo sa nenájde rodiè (podla nasl.
 *    podmienok) a vznikne tak krí¾ová referencia. Preto je tu mo¾né pou¾i» dve mo¾nosti:
 *    -# predpoklada», ¾e je FAT v poriadku a uvies» korektnú podmienku na nájdenie rodièa
 *    -# pou¾i» poloviènú (ale postaèujúcu) podmienku a nepotrebova», aby klastre neodkazovali na 0.
 *    .
 *    Korektná podmienka na nájdenie rodièa: Ak klaster nie je ¹tartovací a jeho hodnota nie je 0 hladaj rodièa<br/>
 *    Polovièná podimenka na nájdenie rodièa: Ak klaster nie je ¹tartovací hladaj rodièa
 *    Vo svojom rie¹ení som pou¾il poloviènú podmienku, preto som musel upravi» aj
 *    funkciu pre nájdenie rodièa.
 *    Po týchto operáciách sa navzájom vymenia hodnoty klastrov vo FAT tabulke - tu
 *    v¹ak musím da» pozor na zacyklenie, ako vyplýva z nasledujúceho príkladu:
 *    \code
 *      defragmentovane:  A -> A -> A -> N -> N -> N -> N
 *      retaz          : ...->213->214->2c4->215->980->...
 *    \endcode
 *    a chcem vymeni» 2c4<->215.
 *    Tak skúsim normálnu výmenu:
 *    - 2c4 nie je ¹tartovací, jeho rodièom je 214 (èi¾e 214 ukazuje na 2c4)
 *    - 215 nie je ¹tartovací, jeho rodièom je 2c4 (èi¾e 2c4 ukazuje na 215)
 *    .
 *    update rodièov
 *      - 214 bude ukazova» na 215
 *      - 2c4 bude ukazova» na 980
 *      .
 *    a výmena FAT hodnôt: 
 *    - klaster 2c4 pôvodne ukazujúci na 215 bude ukazova» na 980 (na hodnotu, na ktorú ukazuje klaster 215)
 *    - klaster 215 pôvodne ukazujúci na 980 bude ukazova» na 215 (na hodnotu, na ktorú ukazuje klaster 2c4)
 *    .
 *    Èi¾e po takejto výmene bude nová re»az vyzera» takto:
 *    \code
 *       214->215->215->215->... (zacyklenie)
 *       2c4->980->...
 *    \endcode
 *    a preto musím urobi» opatrenie. V inom prípade sa pou¾ije táto klasická výmena.
 * -# update hodnôt v aTable
 * -# výmena samotných dát klastrov
 *
 * @param cluster1 èíslo prvého klastra
 * @param cluster2 èíslo druhého klastra
 * @return funkcia vráti 0, ak nebola chyba.
 */
int def_switchClusters(unsigned long cluster1, unsigned long cluster2)
{
  unsigned long isStarting1, isStarting2; /* ci su clustre startovacie. 
                                             Ak ano, vyjadruju (index + 1)
					     v tabulke aTable
					  */
  unsigned long tmpVal1, tmpVal2;
  unsigned long clus1val, clus2val;

  if (debug_mode)
    fprintf(output_stream,gettext("  switching %lx <-> %lx\n"), cluster1, cluster2);

  if (cluster1 == cluster2) return 0;
  /* 1. zistenie, ci su clustre startovacie. Ak ano, update dir entry. */
  /* pozor na root ! moze sa jednat aj o neho */
    def_isStarting(cluster1, &isStarting1);
    def_isStarting(cluster2, &isStarting2);
    
    if (isStarting1) {
      if (!aTable[isStarting1-1].entryCluster) {
        /* jedna sa o Root */
        if (debug_mode)
          fprintf(output_stream, gettext("  cluster1(%lx) is root cluster\n"), cluster1);
	bpb.BPB_RootClus = cluster2;
	d_writeSectors(0, (char*)&bpb, 1);
      } else {
        f32_readCluster(aTable[isStarting1-1].entryCluster, entries);
        if (debug_mode) {
          fprintf(output_stream, gettext("  cluster1(%lx) is starting; dir entry cluster %lx (aTable index %lu)\n"), cluster1, aTable[isStarting1-1].entryCluster, isStarting1-1);
          fprintf(output_stream, gettext("    redefining starting cluster1(%lx) %lx (equal to cluster1) to %lx\n"), cluster1, f32_getStartCluster(entries[aTable[isStarting1-1].entryIndex]), cluster2);
	}
        f32_setStartCluster(cluster2,&entries[aTable[isStarting1-1].entryIndex]);
        f32_writeCluster(aTable[isStarting1-1].entryCluster, entries);
      }
    }
    if (isStarting2) {
      if (!aTable[isStarting2-1].entryCluster) {
        if (debug_mode)
          fprintf(output_stream, gettext("  cluster2(%lx) is root\n"), cluster2);
        /* jedna sa o Root */
	bpb.BPB_RootClus = cluster1;
	d_writeSectors(0, (char*)&bpb, 1);
      } else {
        f32_readCluster(aTable[isStarting2-1].entryCluster, entries);
	if (debug_mode) {
          fprintf(output_stream, gettext("  cluster2(%lx) is starting; reading entry from cluster %lx\n"), cluster2, aTable[isStarting2-1].entryCluster);
          fprintf(output_stream, gettext("    redefining starting cluster2(%lx) %lx (equal to cluster2) to %lx\n"), cluster2, f32_getStartCluster(entries[aTable[isStarting2-1].entryIndex]), cluster1);
	}
        f32_setStartCluster(cluster1,&entries[aTable[isStarting2-1].entryIndex]);
        f32_writeCluster(aTable[isStarting2-1].entryCluster, entries);
      }
    }
  /* 2. update FAT */
    if (f32_readFAT(cluster1, &clus1val)) error(0,gettext("Can't read from FAT !"));
    if (f32_readFAT(cluster2, &clus2val)) error(0,gettext("Can't read from FAT !"));
    if (debug_mode) {
      fprintf(output_stream, gettext("  cluster1(%lx).value = %lx\n"), cluster1, clus1val);
      fprintf(output_stream, gettext("  cluster2(%lx).value = %lx\n"), cluster2, clus2val);
    }
    /* ak bol niektory/obidva clustery castou retaze, treba updatovat jeho/ich
       rodicov vo FAT */
    /* v pripade, ze bude chybna FAT a nejaky cluster odkazuje na
       free cluster (cize clus1val alebo clus2val = 0), nastane kruta chyba,
       lebo sa nenajde rodic (podla nasl. podmienok) a vznikne tak
       krizova referencia. Preto rozmyslam o dvoch moznostiach:
       predpokladat, ze je FAT v poriadku a uviest korektnu podmienku, alebo
       pouzit polovicnu podmienku a nechciet, aby hodnota clusterov nebola 0 */
//    if (!isStarting1 && clus1val)
    if (!isStarting1)
      tmpVal1 = def_findParent(cluster1);
    else tmpVal1 = 0;
    if (!isStarting2)
//    if (!isStarting2 && clus2val)
      tmpVal2 = def_findParent(cluster2);
    else tmpVal2 = 0;
    if (tmpVal1) {
      if (debug_mode)
        fprintf(output_stream, gettext("    found cluster1(%lx) parent: %lx\n"), cluster1, tmpVal1);
      f32_writeFAT(tmpVal1, cluster2);
    }
    if (tmpVal2) {
      if (debug_mode)
        fprintf(output_stream, gettext("    found cluster2(%lx) parent: %lx\n"), cluster2, tmpVal2);
      f32_writeFAT(tmpVal2, cluster1);
    }
    /* vymena FAT hodnot */
    if (clus1val == cluster2) {
      /* proti zacykleniu
         priklad:
         defrag:  A -> A -> N -> N -> N -> N
         chain : ...->214->2c4->215->980->...

         chcem vymenit 2c4<->215
	 po normalnej vymene bude: 214->215->215->215->...
	                           2c4->980->...
	 preto musim urobit opatrenie
      */
      f32_writeFAT(cluster1, clus2val);
      f32_writeFAT(cluster2, cluster1);
    } else if (clus2val == cluster1) {
      /* z druhej strany opatrenie */
      /* ak plati cluster1 < cluster2, k tejto moznosti by nemalo dojst ak plati cluster1 < cluster2 */
      f32_writeFAT(cluster1, cluster2);
      f32_writeFAT(cluster2, clus1val);
    } else {
      f32_writeFAT(cluster1, clus2val);
      f32_writeFAT(cluster2, clus1val);
    } 
    /* update aTable */
    if (isStarting1) {
      if (debug_mode)
        fprintf(output_stream, gettext("  (redefining starting cluster) aTable[%lu] = %lx\n"), isStarting1-1, cluster2);
      aTable[isStarting1-1].startCluster = cluster2;
    }
    if (isStarting2) {
      if (debug_mode)
        fprintf(output_stream, gettext("  (switching) aTable[%lu] = %lx\n"), isStarting2-1, cluster1);
      aTable[isStarting2-1].startCluster = cluster1;
    }
    /* ak bol niektory z vymenenych clusterov direntry nejakeho startovacieho
       v aTable, musim updatovat aj tuto hodnotu */
    for (tmpVal1 = 0; tmpVal1 < tableCount; tmpVal1++)
      if (aTable[tmpVal1].entryCluster == cluster1)
        aTable[tmpVal1].entryCluster = cluster2;
      else if (aTable[tmpVal1].entryCluster == cluster2)
        aTable[tmpVal1].entryCluster = cluster1;

  /* 3. fyzicka vymena */
    f32_readCluster(cluster1, cacheCluster1);
    f32_readCluster(cluster2, cacheCluster2);
    f32_writeCluster(cluster1, cacheCluster2);
    f32_writeCluster(cluster2, cacheCluster1);

  return 0;
}


/** Funkcia nájde nové miesto (optimálne) pre ¹tartovací klaster v prípade potreby
  * Pracuje klasickým algoritmom, èi¾e nájde najbli¾¹í pou¾itelný klaster od
  * beginCluster a ak je men¹í ako pôvodný startCluster, vymenia sa.
  * @param startCluster súèasný ¹tartovací klaster
  * @param beginCluster odkial sa mô¾e hlada» nový ¹tartovací klaster
  * @param outputCluster výstup - tu sa zapí¹e nové èíslo ¹tartovacieho klastra
  * @return funkcia vráti 0 ak nebola chyba, inak 1.
  */
int def_optimizeStartCluster(unsigned long startCluster, unsigned long beginCluster, unsigned long *outputCluster)
{
  unsigned long newCluster, value;

  if (startCluster == beginCluster) return 0;

  if (def_findFirstUsable(beginCluster, &newCluster, &value)) return 1;
  if (startCluster > newCluster) {
    if (debug_mode)
      fprintf(output_stream, gettext("(def_placeStartCluster) moving %lx -> %lx\n"), startCluster, newCluster);
    def_switchClusters(startCluster, newCluster);
    if (newCluster > beginCluster)
      *outputCluster = newCluster;
  } else return 0;
}

/**
 * Vykreslí grafický proggress bar zlo¾ený z '='.
 * Percentá vypoèíta na základe vzorcov:
 * \code
 *   percent = (cislo defragmentovaneho klastra) / (pocet vsetkych pouzitych klastrov) * 100
 *   (pocet '=') = size / 100 * percent
 * \endcode
 * @param size velkos» baru
*/
void print_bar(int size)
{
  double percent;
  int count, i;
  
  percent = ((double)clusterIndex / (double)usedClusters) * 100.0;
  count = (int)(((double)size / 100.0) * percent);
  printf("[");
  for (i = 0; i < count-1; i++)
    printf("=");
  if (count)
    printf(">");
  for (i = 0; i < (size - count); i++)
    printf(" ");
  printf("]");
}

/** Funkcia defragmentuje ne¹tartovacie klastre súboru/adresára, pracuje iba s 1 klastrom
 *  pozor! FAT-ka musí byt v poriadku, nesmie obsahova» krí¾ové referencie.
 *
 *  @param startCluster èíslo ¹tartovacieho klastra
 *  @return funkcia vráti èíslo posledného klastra, ktorý bol defragmentovaný
 */
unsigned long def_defragFile(unsigned long startCluster)
{
  unsigned long cluster1, cluster2, tmpClus, tmp;
  
  cluster1 = startCluster;
  cluster2 = startCluster;
  for (;;) {
    cluster2 = f32_getNextCluster(cluster1);
    clusterIndex++;
    /* koniec suboru */
    if (F32_LAST(cluster2)) { cluster2 = cluster1; break; }
    /* volny, rezervovany cluster */
    if (F32_FREE(cluster2) || F32_RESERVED(cluster2)) { cluster2 = cluster1; break; }
    /* chybny cluster */
    if (F32_BAD(cluster2)) { cluster2 = cluster1; break; }
    /* chybna hodnota FAT */
    if ((cluster2 > 0xfffffff) || (cluster2 > info.clusterCount)) { cluster2 = cluster1; break; }

    if ((cluster1+1) != cluster2) {
      if (def_findFirstUsable(cluster1+1, &tmpClus, &tmp)) break;
      if (debug_mode)
        fprintf(output_stream,gettext("  (def_defragFile) defragmenting chain: %lx->%lx to %lx->%lx\n"), cluster1, cluster2, cluster1, tmpClus);
      if (cluster2 > tmpClus) {
        /* treba defragmentovat */
        def_switchClusters(cluster2, tmpClus);
	cluster2 = tmpClus;
      }
    }
    cluster1 = cluster2;
    printf("%3d%% ", (int)(((double)clusterIndex / (double)usedClusters) * 100.0));
    print_bar(30);
    printf("\r");
  }
  return cluster2;
}

/** Funkcia defragmentuje súbory/adresáre podla tabulky aTable.
 *  Alokuje pamä» pre cache klastrov, pre buffer direntry. Defragmentácia prebieha v cykle.
 *  V tomto cykle sa pre aktuálnu polo¾ku aTable najprv nájde nový (optimálny) ¹tartovací klaster
 *  a potom sa zavolá funkcia na defragmentáciu ne¹tartovacích klastrov.
 *  @return Funkcia vráti 0, ak nenastala chyba.
 */
int def_defragTable()
{
  unsigned long tableIndex;
  unsigned long defClus = 1;
  unsigned long i, j = 0, k = 0;

  fprintf(output_stream, gettext("Defragmenting disk...\n"));
  fprintf(output_stream, "0%%\r");

  /* alokacia direntry a pomocnych clusterov */
  entryCount = (bpb.BPB_SecPerClus * info.BPSector) / sizeof(F32_DirEntry);
  if ((entries = (F32_DirEntry *)malloc(entryCount * sizeof(F32_DirEntry))) == NULL) error(0,gettext("Out of memory !"));
  if ((cacheCluster1 = (unsigned char*)malloc(bpb.BPB_SecPerClus * info.BPSector * sizeof(unsigned char))) == NULL) error(0, gettext("Out of memory !"));
  if ((cacheCluster2 = (unsigned char*)malloc(bpb.BPB_SecPerClus * info.BPSector * sizeof(unsigned char))) == NULL) error(0, gettext("Out of memory !"));

  clusterIndex = 0;
  for (tableIndex = 0; tableIndex < tableCount; tableIndex++) {
    if (debug_mode) {
      fprintf(output_stream, gettext("(def_defragTable) filechain for %lu: "), tableIndex);
      for (i = aTable[tableIndex].startCluster, j=0; i && (!F32_LAST(i)); i = f32_getNextCluster(i), j++)
        fprintf(output_stream, "%lx -> ", i);
      if (F32_LAST(i)) { j++; fprintf(output_stream, "%lx", i); }
      fprintf(output_stream,gettext("(count: %lu)\n"),j);
    }
    /* optimalne umiestni startovaci cluster, moze sposobit dodatocnu fragmentaciu */
    defClus++;
    def_optimizeStartCluster(aTable[tableIndex].startCluster, defClus, &defClus);
    clusterIndex++;
    /* defragmentacia nestartovacich clusterov */
    defClus = def_defragFile(aTable[tableIndex].startCluster);

    if (debug_mode) {
      fprintf(output_stream,gettext("(def_defragTable) new filechain for %lu: "), tableIndex);
      for (i = aTable[tableIndex].startCluster, k=0; i < 0xffffff0; i = f32_getNextCluster(i),k++)
        fprintf(output_stream, "%lx -> ", i);
      if (F32_LAST(i)) { k++; fprintf(output_stream,"%lx", i); }
      fprintf(output_stream, gettext(" (count: %lu)\n"),k);
    }
    printf("%3d%% ", (int)(((double)clusterIndex / (double)usedClusters) * 100.0));
    print_bar(30);
    printf("\r");
    fflush(stdout);
  }
  fprintf(output_stream,"\n");

  if (debug_mode) {
    fprintf(output_stream, gettext("(def_defragTable) aTable values (%lu): "), tableCount);
    for (tableIndex = 0; tableIndex < tableCount; tableIndex++)
      fprintf(output_stream, "%lx | ", aTable[tableIndex].startCluster);
  }

  free(cacheCluster2);
  free(cacheCluster1);
  free(entries);

  return 0;
}
