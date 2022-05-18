/**
 * @file defrag.c
 *
 * @brief Module performs disk defragmentation
 *
 */

/* I've started to write the module at day: 3.11.2006
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libintl.h>
#include <locale.h>

#include <entry.h>
#include <analyze.h>
#include <fat32.h>
#include <disk.h>


/** temporary buffer for directory items (if direntry is updated) */
F32_DirEntry *entries = NULL;
F32_DirEntry *entries2 = NULL; // temp
unsigned short entryCount;

/** 1. cache of cluster */
unsigned char *cacheCluster1 = NULL;
/** 2. cache of cluster */
unsigned char *cacheCluster2 = NULL;

/** index of cluster that is actually defragmenting (it is used for percentage computation)*/
unsigned long clusterIndex;

/** The function finds parent of cluster from FAT
 *  If parameter has value 0, parent is not searched. In the other case whole FAT is being scanned
 *  if some cluster links to the cluster given as parameter.
 * @param cluster number of cluster that we need parent of
 * @return number of parent cluster of the given cluster or 0 in a case that it is not found or it is
 *         root cluster.
 */
unsigned long def_findParent(unsigned long cluster)
{
  unsigned long i, val;
  if (!cluster) return 0;
  for (i = 2; i <= info.clusterCount; i++) {
    if (f32_readFAT(i, &val)) error(0,_("Can't read from FAT !"));
    if (val == cluster) return i;
  }
  return 0;
}

/**
 * Function determines if a cluster is starting cluster (the aTable is translated)
 * @param cluster testing cluster
 * @param index it is output variable that will contain incremented index in aTable, if the
 *              cluster was starting. If not, its value will be 0.
 * @return it returns 1, if cluster is starting or 0 otherwise
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

/** The function find first usable cluster (output is directed into outCluster variable) and its value (the output is
  * directed into outValue).
  * Usable cluster is that can be overwritten (is in the "interval of good clusters") and is not bad.
  * @param beginCluster from where we should start to search
  * @param outCluster output variable - found usable cluster
  * @param outValue output variable - value of found cluster
  * @return it returns 1 in case of an error, or 0 otherwise.
  */
int def_findFirstUsable(unsigned long beginCluster, unsigned long *outCluster, unsigned long *outValue)
{
  unsigned long cluster;
  unsigned long value = 0;
  char found = 0;
 
  if (debug_mode) 
    fprintf(output_stream,_("(def_findFirstUsable) First usable cluster from 0x%lx is: "), beginCluster);

  for (cluster = beginCluster; cluster <= info.clusterCount; cluster++) {
    if (f32_readFAT(cluster, &value)) error(0,_("Can't read from FAT !"));
    if (value != F32_BAD_L) {
      found = 1;
      break;
    }
  }
  if (!found) {
    if (debug_mode)
      fprintf(output_stream,_("Not found!\n"));
    return 1;
  }
  if (debug_mode)
    fprintf(output_stream,_("0x%lx\n"), cluster);
  *outCluster = cluster;
  *outValue = value;
  return 0;
}

