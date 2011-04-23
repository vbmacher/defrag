/*
 * disk.h
 *
 * (c) Copyright 2006, vbmacher <pjakubco@gmail.com>
 * use GNU C compiler (gcc)
 *
 * Motto: Keep It Simple Stupid
 *
 * start writing: 1.11.2006
 *
 */
 
#ifndef __DISKOP__
#define __DISKOP__
  
  int d_mount(int);
  int d_umount();
  int d_mounted();
  unsigned short d_readSectors(unsigned long, void*, unsigned short, unsigned short);
  unsigned short d_writeSectors(unsigned long, void*, unsigned short, unsigned short);
  
#endif
