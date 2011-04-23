/**
 * @file entry.c
 *
 * @brief Toto je hlavný modul, rozlí¹i parametre a spustí defragmentáciu.
 *
 * Najprv sa nastaví textová doména podla nastavenia LOCALE, potom sa rozparsujú prepínaèe pomocou
 * funkcie getopt_long. V¹etky hlásenia programu (okrem chybových a okrem progress baru) sa zapisujú
 * do ¹tandardného prúdu (definovaného smerníkom output_stream), ktorý je na zaèiatku nastavený na
 * stdout. V prípade, ak je pou¾itý prepínaè -l (alebo -log_file), tak sú hlásenia presmerované
 * do log súboru, ktorý je daný ako prvý argument tohto prepínaèa.
 *
 * @section Options Popis prepínaèov
 * - -h (alebo --help)                     - Zobrazí informácie o pou¾ití programu
 * - -l logfile (alebo --log_file logfile) - Presmeruje hlásenia programu do súboru logfile
 * - -x (alebo --xmode)                    - Program pracuje v debug re¾ime (vypisuje sa vela dodatoèných informácií)
 *
 */
/* Modul som zaèal písa» dòa: 31.10.2006 */

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

/** Názov programu */
const char *program_name;
/** Výstupný prúd (buï stdout, alebo log súbor) */
FILE *output_stream;

/** Èi je pou¾itý debug re¾im */
int debug_mode = 0;

/** Procedúra vypí¹e hlásenie o pou¾ití programu a ukonèí ho 
 *  @param stream definuje prúd, do ktorého sa bude zapisova»
 *  @param exit_code definuje chybový kód, ktorým sa program ukonèí
 */
void print_usage(FILE *stream, int exit_code)
{
  fprintf(stream, gettext("Syntax: %s options image_file\n"), program_name);
  fprintf(stream,
		    gettext("  -h  --help			Shows this information\n"
		    "  -l  --log_file nazov_suboru	Set program output to log file\n"
		    "  -x  --xmode			Work in X mode (debug mode)\n"));
  exit(exit_code);
}

/** Vypí¹e chybové hlásenie do stderr a podla nastavenia parametra p_usage
 *  vypíse help
 *  @param p_usage Èi sa má zavola» funkcia print_usage
 *  @param message Správa na vypísanie
 */
void error(int p_usage, char *message, ...)
{
  va_list args;
  fprintf(stderr, gettext("\nERROR: "));
  
  va_start(args, message);
  vfprintf(stderr,message, args);
  va_end(args);
  fprintf(stderr,"\n");
  
  if (p_usage)
    print_usage(stderr, 1);
  else exit(1);
}

/** Hlavná funkcia
 * @brief nastaví doménu správ, rozparsuje prepínaèe, vykoná analýzu fragmentácie disku a zavolá funkciu defragmentovania.
 * Doména správ sa berie z predvoleného adresára /usr/share/locale, a má názov f32id_loc.
 * Na rozparsovanie prepínaèov (ako krátkych, tak aj dlhých a prípadne ich argumenty) pou¾ívam funkciu getopt_long, ktorá
 * to urobí za mòa :-)
 *
 * Ak bol nastavený prepínaè -h (zobrazí pou¾itie programu), tak sa ïal¹ie parametre u¾ nerozoberajú (okrem prepínaèa -l)
 * a program po vypísaní daného hlásenia ukonèí svoju èinnos». V iných prípadoch program pokraèuje analýzou fragmentácie
 * a samotnou defragmentáciou (v prípade potreby). Defragmentácia sa zapne, iba ak je disk fragmentovaný z minimálne 1%.
 */
int main(int argc, char *argv[])
{
  int next_option; 				/* ïal¹í prepínaè */
  int image_descriptor = 0;			/* file deskriptor obrazu FS */
  const char *log_filename = NULL;		/* názov log súboru */
  const char* const short_options = "hl:x";	/* re»azec krátkych prepínaèov */

  /* Pole ¹truktúr popisujúce platné dlhé prepínaèe */
  const struct option long_options[] = {
    { "help",		0, NULL, 'h' },
    { "log_file",	1, NULL, 'l' },
    { "xmode",		0, NULL, 'x' },
    { NULL,		0, NULL, 0 }		/* Potrebne na konci pola */
  };
  Oflags flags = { 0,0,0,0 };			/* príznaky prepínaèov programu */

  /* Nastaví sa doména správ */
  setlocale(LC_ALL, "");
  bindtextdomain("f32id_loc", "/usr/share/locale"); /* default je /usr/share/locale */
  textdomain("f32id_loc");

  output_stream = stdout;			/* pre zaèiatok je výstup na obrazovku */
  /* Skutoèný názov programu sa zapamätá, aby mohol by» pou¾itý v rôznych hláseniach programu */
  program_name = argv[0];
  opterr = 0; /* Potlaèenie výpisu chybových hlásení funkcie getopt_long */
  do {
    next_option = getopt_long(argc, argv, short_options, long_options, NULL);
    switch (next_option) {
      case 'h':  /* -h alebo --help */
        flags.f_help = 1;
	break;
      case 'l':  /* -l alebo --log_file */
        log_filename = optarg;
	flags.f_logfile = 1;
        break;
      case 'x':  /* -x alebo --xmode */
        flags.f_xmode = 1;
        break;
      case '?':
        /* Neplatny prepinac */
	error(0,gettext("Wrong option, use -h or --help"));
      case -1:
        break;
      default:
        abort();
    }
  } while (next_option != -1);

  /* V prípade pou¾itia log súboru treba presmerova» do neho stdout */
  if (flags.f_logfile)
    if ((output_stream = fopen(log_filename,"w")) == NULL)
      error(0,gettext("Can't open log file: %s"), log_filename);

  fprintf(output_stream, "FAT32 Image Defragmenter v%s,\n(c) Copyright 2006, vbmacher <pjakubco@gmail.com>\n", __F32ID_VERSION__);
  if (flags.f_help)
    print_usage(output_stream, 0);

  if (flags.f_xmode)
    debug_mode = 1;
  else
    debug_mode = 0;

  /* Teraz premenná optind ukazuje na prvý nie-prepínaèový parameter;
   *  mal by nasledova» jeden parameter - názov súboru image-u
   */
  if (optind == argc)
    error(1,gettext("Missing argument - image file"));

  /* pokúsi sa otvori» image */
  if ((image_descriptor = open(argv[optind], O_RDWR)) == -1)
    error(0,gettext("Can't open image file (%s)"), argv[optind]);
  
  /* namontovanie obrazu */
  f32_mount(image_descriptor);

  /* analýza fragmentácie disku */
  an_analyze();

  /* ak je disk fragmentovaný z min. 1% */
  if ((int)diskFragmentation > 0)
    /** samotná defragmentácia */
    def_defragTable();
  else
    fprintf(output_stream, gettext("Disk doesn't need defragmentation.\n"));
  /* odmontovanie obrazu, uvolnenie pamäte */
  an_freeTable();
  f32_umount();
  
  close(image_descriptor);
}