/**
 * The function switches 2 clusters.
 *
 * Clusters can be starting, or part in a file chain. They can be directories, slots or file data.
 *
 * If a cluster is starting:
 *
 * -# Update dir entry -> set the other cluster to be starting in the directory entry.
 * -# If it is root, update also bpb (FAT32 can have root cluster anywhere).
 *
 * If a cluster is directory:
 *
 * -# Point the "." direntry of that cluster to the other cluster
 * -# Point the ".." direntry of that cluster to the parent of this cluster
 *
 * If a cluster is part of a chain
 *
 * -# Update its parent in FAT to point to the other cluster
 *
 * The two clusters are switched by:
 *
 * -# Switching the cluster values in FAT table.
 * -# Updating values in aTable (all files having entryCluster set to the one cluster
 *    must point to the other cluster)
 * -# Switching real data in clusters
 *
 * There must be taken care for infinite loop, as it is shown in the following example:
 *
 *    \code
 *      defragmented:  Y -> Y -> Y -> N -> N -> N -> N
 *      chain       : ...->213->214->2c4->215->980->...
 *    \endcode
 *
 * And we want to switch 2c4<->215. Firstly we try normal switch:
 *    - 2c4 is not starting, its parent is 214 (i.e. 214 points at 2c4)
 *    - 215 is not starting, its parent is 2c4 (i.e. 2c4 points at 215)
 *    .
 *    update of parents:
 *      - 214 will point at 215
 *      - 2c4 will point at 980
 *      .
 *    and switching FAT values: 
 *    - cluster 2c4 originally pointing at 215 will point at 980 (on value that 215 cluster is pointing at)
 *    - cluster 215 originally pointing at 980 will point at 215 (on value that 2c4 cluster is pointing at)
 *    .
 *    After this switch new chain will look like this:
 *    \code
 *       214->215->215->215->... (circular referrence)
 *       2c4->980->...
 *    \endcode
 *    Therefore I need to perform a precaution. In another case the classic switch will be performed.
 *
 * @param cluster1
 *   Number of the first cluster
 * @param cluster2
 *   Number of the sectond cluster
 */
