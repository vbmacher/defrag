/*
 * version.h
 *
 * (c) Copyright 2006, vbmacher <pjakubco@gmail.com>
 * use GNU C compiler (gcc)
 *
 * Motto: Keep It Simple Stupid (KISS)
 *
 * start writing: 31.10.2006
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
 
/*
	Release info
	~~~~~~~~~~~~
        v0.1b (12.11.2006)
          Fixed all relevant bugs, command line parameters added
	v0.1a (4.11.2006) 
	  simple defragmentation, manipulates with single cluster.
	  Not functioning checking of FAT table, working only with FAT32.
          Algorithm as in Windows (pushes everything on the beginning)
*/
 
#ifndef __F32ID_VERSION__
  #define __F32ID_VERSION__ "0.1b"
#endif
