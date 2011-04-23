/**
 * @file disk.c
 *
 * @brief Modul simuluje fyzické diskové operácie (cez súbor obrazu).
 *
 * Mojou snahou bolo napísa» nezávislý modul na ovládanie fyzických diskových operácií, ktorý je mo¾né v
 * budúcnosti prepísa» aj pre pou¾itie pre reálny disk bez nutnosti zmien ostatných modulov. Sú tu implementované
 * niektoré základné funkcie ako namontovanie disku (ide o priradenie file descriptora), naèítanie niekolkých
 * sektorov a zápis niekolkých sektorov. Tieto funkcie sú implementované tak, ¾e sa iba pohybujú v súbore danom
 * deskriptorom disk_descriptor a na danej pozícii urobia príslu¹né operácie.
 *
 */
/* Modul som zaèal písa» dòa: 1.11.2006 */

#include <stdio.h>
#include <unistd.h>

#include <disk.h>
#include <entry.h>

/** Deskriptor súboru obrazu */
int disk_descriptor = 0;

/** Funkcia namontuje obraz disku (èi¾e priradí globálnej premennej disk_descriptor parameter
 *  @param image_descriptor Tento parameter sa priradí do globálnej premennej disk_descriptor
 */
int d_mount(int image_descriptor)
{
  disk_descriptor = image_descriptor;
  return 0;
}


/** Odmontovanie obrazu disku, ide o vynulovanie deskriptora */
int d_umount()
{
  disk_descriptor = 0;
  return 0;
}

/** Funkcia zistí, èi je disk namontovaný */
int d_mounted()
{
  if (!disk_descriptor) return 0;
  else return 1;
}

/** Funkcia preèíta count sektorov z obrazu z logickej LBA adresy do buffera
 *  @param LBAaddress logická LBA adresa, z ktorej sa má èíta»
 *  @param buffer do tohto buffera sa zapí¹u naèítané sektory dát
 *  @param count poèet sektorov, ktoré sa majú naèíta»
 *  @param BPSector Poèet bytov / sektor
 *  @return vracia poèet skutoène naèitaných sektorov
 */
unsigned short d_readSectors(unsigned long LBAaddress, void *buffer, unsigned short count, unsigned short BPSector)
{
  ssize_t size;
  if (!disk_descriptor) return 0;
  lseek(disk_descriptor, LBAaddress * BPSector, SEEK_SET);
  size = read(disk_descriptor, buffer, count * BPSector);
  
  return (unsigned short)(size / BPSector);
}

/** Funkcia zapí¹e count sektorov do obrazu na logickú LBA adresu z buffera
 *  @param LBAaddress logická LBA adresa, na ktorú sa má zapisova»
 *  @param buffer z tohto buffera sa budú dáta èíta»
 *  @param count poèet sektorov, ktoré sa majú zapísa»
 *  @param BPSector Poèet bytov / sektor
 *  @return vracia poèet skutoène zapísaných sektorov
 */
unsigned short d_writeSectors(unsigned long LBAaddress, void *buffer, unsigned short count, unsigned short BPSector)
{
  ssize_t size;
  if (!disk_descriptor) return 0;
  lseek(disk_descriptor, LBAaddress * BPSector, SEEK_SET);
  size = write(disk_descriptor, buffer, count * BPSector);

  return (unsigned short)(size / BPSector);
}