void def_switchClusters(unsigned long cluster1, unsigned long cluster2)
{
  unsigned long isStarting1, isStarting2; /* if the clusters are starting. 
                                             If yes, they will hold (index + 1)
					     in table aTable
					  */
  unsigned long tmpVal1, tmpVal2;
  unsigned long clus1val, clus2val;
  int i; // temp variable

  if (debug_mode)
    fprintf(output_stream,_("  (def_switchClusters) 0x%lx <=> 0x%lx\n"), cluster1, cluster2);

  if (cluster1 == cluster2)
    return;

  /* 1. find out if clusters are starting. If yes, update dir entry. */
  /* be careful on root! It can be one of the clusters */
    def_isStarting(cluster1, &isStarting1);
    def_isStarting(cluster2, &isStarting2);

    if (debug_mode) {
      // what we know about the clusters.
      tmpVal1 = def_findParent(cluster1);
      fprintf(output_stream, _("    1:parent= 0x%lx\n"), tmpVal1);
      tmpVal1 = def_findParent(cluster2);
      fprintf(output_stream, _("    2:parent= 0x%lx\n"), tmpVal1);
      tmpVal1 = 0;
    }
    
    if (isStarting1) {
      if (!aTable[isStarting1-1].entryCluster) {
        /* the first cluster is root */
        if (debug_mode)
          fprintf(output_stream, "    1:0x%lx=(root)\n", cluster1);
	bpb.BPB_RootClus = cluster2;
	d_writeSectors(0, (char*)&bpb, 1, bpb.BPB_SecPerClus);
      } else {
        f32_readCluster(aTable[isStarting1-1].entryCluster, entries);
        if (debug_mode) {
          // what we know about the clusters.
          fprintf(output_stream, "    1:'");
          for (i = 0; i < 8; i++)
            fprintf(output_stream,   "%c", entries[aTable[isStarting1-1].entryIndex].fileName[i]);
          fprintf(output_stream, _("' 0x%lx=(files[%lx]); dir=0x%lx.entry=%d; start=0x%lx (new 0x%lx)\n"),
            cluster1, isStarting1-1, aTable[isStarting1-1].entryCluster, aTable[isStarting1-1].entryIndex,
            f32_getStartCluster(entries[aTable[isStarting1-1].entryIndex]), cluster2);
	}
        f32_setStartCluster(cluster2,&entries[aTable[isStarting1-1].entryIndex]);
        f32_writeCluster(aTable[isStarting1-1].entryCluster, entries);
      }
    }
    if (isStarting2) {
      if (!aTable[isStarting2-1].entryCluster) {
        if (debug_mode)
          fprintf(output_stream, "  2:0x%lx=(root)\n", cluster2);
        /* second cluster is root */
	bpb.BPB_RootClus = cluster1;
	d_writeSectors(0, (char*)&bpb, 1, bpb.BPB_SecPerClus);
      } else {
        f32_readCluster(aTable[isStarting2-1].entryCluster, entries);
	if (debug_mode) {
          fprintf(output_stream, "    2:'");
          for (i = 0; i < 8; i++)
            fprintf(output_stream, "%c", entries[aTable[isStarting2-1].entryIndex].fileName[i]);
          fprintf(output_stream, _("' 0x%lx=(files[%lx]); dir=0x%lx.entry=%d; start=0x%lx (new 0x%lx)\n"),
            cluster2, isStarting2-1, aTable[isStarting2-1].entryCluster, aTable[isStarting2-1].entryIndex,
            f32_getStartCluster(entries[aTable[isStarting2-1].entryIndex]), cluster1);
	}
        f32_setStartCluster(cluster1,&entries[aTable[isStarting2-1].entryIndex]);
        f32_writeCluster(aTable[isStarting2-1].entryCluster, entries);
      }
    }
  /* 2. update FAT */
    if (f32_readFAT(cluster1, &clus1val)) error(0,_("Can't read from FAT !"));
    if (f32_readFAT(cluster2, &clus2val)) error(0,_("Can't read from FAT !"));
    if (debug_mode) {
      fprintf(output_stream, _("    1:0x%lx.value= %lx\n"), cluster1, clus1val);
      fprintf(output_stream, _("    2:0x%lx.value= %lx\n"), cluster2, clus2val);
    }
    /* If some or both clusters were part of the chain, it is necessary to update its/their
       parents in FAT.

       In a case that FAT is wrong and some cluster points at free cluster (i.e. clus1val or clus2val = 0),
       cruel error will be created, because the parent won't be found. */
    if (!isStarting1 && clus1val)
      tmpVal1 = def_findParent(cluster1);
    else tmpVal1 = 0;
    if (!isStarting2 && clus2val)
      tmpVal2 = def_findParent(cluster2);
    else tmpVal2 = 0;
    if (tmpVal1) {
      if (debug_mode)
        fprintf(output_stream, _("    1:0x%lx.parent= 0x%lx\n"), cluster1, tmpVal1);
      f32_writeFAT(tmpVal1, cluster2);
    }
    if (tmpVal2) {
      if (debug_mode)
        fprintf(output_stream, _("    2:0x%lx.parent= 0x%lx\n"), cluster2, tmpVal2);
      f32_writeFAT(tmpVal2, cluster1);
    }
    /* switching FAT values */
    if (clus1val == cluster2) {
      /* precaution */
      f32_writeFAT(cluster1, clus2val);
      f32_writeFAT(cluster2, cluster1);
    } else if (clus2val == cluster1) {
      /* precaution from the other side */
      /* If cluster1 < cluster2, we should not consider this option.. */
      f32_writeFAT(cluster1, cluster2);
      f32_writeFAT(cluster2, clus1val);
    } else {
      f32_writeFAT(cluster1, clus2val);
      f32_writeFAT(cluster2, clus1val);
    }

    /* update aTable */
    if (isStarting1)
      aTable[isStarting1-1].startCluster = cluster2;
    if (isStarting2)
      aTable[isStarting2-1].startCluster = cluster1;

    /* If some of switched clusters was direntry of some starting cluster in aTable, we have to update
       also this value */
    for (tmpVal1 = 0; tmpVal1 < tableCount; tmpVal1++) {
      if (aTable[tmpVal1].entryCluster == cluster1) {
        tmpVal2 = aTable[tmpVal1].entryCluster;
        aTable[tmpVal1].entryCluster = cluster2;
        if (debug_mode)
          fprintf(output_stream, "    file[%lu].entryCluster (originally 0x%lx) = 0x%lx\n", tmpVal1, tmpVal2,cluster2);
      }
      else if (aTable[tmpVal1].entryCluster == cluster2) {
        tmpVal2 = aTable[tmpVal1].entryCluster;
        aTable[tmpVal1].entryCluster = cluster1;
        if (debug_mode)
          fprintf(output_stream, "    file[%lu].entryCluster (originally 0x%lx) = 0x%lx\n", tmpVal1, tmpVal2, cluster1);
      }
    }

  /* 3. physicall switch */
    f32_readCluster(cluster1, cacheCluster1);
    f32_readCluster(cluster2, cacheCluster2);
    f32_writeCluster(cluster1, cacheCluster2);
    f32_writeCluster(cluster2, cacheCluster1);

    if (debug_mode) {
      i = def_findParent(cluster1);
      fprintf(output_stream, _("    1:(new)0x%lx.parent= 0x%lx, value=0x%lx\n"), cluster1, i, f32_getNextCluster(cluster1));
      i = def_findParent(cluster2);
      fprintf(output_stream, _("    2:(new)0x%lx.parent= 0x%lx, value=0x%lx\n"), cluster2, i, f32_getNextCluster(cluster2));
    }

    /* Update "." and ".." entries if one of starting cluster was directory*/

    // if a directory is moving somewhere else, 
    // in all its dir entries we must find subdirectories,
    // load their entries and at every '..' entry put the new value
    // of the directory cluster..
    if ((entries2 = (F32_DirEntry *)malloc(entryCount * sizeof(F32_DirEntry))) == NULL) {
      error(0, _("Out of memory !"));
    }

    if (isStarting1 && aTable[isStarting1-1].isDir) {
      // cluster1 will point to cluster2
      for (tmpVal1 = cluster2; !F32_LAST(tmpVal1); tmpVal1 = f32_getNextCluster(tmpVal1)) {
        f32_readCluster(tmpVal1, entries);
        if (!memcmp(entries[0].fileName,".       ",8)) {
          // found it
          if (debug_mode) {
            fprintf(output_stream, "    1:0x%lx.%d ('.').start=0x%lx (new 0x%lx)\n", tmpVal1,0,
            f32_getStartCluster(entries[0]), cluster2);
          }
          f32_setStartCluster(cluster2,&entries[0]);
        }
        if (!memcmp(entries[1].fileName,"..      ",8)) {
          tmpVal2 = def_findParent(tmpVal1);
          // found it
          if (debug_mode) {
            fprintf(output_stream, "    1:0x%lx.%d ('..').start=0x%lx (new 0x%lx)\n", tmpVal1,1,
            f32_getStartCluster(entries[1]), tmpVal2);
          }
          f32_setStartCluster(tmpVal2,&entries[1]);
        }
        f32_writeCluster(tmpVal1, entries);

        for (i = 0; i < entryCount; i++) {
          if (!memcmp(entries[i].fileName,".       ",8)) continue;
          if (!memcmp(entries[i].fileName,".       ",8)) continue;
          if (entries[i].fileName[0] == 0) continue;
          if (entries[i].fileName[0] == 0xe5) continue;
          if ((entries[i].attributes & 0x10) == 0x10) {
            // subdirectory
            tmpVal2 = f32_getStartCluster(entries[i]);
            f32_readCluster(tmpVal2, entries2);
            if (!memcmp(entries2[1].fileName,"..      ",8)) {
              if (debug_mode) {
                fprintf(output_stream, "    1:0x%lx->0x%lx.%d ('..').start=0x%lx (new 0x%lx): '", tmpVal1, tmpVal2,1,
                  f32_getStartCluster(entries2[1]), cluster2);
                int k;
                for (k = 0; k < 8; k++) fprintf(output_stream,"%c",entries[i].fileName[k]);
                fprintf(output_stream, "'\n");
              }
              f32_setStartCluster(cluster2, &entries2[1]);
              f32_writeCluster(tmpVal2, entries2);
            }
          }
        }
      }
    }
    if (isStarting2 && aTable[isStarting2-1].isDir) {
      for (tmpVal1 = cluster1; !F32_LAST(tmpVal1); tmpVal1 = f32_getNextCluster(tmpVal1)) {
        // cluster2 will point to cluster1
        f32_readCluster(tmpVal1, entries);
        if (!memcmp(entries[0].fileName,".       ",8)) {
          // found it
          if (debug_mode) {
            fprintf(output_stream, "    2:0x%lx.%d ('.').start=0x%lx (new 0x%lx)\n", tmpVal1,0,
            f32_getStartCluster(entries[0]), cluster1);
          }
          f32_setStartCluster(cluster1,&entries[0]);
        }
        if (!memcmp(entries[1].fileName,"..      ",8)) {
          tmpVal2 = def_findParent(tmpVal1);
          // found it
          if (debug_mode) {
            fprintf(output_stream, "    2:0x%lx.%d ('..').start=0x%lx (new 0x%lx)\n", tmpVal1,1,
            f32_getStartCluster(entries[1]), tmpVal2);
          }
          f32_setStartCluster(tmpVal2,&entries[1]);
        }
        f32_writeCluster(tmpVal1, entries);
        for (i = 0; i < entryCount; i++) {
          if (!memcmp(entries[i].fileName,".       ",8)) continue;
          if (!memcmp(entries[i].fileName,".       ",8)) continue;
          if (entries[i].fileName[0] == 0) continue;
          if (entries[i].fileName[0] == 0xe5) continue;
          if ((entries[i].attributes & 0x10) == 0x10) {
            // subdirectory
            tmpVal2 = f32_getStartCluster(entries[i]);
            f32_readCluster(tmpVal2, entries2);
            if (!memcmp(entries2[1].fileName,"..      ",8)) {
              if (debug_mode) {
                fprintf(output_stream, "    2: 0x%lx->0x%lx.%d ('..').start=0x%lx (new 0x%lx): '", tmpVal1,tmpVal2,1,
                  f32_getStartCluster(entries2[1]), cluster1);
                int k;
                for (k = 0; k < 8; k++) fprintf(output_stream,"%c",entries[i].fileName[k]);
                fprintf(output_stream, "'\n");
              }
              f32_setStartCluster(cluster1, &entries2[1]);
              f32_writeCluster(tmpVal2, entries2);
            }
          }
        }
      }
    }
    free(entries2);
}


