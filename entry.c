/**
 * @file entry.c
 *
 * @brief This is the main module, it handles command line parameters and executes defragmentation.
 *
 * At first, text domain is set up according to LOCALE setting, then the command line parameters are parsed using
 * getopt_long function. All program messages (besides error messages and progress bar) are written into standard
 * output stream (defined by pointer output_stream) that is on the start set up to stdout. If -l (or -log_file) parameter
 * is used, then messages are re-directed into the log file that is given as first argument of this parameter.
 *
 * @section Options Description of command line parameters
 * - -h (or --help)                     - Shows information of program usage
 * - -l logfile (or --log_file logfile) - Redirects program messages into log file
 * - -x (or --xmode)                    - Forces the program to work in debug mode (it shows many additional informations)
 *
 */

/* The module I've started to write at day: 31.10.2006 
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

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <libintl.h>
#include <locale.h>

#include <version.h>
#include <entry.h>
#include <fat32.h>
#include <analyze.h>
#include <defrag.h>
#include "mainpage.h"

/** Name of the program */
const char *program_name;
/** Output stream (either stdout, or log file) */
FILE *output_stream;

/** If the debud mode is used */
int debug_mode = 0;

/** The procedure prints message of program usage and exits. 
 *  @param stream define stream where the messages should be written to
 *  @param exit_code defines exit code by which the program will end
 */
void print_usage(FILE *stream, int exit_code)
{
  fprintf(stream, _("Syntax: %s options image_file\n"), program_name);
  fprintf(stream, _("  -h  --help\t\t\tShows this information\n"
		    "  -l  --log_file nazov_suboru\tSet program output to log file\n"
		    "  -x  --xmode\t\t\tWork in X mode (debug mode)\n"
                    "  -a  --analyze\t\t\tAnalyze only (not defragment)\n"));
  exit(exit_code);
}

/** It shows error message into stderr stream and according to setting of p_usage parameter show program usage.
 *  @param p_usage if print_usage function should be called
 *  @param message error message
 */
void error(int p_usage, char *message, ...)
{
  va_list args;
  fprintf(stderr, _("\nERROR: "));
  
  va_start(args, message);
  vfprintf(stderr,message, args);
  va_end(args);
  fprintf(stderr,"\n");
  
  if (p_usage)
    print_usage(stderr, 1);
  else exit(1);
}

/** Main function
 * @brief it sets upt message domain, parse the command line parameters, performs disk fragmentation analysis and calls
 * defragmentation function.
 * The message domain is taken from default directory /usr/share/locale, and it is called f32id_loc.
 * For parsing the command line parameters (as short ones, as long ones and their arguments), there is used getopt_long
 * function that it performs it for me :-)
 *
 * If the -h switch was defined (shows program usage), then other parameters are ignored (instead of -l switch)
 * and the program is terminated. In other cases the program continues with fragmentartion analysis and by the
 * defragmentation itself (in the case of need). Defragmentation will be executed only if the disk is fragmented of
 * minimal 1%.
 */
int main(int argc, char *argv[])
{
  int next_option; 				/* next parameter */
  int image_descriptor = 0;			/* file descriptor of image */
  const char *log_filename = NULL;		/* name of log file */
  const char* const short_options = "hl:xa";	/* string of short parameter names */

  /* Array of structures that describes long parameter names */
  const struct option long_options[] = {
    { "help",		0, NULL, 'h' },
    { "log_file",	1, NULL, 'l' },
    { "xmode",		0, NULL, 'x' },
    { "analyze",        0, NULL, 'a' },
    { NULL,		0, NULL, 0 }		/* Needed for to determine end of the array */
  };
  Oflags flags = { 0,0,0,0,0 };			/* flags of the program switches */

  /* Sets up the message domain */
  setlocale(LC_ALL, "");
  bindtextdomain("f32id_loc", "/usr/share/locale"); /* default is /usr/share/locale */
  textdomain("f32id_loc");

  output_stream = stdout;			/* for start, the output is on the screen */
  /* Real name of the program is stored in order to be possible to use it in various messages of the program */
  program_name = argv[0];
  opterr = 0; /* Force the getopt_long function for not showing error messages */
  do {
    next_option = getopt_long(argc, argv, short_options, long_options, NULL);
    switch (next_option) {
      case 'h':  /* -h or --help */
        flags.f_help = 1;
	break;
      case 'l':  /* -l or --log_file */
        log_filename = optarg;
	flags.f_logfile = 1;
        break;
      case 'x':  /* -x or --xmode */
        flags.f_xmode = 1;
        break;
      case 'a': /* -a or --analyze */
        flags.f_analyze = 1;
        break;
      case '?':
        /* Wrong parameter */
	error(0,_("Wrong option, use -h or --help"));
      case -1:
        break;
      default:
        abort();
    }
  } while (next_option != -1);

  /* If log file is used, the stdout needed to be redirected to it */
  if (flags.f_logfile)
    if ((output_stream = fopen(log_filename,"w")) == NULL)
      error(0,_("Can't open log file: %s"), log_filename);

  fprintf(output_stream, "FAT32 Image Defragmenter v%s,\n(c) Copyright 2006, vbmacher <pjakubco@gmail.com>\n", __F32ID_VERSION__);
  if (flags.f_help)
    print_usage(output_stream, 0);

  if (flags.f_xmode)
    debug_mode = 1;
  else
    debug_mode = 0;

  /* Now the optind variable points at the first non-switch parameter;
   *  i.e. there should be one parameter - name of the file image
   */
  if (optind == argc)
    error(1,gettext("Missing argument - image file"));

  /* tries to open the image */
  if ((image_descriptor = open(argv[optind], O_RDWR)) == -1)
    error(0,gettext("Can't open image file (%s)"), argv[optind]);
  
  /* mounting the image */
  f32_mount(image_descriptor);

  /* analysis of the disk fragmentation */
  an_analyze();

  /* if the disk is fragmented from min. 1% */
  if (!flags.f_analyze) {
    if ((int)diskFragmentation > 0)
      /** the defragmentation itself */
      def_defragTable();
    else
      fprintf(output_stream, gettext("Disk doesn't need defragmentation.\n"));
  }

  /* un-mounting the image, freeing memory */
  an_freeTable();
  f32_umount();
  
  close(image_descriptor);
}
