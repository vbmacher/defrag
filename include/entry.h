/*
 * entry.h
 *
 * (c) Copyright 2006, vbmacher <pjakubco@gmail.com>
 * use GNU C compiler (gcc)
 *
 * Motto: Keep It Simple Stupid (KISS)
 * start writing: 1.11.2006
 *
 */

#ifndef __MAIN_ENTRY__
#define __MAIN_ENTRY__
  #include <stdio.h>

  #define _(STRING) gettext(STRING)
  /* flags for command line parameters */
  typedef struct {
    unsigned f_help      : 1;
    unsigned f_logfile   : 1;
    unsigned f_xmode	 : 1;
    unsigned f_reserved  : 5;
  } __attribute__((packed)) Oflags;

  /* error message print */
  void error(int, char*, ...);
  
  extern FILE *output_stream;
  extern int debug_mode;

#endif