/** This function finds a new location (optimal) for starting cluster in case of need.
  * It works by classic algorithm, i.e. it founds closest usable cluster from beginCluster
  * and if it is less than original startCluster, they will switch.
  * @param startCluster current starting cluster
  * @param beginCluster from where we can search for new starting cluster
  * @param outputCluster output variable - there a new number of starting cluster will be written
  * @return function returns 0 if there was no error, or 1 otherwise.
  */
int def_optimizeStartCluster(unsigned long startCluster, unsigned long beginCluster, unsigned long *outputCluster)
{
  unsigned long newCluster, value;

  if (startCluster == beginCluster)
    return 0;

  if (def_findFirstUsable(beginCluster, &newCluster, &value))
    return 1;
  if (startCluster > newCluster) {
    if (debug_mode)
      fprintf(output_stream, _("(def_optimizeStartCluster) moving 0x%lx to 0x%lx\n"), startCluster, newCluster);
    def_switchClusters(startCluster, newCluster);
    if (newCluster > beginCluster)
      *outputCluster = newCluster;
  } 
  return 0;
}

/**
 * This function draws graphical progress bar from '=' chars.
 * Percentage is computed based on equations:
 * \code
 *   percent = (number of defragmented cluster) / (number of all used clusters) * 100
 *   (number of '=') = size / 100 * percent
 * \endcode
 * @param size size of the progress bar
*/
void print_bar(int size)
{
  static int old_percent = -1;
  double percent;
  int count, i;
 
  percent = ((double)clusterIndex / (double)usedClusters) * 100.0;

  if ((int)percent == old_percent)
    return;
  old_percent = (int)percent;

  printf("%3d%% ", (int)percent);
  count = (int)(((double)size / 100.0) * percent);
  printf("[");
  for (i = 0; i < count-1; i++)
    printf("=");
  if (count)
    printf(">");
  for (i = 0; i < (size - count); i++)
    printf(" ");
  printf("]\r");
  fflush(stdout);
}

