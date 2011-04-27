/*
 * analyze.h
 *
 * (c) Copyright 2006, vbmacher <pjakubco@gmail.com>
 *
 * Motto: Keep It Simple Stupid (KISS)
 *
 * start writing: 2.11.2006
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 * 
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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
