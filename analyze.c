/**
 * @file analyze.c
 *
 * @brief Module for disk fragmentation analysis
 *
 */

/* Module I've started to write at day: 2.11.2006
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


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libintl.h>
#include <locale.h>

#include <entry.h>
#include <fat32.h>
#include <analyze.h>


#define MAX_FILES 200000

/** Table with important informations about each item in all directory structure.
 * The items contain: starting cluster, the number of directory cluster, the number of the item
 * in the directory cluster, number of clusters (this value is filled up in deep analysis).
 * Defragmentation works based on values within this table.
 */
aTableItem* aTable = NULL;

/** Number of values in the table is equal to number of all files and directories that have
  * allocated almost 1 cluster
  */
unsigned long tableCount = 0;

/** Percentual disk fragmentation */
float diskFragmentation;

/** Number of used clusters */
unsigned long usedClusters;

/** Number of items in single directory (variable value according to cluster size) */
unsigned short an_entryCount;

/** Filling the aTable table woks in recursive way, the table is implemented
  * as dynamic array of max 10000 items (i.e. there can exist maximum 10000
  * files and directories together on disk). More appropriate implementation would be
  * using linked list, but for experiment purposes the dynamic array is enough.
  * This function adds new item into the aTable, however it fills only some informations in the item
  * @param startCluster starting cluster of the item
  * @param entCluster directory cluster that links to the item
  * @param ind index of the item in directory cluster
  * @param isDir if the file is directory (1), or regular file (0)
  */
void an_addFile(unsigned long startCluster, unsigned long entCluster, unsigned short ind, unsigned char isDir)
{
  if (aTable == NULL) {
    if ((aTable = (aTableItem *)malloc(MAX_FILES * sizeof(aTableItem))) == NULL)
      error(0, _("Out of memory !"));
    tableCount = 0;
  } else;
  tableCount++;
  if (tableCount >= MAX_FILES) { free(aTable); error(0,_("Out of memory !"));}
  aTable[tableCount-1].startCluster = startCluster;
  aTable[tableCount-1].entryCluster = entCluster;
  aTable[tableCount-1].entryIndex = ind;
  aTable[tableCount-1].isDir = isDir;

  if (debug_mode)
    fprintf(output_stream, "(an_addFile) [%d]: start= 0x%5lx; dir= 0x%5lx; index= %2d; isDir=%d\n", tableCount-1, startCluster,entCluster,ind,isDir);
}

/** The function frees up memory used for aTable
  */
void an_freeTable()
{
  free(aTable);
}

/** The function determines percentage fragmentation of single directory item (file/directory). It is
  * part of the deep disk analysis.
  * 
  * For given starting cluster it traverses all the chain of clusters and determines if clusters are
  * going one after another, i.e. that if the difference of next and actual cluster number is 1. 
  * In a case that not, it increments number of fragmented clusters.
  * Within the traversion of the chain there is stored number of all clusters that were traversed. After
  * loop is finished, this variable contains number ofall clusters of the file or directory and it is stored.
  * into aTable. Percentual fragmentation is computed as:
  * \code
  *   (num. of frag.cluster of the item) / (num. of all used clusters of the item) * 100
  * \endcode
  * @param startCluster Starting cluster of the item
  * @param aTIndex index in aTable - into the table is written number of clusters of the directory item
  * @return item fragmentation in percentage
*/
float an_getFileFragmentation(unsigned long startCluster, unsigned long aTIndex)
{
  unsigned long cluster; /* temp cluster */
  int fragmentCount = 0; /* number of fragmented clusters */
  int count = 0;	 /* number of file clusters; starting cluster is not considered */
  
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

/** This function recursively traverses all directory structure. It is part of the first phase of disk analysis (the basic one).
  * During the traversation it stores into aTable important information about directory item, such as starting cluster,
  * number of directory cluster that contains link for given item and index in the directory cluster. Besides it calls
  * for each item the an_getFileFragmentation function (to get its percentage fragmentation). This fragmentation is added to global
  * variable called diskFragmentation. Be careful! The FAT table has to be OK; in the other case there can be circular referrences
  * (or cross references)!
  * @param startCluster number of root cluster (from where should the traversation start)
*/
void an_scanDisk(unsigned long startCluster)
{
  unsigned short index;
  unsigned long cluster, tmpCluster;
  unsigned char tmpAttr;
  F32_DirEntry *entries;
  
  /* In errorneous FATk we must count with clusterCount instead of 0xffffff0 */
  if (startCluster > info.clusterCount) return;

  if ((entries = (F32_DirEntry *)malloc(an_entryCount * sizeof(F32_DirEntry))) == NULL)
    error(0, _("Out of memory !"));

  for (cluster = startCluster; !F32_LAST(cluster); cluster = f32_getNextCluster(cluster)) {
    f32_readCluster(cluster, entries);
    for (index = 0; index < an_entryCount; index++) {
      if (!entries[index].fileName[0]) { free(entries); return; }
      /* in the next we work with items that:
           1. are not deleted,
	   2. are not slots (long names)
	   3. do not point to parent or root directory
      */
      if ((entries[index].fileName[0] != 0xe5) && 
          entries[index].attributes != 0x0f &&
          memcmp(entries[index].fileName,".       ",8) &&
	  memcmp(entries[index].fileName,"..      ",8)) {
	tmpAttr = entries[index].attributes & 0x10;
        tmpCluster = f32_getStartCluster(entries[index]);
	if (tmpCluster != 0) {
          int isDir = 0;
	  /* if the item is subdirectory, the function is called recursively */
          if (tmpAttr == 0x10) {
            isDir = 1;
	    /* protection against infinite loop */
	    if (tmpCluster != startCluster)
	      an_scanDisk(tmpCluster);
	  }
          /* if a starting cluster is bad, it ignores the item */
          if (tmpCluster <= info.clusterCount) {
            an_addFile(tmpCluster,cluster,index,isDir);
            diskFragmentation += an_getFileFragmentation(tmpCluster, tableCount-1);
          }
	}
      }
    }
  }
  free(entries);
}

/** Main function for disk analysis; before it calls an_scanDisk function, it performs
  * some preparation operations, such as it gets number of items in directory and adds first
  * value into aTable - the root cluster, and computes its fragmentation. After finishing
  * recursive traversation of directory structure it computes global percentage disk fragmentation
  * by dividing diskFragmentation variable by number of items in aTable.
  */
int an_analyze()
{
  fprintf(output_stream, _("Analysing disk...\n"));

  an_entryCount = (bpb.BPB_SecPerClus * info.BPSector) / sizeof(F32_DirEntry);
  /* first phase of analysis starts with root cluster */
  aTable = NULL;
  tableCount = 0;
  
  /* table contains also root cluster */
  an_addFile(bpb.BPB_RootClus, 0, 0, 1);
  usedClusters = 0;
  diskFragmentation = an_getFileFragmentation(bpb.BPB_RootClus, 0);
  an_scanDisk(bpb.BPB_RootClus);
  diskFragmentation /= (tableCount - 1);

  fprintf(output_stream, _("Disk is fragmented for: %.2f%%\n"), diskFragmentation);
 
  /*WARNING! We do not free memory in this time, but AFTER defragmentation,
    otherwise we would get an error "Segmentation fault" because the table will be
    used in later time.
  */
  return 0;
}
