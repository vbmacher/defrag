/**
 * @file disk.c
 *
 * @brief The module simulates physical disk operations (through image file).
 *
 * The aim was to write independent module for controlling physical disk operations that is possible to rewrite in
 * future also for real disk usage without modification needs of other modules. There are implemented some basic
 * functions, such as disk mount (assigning a file descriptor), loading/storing some sectors. These functions are
 * implemented in a way that every disk change is only movement of a pointer into the image file, given by file
 * descriptor called disk_descriptor and on given location they perform the operations.
 *
 */

/* The module I've started to write at day: 1.11.2006 
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
#include <unistd.h>

#include <disk.h>
#include <entry.h>

/** Descriptor of file image */
int disk_descriptor = 0;

/** Function mounts disk image (i.e. assigns the parameter into global variable disk_descriptor)
 *  @param image_descriptor This parameter will be assigned into disk_descriptor variable
 */
int d_mount(int image_descriptor)
{
  disk_descriptor = image_descriptor;
  return 0;
}


/** Un-mounting disk image, the descriptor is zero-ed. */
int d_umount()
{
  disk_descriptor = 0;
  return 0;
}

/** The function determines if the disk is mounted
    @return If the disk is mounted, return 1, or 0 otherwise. */
int d_mounted()
{
  if (!disk_descriptor) return 0;
  else return 1;
}

/** The function reads 'count' sectors from the image of LBA logicall address into buffer
 *  @param LBAaddress logical LBA address, from that we should read sectors
 *  @param buffer into this buffer the sectors' data are written to
 *  @param count number of sectors that should be read
 *  @param BPSector Number of bytes per sector
 *  @return number of really read sectors
 */
unsigned short d_readSectors(unsigned long LBAaddress, void *buffer, unsigned short count, unsigned short BPSector)
{
  ssize_t size;
  if (!disk_descriptor) return 0;
  lseek(disk_descriptor, LBAaddress * BPSector, SEEK_SET);
  size = read(disk_descriptor, buffer, count * BPSector);
  
  return (unsigned short)(size / BPSector);
}

/** The function writes 'count' sectors into the file disk image on the logical LBA address from buffer.
 *  @param LBAaddress logical LBA address, where we should write sectors
 *  @param buffer from this buffer the data will be read
 *  @param count number of sectors that should be written
 *  @param BPSector Number of bytes per sector
 *  @return number of really written sectors
 */
unsigned short d_writeSectors(unsigned long LBAaddress, void *buffer, unsigned short count, unsigned short BPSector)
{
  ssize_t size;
  if (!disk_descriptor) return 0;
  lseek(disk_descriptor, LBAaddress * BPSector, SEEK_SET);
  size = write(disk_descriptor, buffer, count * BPSector);

  return (unsigned short)(size / BPSector);
}