/** The function defragments non-starting clusters of file/directory, it works only with a single cluster
 *  WARNING! FAT had to be OK, it does not have to contain cross referrences.
 *
 *  @param startCluster number of starting cluster
 *  @return function returns number of last cluster that was defragmented
 */
unsigned long def_defragFile(unsigned long startCluster)
{
  unsigned long cluster1, cluster2, tmpClus, tmp;
  
  cluster1 = startCluster;
  cluster2 = startCluster;
  for (;;) {
    cluster2 = f32_getNextCluster(cluster1);
    clusterIndex++;
    /* end of the file */
    if (F32_LAST(cluster2)) { cluster2 = cluster1; break; }
    /* free, reserved cluster */
    if (F32_FREE(cluster2) || F32_RESERVED(cluster2)) { cluster2 = cluster1; break; }
    /* bad cluster */
    if (F32_BAD(cluster2)) { cluster2 = cluster1; break; }
    /* bad value in FAT */
    if ((cluster2 > 0xfffffff) || (cluster2 > info.clusterCount)) { cluster2 = cluster1; break; }

    if ((cluster1+1) != cluster2) {
      if (def_findFirstUsable(cluster1+1, &tmpClus, &tmp)) break;
      if (debug_mode)
        fprintf(output_stream,_("  (def_defragFile) defragmenting chain: %lx->%lx to %lx->%lx\n"), cluster1, cluster2, cluster1, tmpClus);
      if (cluster2 > tmpClus) {
        /* it is needed to defragment */
        def_switchClusters(cluster2, tmpClus);
	cluster2 = tmpClus;
      }
    }
    cluster1 = cluster2;
    if (!debug_mode)
      print_bar(30);
  }
  return cluster2;
}

