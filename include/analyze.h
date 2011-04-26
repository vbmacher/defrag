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

  /* Item in a table of fragmented files/directories */
  typedef struct {
    unsigned long entryCluster;	/* number of cluster where file entry is located */
    unsigned short entryIndex;	/* number of an entry in cluster */
    unsigned long startCluster;	/* number of starting sector */
    unsigned long clusterCount; /* number of clusters */
  } __attribute__((packed)) aTableItem;

  extern aTableItem *aTable;
  extern unsigned long tableCount;
  extern float diskFragmentation;
  extern unsigned long usedClusters;

  int an_analyze();
  void an_freeTable();
  
#endif
