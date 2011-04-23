/*
 * analyze.h
 *
 * (c) Copyright 2006, vbmacher <pjakubco@gmail.com>
 *
 * Motto: Keep It Simple Stupid (KISS)
 *
 * start writing: 2.11.2006
 *
 */

#ifndef __ANALYZE__
#define __ANALYZE__

  /* polozka v tabulke fragmentovanych suborov/adresarov */
  typedef struct {
    unsigned long entryCluster;	/* cislo clustera, kde sa nachadza suborova polozka */
    unsigned short entryIndex;	/* cislo polozky v clusteri */
    unsigned long startCluster;	/* cislo pociatocneho sektora */
    unsigned long clusterCount; /* pocet clusterov */
  } __attribute__((packed)) aTableItem;

  extern aTableItem *aTable;
  extern unsigned long tableCount;
  extern float diskFragmentation;
  extern unsigned long usedClusters;

  int an_analyze();
  void an_freeTable();
  
#endif