/** The function defragments files/directories according to aTable.
 *  It allocates memory for clusters cache, then for direntry buffer. Defragmentation runs in a cycle.
 *  In that cycle, at first new (optimal) starting cluster is found for actual item in aTable. Then a function
 *  for non-starting clusters defragmentation is called.
 *  @return Function returns 0, if there was no error.
 */
int def_defragTable()
{
  unsigned long tableIndex;
  unsigned long defClus = 1;
  unsigned long i, j = 0, k = 0;

  fprintf(output_stream, _("Defragmenting disk...\n"));

  /* Allocation of direntry and temporary clusters */
  entryCount = (bpb.BPB_SecPerClus * info.BPSector) / sizeof(F32_DirEntry);
  if ((entries = (F32_DirEntry *)malloc(entryCount * sizeof(F32_DirEntry))) == NULL) error(0,_("Out of memory !"));
  if ((cacheCluster1 = (unsigned char*)malloc(bpb.BPB_SecPerClus * info.BPSector * sizeof(unsigned char))) == NULL) error(0, _("Out of memory !"));
  if ((cacheCluster2 = (unsigned char*)malloc(bpb.BPB_SecPerClus * info.BPSector * sizeof(unsigned char))) == NULL) error(0, _("Out of memory !"));

  if (debug_mode) {
    fprintf(output_stream, _("(def_defragTable) original aTable values (%lu): "), tableCount);
    for (tableIndex = 0; tableIndex < tableCount; tableIndex++)
      fprintf(output_stream, "%lx | ", aTable[tableIndex].startCluster);
  }

  clusterIndex = 0;
  for (tableIndex = 0; tableIndex < tableCount; tableIndex++) {
    /* Optimally places starting cluster, it can cause additional fragmentation */
    defClus++;
    def_optimizeStartCluster(aTable[tableIndex].startCluster, defClus, &defClus);
    clusterIndex++;
    /* Defragmentation of non-starting clusters */
    defClus = def_defragFile(aTable[tableIndex].startCluster);

    if (!debug_mode)
      print_bar(30);
  }
  fprintf(output_stream,"\n");

  if (debug_mode) {
    fprintf(output_stream, _("(def_defragTable) aTable values (%lu): "), tableCount);
    for (tableIndex = 0; tableIndex < tableCount; tableIndex++)
      fprintf(output_stream, "%lx | ", aTable[tableIndex].startCluster);
  }

  free(cacheCluster2);
  free(cacheCluster1);
  free(entries);

  return 0;
}
