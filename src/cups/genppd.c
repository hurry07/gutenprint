/*
 * "$Id: genppd.c,v 1.26 2001/09/06 20:24:59 easysw Exp $"
 *
 *   PPD file generation program for the CUPS drivers.
 *
 *   Copyright 1993-2001 by Easy Software Products.
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License,
 *   version 2, as published by the Free Software Foundation.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, please contact Easy Software
 *   Products at:
 *
 *       Attn: CUPS Licensing Information
 *       Easy Software Products
 *       44141 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 *   main()                   - Process files on the command-line...
 *   initialize_stp_options() - Initialize the min/max values for
 *                              each STP numeric option.
 *   usage()                  - Show program usage...
 *   write_ppd()              - Write a PPD file.
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <zlib.h>

#include <cups/cups.h>
#include <cups/raster.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef INCLUDE_GIMP_PRINT_H
#include INCLUDE_GIMP_PRINT_H
#else
#include <gimp-print/gimp-print.h>
#endif
#include <gimp-print/gimp-print-intl.h>
#include "../../lib/libprintut.h"

/*
 * File handling stuff...
 */

#ifdef HAVE_LIBZ
#  define PPDEXT ".ppd.gz"
#else
#  define PPDEXT ".ppd"
#  define gzFile FILE *
#  define gzopen fopen
#  define gzclose fclose
#  define gzprintf fprintf
#  define gzputs(f,s) fputs((s),(f))
#  define gzputc(f,c) putc((c),(f))
#endif /* HAVE_LIBZ */


/*
 * Size data...
 */

#define DEFAULT_SIZE	"Letter"
/*#define DEFAULT_SIZE	"A4"*/

typedef struct				/**** Media size values ****/
{
  const char	*name,			/* Media size name */
		*text;			/* Media size text */
  int		width,			/* Media width */
		height,			/* Media height */
		left,			/* Media left margin */
		right,			/* Media right margin */
		bottom,			/* Media bottom margin */
		top;			/* Media top margin */
} paper_t;


/*
 * STP option data...
 */

static struct				/**** STP numeric options ****/
{
  const char	*name,			/* Name of option */
    		*text;			/* Human-readable text */
  int		low,			/* Low value (thousandths) */
    		high,			/* High value (thousandths) */
	        defval,			/* Default value */
    		step;			/* Step (thousandths) */
}		stp_options[] =
{
  { "stpBrightness",	"Brightness" },
  { "stpContrast",	"Contrast" },
  { "stpGamma",		"Gamma" },
  { "stpDensity",	"Density" },
  { "stpCyan",		"Cyan" },
  { "stpMagenta",	"Magenta" },
  { "stpYellow",	"Yellow" },
  { "stpSaturation",	"Saturation" }
};


/*
 * Local functions...
 */

void	initialize_stp_options(void);
void	usage(void);
int	write_ppd(const stp_printer_t p, const char *prefix,
	          const char *language);


/*
 * 'main()' - Process files on the command-line...
 */

int				/* O - Exit status */
main(int  argc,			/* I - Number of command-line arguments */
     char *argv[])		/* I - Command-line arguments */
{
  int		i;		/* Looping var */
  int		option_index;	/* Option index */
  const char	*prefix;	/* Directory prefix for output */
  const char	*language;	/* Language */
  const char    *catalog = NULL;/* Catalog location */
  stp_printer_t	printer;	/* Pointer to printer driver */
  static struct option long_options[] =
		{		/* Command-line options */
		  /* name,	has_arg,		flag	val */
		  {"help",	no_argument,		0,	0},
		  {"catalog",	required_argument,	0,	0},
		  {"language",	required_argument,	0,	0},
		  {"prefix",	required_argument,	0,	0},
		  {0,		0,			0,	0}
		};


 /*
  * Initialise libgimpprint
  */

  stp_init();

 /*
  * Parse command-line args...
  */

  prefix   = "ppd";
  language = "C";

  initialize_stp_options();

  option_index = 0;

  for (;;)
  {
    if ((i = getopt_long_only(argc, argv, "", long_options,
                              &option_index)) == -1)
      break;

    switch (i)
    {
      case 0:
	  /* option already dealt with, so skip to next argv entry */
          if (long_options[option_index].flag != 0)
            break;

	  if (strncmp(long_options[option_index].name, "help", 4) == 0)
          {
	    usage();
	    break;
          }

	  if (strncmp(long_options[option_index].name, "language", 8) == 0)
          {
	    language = optarg;
	    break;
          }

	  if (strncmp(long_options[option_index].name, "catalog", 7) == 0)
          {
	    catalog = optarg;
	    break;
          }

	  if (strncmp(long_options[option_index].name, "prefix", 6) == 0)
          {
	    prefix = optarg;
	    break;
	  }

      default:
          usage();
	  break;
    }
  }
  

 /*
  * Set the language...
  */

  setlocale(LC_MESSAGES, language);

 /*
  * Set up the catalog
  */

  if (catalog)
  {
    if ((bindtextdomain(PACKAGE, catalog)) == NULL)
    {
      fprintf(stderr, "genppd: cannot load message catalog %s: %s\n", catalog,
              strerror(errno));
      exit(1);
    }
  }

 /*
  * Write PPD files...
  */

  for (i = 0; i < stp_known_printers(); i++)
  {
    printer = stp_get_printer_by_index(i);

    if (printer && write_ppd(printer, prefix, language))
      return (1);
  }

  return (0);
}


/*
 * 'initialize_stp_options()' - Initialize the min/max values for
 *                              each STP numeric option.
 */

void
initialize_stp_options(void)
{
  const stp_vars_t lower = stp_minimum_settings();
  const stp_vars_t upper = stp_maximum_settings();
  const stp_vars_t defvars = stp_default_settings();


  stp_options[0].low = 1000 * stp_get_brightness(lower);
  stp_options[0].high = 1000 * stp_get_brightness(upper);
  stp_options[0].defval = 1000 * stp_get_brightness(defvars);
  stp_options[0].step = 50;

  stp_options[1].low = 1000 * stp_get_contrast(lower);
  stp_options[1].high = 1000 * stp_get_contrast(upper);
  stp_options[1].defval = 1000 * stp_get_contrast(defvars);
  stp_options[1].step = 50;

  stp_options[2].low = 1000 * stp_get_gamma(lower);
  stp_options[2].high = 1000 * stp_get_gamma(upper);
  stp_options[2].defval = 1000 * stp_get_gamma(defvars);
  stp_options[2].step = 50;

  stp_options[3].low = 1000 * stp_get_density(lower);
  stp_options[3].high = 1000 * stp_get_density(upper);
  stp_options[3].defval = 1000 * stp_get_density(defvars);
  stp_options[3].step = 50;

  stp_options[4].low = 1000 * stp_get_cyan(lower);
  stp_options[4].high = 1000 * stp_get_cyan(upper);
  stp_options[4].defval = 1000 * stp_get_cyan(defvars);
  stp_options[4].step = 50;

  stp_options[5].low = 1000 * stp_get_magenta(lower);
  stp_options[5].high = 1000 * stp_get_magenta(upper);
  stp_options[5].defval = 1000 * stp_get_magenta(defvars);
  stp_options[5].step = 50;

  stp_options[6].low = 1000 * stp_get_yellow(lower);
  stp_options[6].high = 1000 * stp_get_yellow(upper);
  stp_options[6].defval = 1000 * stp_get_yellow(defvars);
  stp_options[6].step = 50;

  stp_options[7].low = 1000 * stp_get_saturation(lower);
  stp_options[7].high = 1000 * stp_get_saturation(upper);
  stp_options[7].defval = 1000 * stp_get_saturation(defvars);
  stp_options[7].step = 50;
}


/*
 * 'usage()' - Show program usage...
 */

void
usage(void)
{
  fputs("Usage: genppd [--help] [--catalog=domain] "
        "[--language=locale] [--prefix=dir]\n", stderr);

  exit(EXIT_FAILURE);
}


/*
 * 'write_ppd()' - Write a PPD file.
 */

int					/* O - Exit status */
write_ppd(const stp_printer_t p,	/* I - Printer driver */
	  const char          *prefix,	/* I - Prefix (directory) for PPD files */
	  const char          *language)/* I - Language/locale */
{
  int		i, j;			/* Looping vars */
  gzFile	fp;			/* File to write to */
  char		filename[1024];		/* Filename */
  char		manufacturer[64];	/* Manufacturer name */
  int		num_opts;		/* Number of printer options */
  stp_param_t	*opts;			/* Printer options */
  const char	*defopt;		/* Default printer option */
  int		xdpi, ydpi;		/* Resolution info */
  stp_vars_t	v;			/* Variable info */
  int		width, height,		/* Page information */
		bottom, left,
		top, right;
  const char	*driver;		/* Driver name */
  const char	*long_name;		/* Driver long name */
  stp_vars_t	printvars;		/* Printer option names */
  int		model;			/* Driver model number */
  const stp_printfuncs_t *printfuncs;	/* Driver functions */
  paper_t	*the_papers;		/* Media sizes */
  int		cur_opt;		/* Current option */


 /*
  * Initialize driver-specific variables...
  */

  driver     = stp_printer_get_driver(p);
  long_name  = stp_printer_get_long_name(p);
  printvars  = stp_printer_get_printvars(p);
  model      = stp_printer_get_model(p);
  printfuncs = stp_printer_get_printfuncs(p);
  the_papers = NULL;
  cur_opt    = 0;

 /*
  * Skip the PostScript drivers...
  */

  if (strcmp(driver, "ps") == 0 ||
      strcmp(driver, "ps2") == 0)
    return (0);

 /*
  * Make sure the destination directory exists...
  */

  mkdir(prefix, 0777);
  sprintf(filename, "%s/%s" PPDEXT, prefix, driver);

 /*
  * Open the PPD file...
  */

  if ((fp = gzopen(filename, "wb")) == NULL)
  {
    fprintf(stderr, "genppd: Unable to create file \"%s\" - %s.\n",
            filename, strerror(errno));
    return (2);
  }

 /*
  * Write a standard header...
  */

  sscanf(long_name, "%63s", manufacturer);

  fprintf(stderr, "Writing %s...\n", filename);

  gzputs(fp, "*PPD-Adobe: \"4.3\"\n");
  gzputs(fp, "*%PPD file for CUPS/GIMP-print.\n");
  gzputs(fp, "*%Copyright 1993-2001 by Easy Software Products, All Rights Reserved.\n");
  gzputs(fp, "*%This PPD file may be freely used and distributed under the terms of\n");
  gzputs(fp, "*%the GNU GPL.\n");
  gzputs(fp, "*FormatVersion:	\"4.3\"\n");
  gzputs(fp, "*FileVersion:	\"" VERSION "\"\n");
  gzprintf(fp, "*LanguageVersion: %s\n", _("English"));
  gzprintf(fp, "*LanguageEncoding: %s\n", _("ISOLatin1"));
  gzprintf(fp, "*PCFileName:	\"%s.ppd\"\n", driver);
  gzprintf(fp, "*Manufacturer:	\"%s\"\n", manufacturer);
  gzputs(fp, "*Product:	\"(GIMP-print v" VERSION ")\"\n");
  gzprintf(fp, "*ModelName:     \"%s\"\n", driver);
  gzprintf(fp, "*ShortNickName: \"%s\"\n", long_name);
  gzprintf(fp, "*NickName:      \"%s, CUPS+GIMP-print v" VERSION "\"\n", long_name);
  gzputs(fp, "*PSVersion:	\"(3010.000) 550\"\n");
  gzputs(fp, "*LanguageLevel:	\"3\"\n");
  gzprintf(fp, "*ColorDevice:	%s\n",
           stp_get_output_type(printvars) == OUTPUT_COLOR ? "True" : "False");
  gzprintf(fp, "*DefaultColorSpace: %s\n",
           stp_get_output_type(printvars) == OUTPUT_COLOR ? "RGB" : "Gray");
  gzputs(fp, "*FileSystem:	False\n");
  gzputs(fp, "*LandscapeOrientation: Plus90\n");
  gzputs(fp, "*TTRasterizer:	Type42\n");

  gzputs(fp, "*cupsVersion:	1.1\n");
  gzprintf(fp, "*cupsModelNumber: \"%d\"\n", model);
  gzputs(fp, "*cupsManualCopies: True\n");
  gzputs(fp, "*cupsFilter:	\"application/vnd.cups-raster 100 rastertoprinter\"\n");
  if (strcasecmp(manufacturer, "EPSON") == 0)
    gzputs(fp, "*cupsFilter:	\"application/vnd.cups-command 33 commandtoepson\"\n");

 /*
  * Get the page sizes from the driver...
  */

  v = stp_allocate_copy(printvars);

  opts = (*(printfuncs->parameters))(p, NULL, "PageSize", &num_opts);
  defopt = (*(printfuncs->default_parameters))(p, NULL, "PageSize");
  the_papers = malloc(sizeof(paper_t) * num_opts);

  for (i = 0; i < num_opts; i++)
  {
    const stp_papersize_t papersize = stp_get_papersize_by_name(opts[i].name);

    if (!papersize)
    {
      printf("Unable to lookup size %s!\n", opts[i].name);
      continue;
    }

    width  = stp_papersize_get_width(papersize);
    height = stp_papersize_get_height(papersize);

    if (width <= 0 || height <= 0)
      continue;

    stp_set_media_size(v, opts[i].name);

    (*(printfuncs->media_size))(p, v, &width, &height);
    (*(printfuncs->imageable_area))(p, v, &left, &right, &bottom, &top);

    the_papers[cur_opt].name   = opts[i].name;
    the_papers[cur_opt].text   = opts[i].text;
    the_papers[cur_opt].width  = width;
    the_papers[cur_opt].height = height;
    the_papers[cur_opt].left   = left;
    the_papers[cur_opt].right  = right;
    the_papers[cur_opt].bottom = bottom;
    the_papers[cur_opt].top    = top;

    cur_opt++;
  }

  gzputs(fp, "*OpenUI *PageSize: PickOne\n");
  gzputs(fp, "*OrderDependency: 10 AnySetup *PageSize\n");
  gzputs(fp, "*DefaultPageSize: ");
  gzputs(fp, defopt);
  gzputs(fp, "\n");
  for (i = 0; i < cur_opt; i ++)
  {
    gzprintf(fp,  "*PageSize %s", the_papers[i].name);
    gzprintf(fp, "/%s:\t\"<</PageSize[%d %d]/ImagingBBox null>>setpagedevice\"\n",
             the_papers[i].text, the_papers[i].width, the_papers[i].height);
  }
  gzputs(fp, "*CloseUI: *PageSize\n");

  gzputs(fp, "*OpenUI *PageRegion: PickOne\n");
  gzputs(fp, "*OrderDependency: 10 AnySetup *PageRegion\n");
  gzputs(fp, "*DefaultPageRegion: ");
  gzputs(fp, defopt);
  gzputs(fp, "\n");
  for (i = 0; i < cur_opt; i ++)
  {
    gzprintf(fp,  "*PageRegion %s", the_papers[i].name);
    gzprintf(fp, "/%s:\t\"<</PageRegion[%d %d]/ImagingBBox null>>setpagedevice\"\n",
	     the_papers[i].text, the_papers[i].width, the_papers[i].height);
  }
  gzputs(fp, "*CloseUI: *PageRegion\n");

  gzputs(fp, "*DefaultImageableArea: ");
  gzputs(fp, defopt);
  gzputs(fp, "\n");
  for (i = 0; i < cur_opt; i ++)
  {
    gzprintf(fp,  "*ImageableArea %s", the_papers[i].name);
    gzprintf(fp, "/%s:\t\"%d %d %d %d\"\n", the_papers[i].text,
             the_papers[i].left, the_papers[i].bottom,
	     the_papers[i].right, the_papers[i].top);
  }

  gzputs(fp, "*DefaultPaperDimension: ");
  gzputs(fp, defopt);
  gzputs(fp, "\n");
  for (i = 0; i < cur_opt; i ++)
  {
    gzprintf(fp, "*PaperDimension %s", the_papers[i].name);
    gzprintf(fp, "/%s:\t\"%d %d\"\n",
	     the_papers[i].text, the_papers[i].width, the_papers[i].height);
  }

  if (opts)
  {
    for (i = 0; i < num_opts; i++)
    {
      free((void *)opts[i].name);
      free((void *)opts[i].text);
    }

    free(opts);
  }

  if (the_papers)
    free(the_papers);

 /*
  * Do we support color?
  */

  gzputs(fp, "*OpenUI *ColorModel: PickOne\n");
  gzputs(fp, "*OrderDependency: 10 AnySetup *ColorModel\n");

  if (stp_get_output_type(printvars) == OUTPUT_COLOR)
    gzputs(fp, "*DefaultColorModel: RGB\n");
  else
    gzputs(fp, "*DefaultColorModel: Gray\n");

  gzprintf(fp, "*ColorModel Gray/Grayscale:\t\"<<"
               "/cupsColorSpace %d"
	       "/cupsColorOrder %d"
	       "/cupsBitsPerColor 8>>setpagedevice\"\n",
           CUPS_CSPACE_W, CUPS_ORDER_CHUNKED);
  gzprintf(fp, "*ColorModel Black/Black & White:\t\"<<"
               "/cupsColorSpace %d"
	       "/cupsColorOrder %d"
	       "/cupsBitsPerColor 8>>setpagedevice\"\n",
           CUPS_CSPACE_K, CUPS_ORDER_CHUNKED);

  if (stp_get_output_type(printvars) == OUTPUT_COLOR)
  {
    gzprintf(fp, "*ColorModel RGB/Color:\t\"<<"
                 "/cupsColorSpace %d"
		 "/cupsColorOrder %d"
		 "/cupsBitsPerColor 8>>setpagedevice\"\n",
             CUPS_CSPACE_RGB, CUPS_ORDER_CHUNKED);
    gzprintf(fp, "*ColorModel CMYK/Raw CMYK:\t\"<<"
                 "/cupsColorSpace %d"
		 "/cupsColorOrder %d"
		 "/cupsBitsPerColor 8>>setpagedevice\"\n",
             CUPS_CSPACE_CMYK, CUPS_ORDER_CHUNKED);
  }

  gzputs(fp, "*CloseUI: *ColorModel\n");

 /*
  * Media types...
  */

  opts   = (*(printfuncs->parameters))(p, NULL, "MediaType", &num_opts);
  defopt = (*(printfuncs->default_parameters))(p, NULL, "MediaType");

  if (num_opts > 0)
  {
    gzputs(fp, "*OpenUI *MediaType: PickOne\n");
    gzputs(fp, "*OrderDependency: 10 AnySetup *MediaType\n");
    gzprintf(fp, "*DefaultMediaType: %s\n", defopt);

    for (i = 0; i < num_opts; i ++)
    {
      gzprintf(fp, "*MediaType %s/%s:\t\"<</MediaType(%s)>>setpagedevice\"\n",
               opts[i].name, opts[i].text, opts[i].name);
      free((void *)opts[i].name);
      free((void *)opts[i].text);
    }

    free(opts);

    gzputs(fp, "*CloseUI: *MediaType\n");
  }

 /*
  * Input slots...
  */

  opts   = (*(printfuncs->parameters))(p, NULL, "InputSlot", &num_opts);
  defopt = (*(printfuncs->default_parameters))(p, NULL, "InputSlot");

  if (num_opts > 0)
  {
    gzputs(fp, "*OpenUI *InputSlot: PickOne\n");
    gzputs(fp, "*OrderDependency: 10 AnySetup *InputSlot\n");
    gzprintf(fp, "*DefaultInputSlot: %s\n", defopt);

    for (i = 0; i < num_opts; i ++)
    {
      gzprintf(fp, "*InputSlot %s/%s:\t\"<</MediaClass(%s)>>setpagedevice\"\n",
               opts[i].name, opts[i].text, opts[i].name);
      free((void *)opts[i].name);
      free((void *)opts[i].text);
    }

    free(opts);

    gzputs(fp, "*CloseUI: *InputSlot\n");
  }

 /*
  * Resolutions...
  */

  opts   = (*(printfuncs->parameters))(p, NULL, "Resolution", &num_opts);
  defopt = (*(printfuncs->default_parameters))(p, NULL, "Resolution");

  gzputs(fp, "*OpenUI *Resolution: PickOne\n");
  gzputs(fp, "*OrderDependency: 20 AnySetup *Resolution\n");
  gzprintf(fp, "*DefaultResolution: %s\n", defopt);

  for (i = 0; i < num_opts; i ++)
  {
   /*
    * Strip resolution name to its essentials...
    */

    (printfuncs->describe_resolution)(p, opts[i].name, &xdpi, &ydpi);

    /* This should not happen! */
    if (xdpi == -1 || ydpi == -1)
      continue;

   /*
    * Write the resolution option...
    */

    gzprintf(fp, "*Resolution %s/%s:\t\"<</HWResolution[%d %d]/cupsCompression %d>>setpagedevice\"\n",
             opts[i].name, opts[i].text, xdpi, ydpi, i);
    free((void *)opts[i].name);
    free((void *)opts[i].text);
  }

  free(opts);

  gzputs(fp, "*CloseUI: *Resolution\n");

 /*
  * STP option group...
  */

  gzputs(fp, "*OpenGroup: STP\n");

   /*
    * Image types...
    */

    gzputs(fp, "*OpenUI *stpImageType/Image Type: PickOne\n");
    gzputs(fp, "*OrderDependency: 10 AnySetup *stpImageType\n");
    gzputs(fp, "*DefaultstpImageType: LineArt\n");

    gzprintf(fp, "*stpImageType LineArt/Line Art:\t\"<</cupsRowCount 0>>setpagedevice\"\n");
    gzprintf(fp, "*stpImageType SolidTone/Solid Tone:\t\"<</cupsRowCount 1>>setpagedevice\"\n");
    gzprintf(fp, "*stpImageType Continuous/Photograph:\t\"<</cupsRowCount 2>>setpagedevice\"\n");

    gzputs(fp, "*CloseUI: *stpImageType\n");

   /*
    * Dithering algorithms...
    */

    gzputs(fp, "*OpenUI *stpDither/Dither Algorithm: PickOne\n");
    gzputs(fp, "*OrderDependency: 10 AnySetup *stpDither\n");
    gzprintf(fp, "*DefaultstpDither: %s\n", stp_default_dither_algorithm());

    for (i = 0; i < stp_dither_algorithm_count(); i ++)
      gzprintf(fp, "*stpDither %s/%s: \"<</cupsRowStep %d>>setpagedevice\"\n",
               stp_dither_algorithm_name(i), stp_dither_algorithm_text(i), i);

    gzputs(fp, "*CloseUI: *stpDither\n");

   /*
    * InkTypes...
    */

    opts   = (*(printfuncs->parameters))(p, NULL, "InkType", &num_opts);
    defopt = (*(printfuncs->default_parameters))(p, NULL, "InkType");

    if (num_opts > 0)
    {
      gzputs(fp, "*OpenUI *stpInkType/Ink Type: PickOne\n");
      gzputs(fp, "*OrderDependency: 20 AnySetup *stpInkType\n");
      gzprintf(fp, "*DefaultstpInkType: %s\n", defopt);

      for (i = 0; i < num_opts; i ++)
      {
       /*
	* Write the inktype option...
	*/

	gzprintf(fp, "*stpInkType %s/%s:\t\"<</OutputType(%s)>>setpagedevice\"\n",
        	 opts[i].name, opts[i].text, opts[i].name);
	free((void *)opts[i].name);
	free((void *)opts[i].text);
      }

      free(opts);

      gzputs(fp, "*CloseUI: *stpInkType\n");
    }

   /*
    * Advanced STP options...
    */

    if (stp_get_output_type(printvars) == OUTPUT_COLOR)
      num_opts = 8;
    else
      num_opts = 4;

    for (i = 0; i < num_opts; i ++)
    {
      gzprintf(fp, "*OpenUI *%s/%s: PickOne\n", stp_options[i].name,
               stp_options[i].text);
      gzprintf(fp, "*Default%s: 1000\n", stp_options[i].name);
      for (j = stp_options[i].low;
           j <= stp_options[i].high;
	   j += stp_options[i].step)
	gzprintf(fp, "*%s %d/%.3f: \"\"\n", stp_options[i].name, j, j * 0.001);
      gzprintf(fp, "*CloseUI *%s\n", stp_options[i].name);
    }

 /*
  * End of STP option group...
  */

  gzputs(fp, "*CloseGroup: STP\n");

 /*
  * Fonts...
  */

  gzputs(fp, "*DefaultFont: Courier\n");
  gzputs(fp, "*Font AvantGarde-Book: Standard \"(001.006S)\" Standard ROM\n");
  gzputs(fp, "*Font AvantGarde-BookOblique: Standard \"(001.006S)\" Standard ROM\n");
  gzputs(fp, "*Font AvantGarde-Demi: Standard \"(001.007S)\" Standard ROM\n");
  gzputs(fp, "*Font AvantGarde-DemiOblique: Standard \"(001.007S)\" Standard ROM\n");
  gzputs(fp, "*Font Bookman-Demi: Standard \"(001.004S)\" Standard ROM\n");
  gzputs(fp, "*Font Bookman-DemiItalic: Standard \"(001.004S)\" Standard ROM\n");
  gzputs(fp, "*Font Bookman-Light: Standard \"(001.004S)\" Standard ROM\n");
  gzputs(fp, "*Font Bookman-LightItalic: Standard \"(001.004S)\" Standard ROM\n");
  gzputs(fp, "*Font Courier: Standard \"(002.004S)\" Standard ROM\n");
  gzputs(fp, "*Font Courier-Bold: Standard \"(002.004S)\" Standard ROM\n");
  gzputs(fp, "*Font Courier-BoldOblique: Standard \"(002.004S)\" Standard ROM\n");
  gzputs(fp, "*Font Courier-Oblique: Standard \"(002.004S)\" Standard ROM\n");
  gzputs(fp, "*Font Helvetica: Standard \"(001.006S)\" Standard ROM\n");
  gzputs(fp, "*Font Helvetica-Bold: Standard \"(001.007S)\" Standard ROM\n");
  gzputs(fp, "*Font Helvetica-BoldOblique: Standard \"(001.007S)\" Standard ROM\n");
  gzputs(fp, "*Font Helvetica-Narrow: Standard \"(001.006S)\" Standard ROM\n");
  gzputs(fp, "*Font Helvetica-Narrow-Bold: Standard \"(001.007S)\" Standard ROM\n");
  gzputs(fp, "*Font Helvetica-Narrow-BoldOblique: Standard \"(001.007S)\" Standard ROM\n");
  gzputs(fp, "*Font Helvetica-Narrow-Oblique: Standard \"(001.006S)\" Standard ROM\n");
  gzputs(fp, "*Font Helvetica-Oblique: Standard \"(001.006S)\" Standard ROM\n");
  gzputs(fp, "*Font NewCenturySchlbk-Bold: Standard \"(001.009S)\" Standard ROM\n");
  gzputs(fp, "*Font NewCenturySchlbk-BoldItalic: Standard \"(001.007S)\" Standard ROM\n");
  gzputs(fp, "*Font NewCenturySchlbk-Italic: Standard \"(001.006S)\" Standard ROM\n");
  gzputs(fp, "*Font NewCenturySchlbk-Roman: Standard \"(001.007S)\" Standard ROM\n");
  gzputs(fp, "*Font Palatino-Bold: Standard \"(001.005S)\" Standard ROM\n");
  gzputs(fp, "*Font Palatino-BoldItalic: Standard \"(001.005S)\" Standard ROM\n");
  gzputs(fp, "*Font Palatino-Italic: Standard \"(001.005S)\" Standard ROM\n");
  gzputs(fp, "*Font Palatino-Roman: Standard \"(001.005S)\" Standard ROM\n");
  gzputs(fp, "*Font Symbol: Special \"(001.007S)\" Special ROM\n");
  gzputs(fp, "*Font Times-Bold: Standard \"(001.007S)\" Standard ROM\n");
  gzputs(fp, "*Font Times-BoldItalic: Standard \"(001.009S)\" Standard ROM\n");
  gzputs(fp, "*Font Times-Italic: Standard \"(001.007S)\" Standard ROM\n");
  gzputs(fp, "*Font Times-Roman: Standard \"(001.007S)\" Standard ROM\n");
  gzputs(fp, "*Font ZapfChancery-MediumItalic: Standard \"(001.007S)\" Standard ROM\n");
  gzputs(fp, "*Font ZapfDingbats: Special \"(001.004S)\" Standard ROM\n");

  gzprintf(fp, "*%%End of %s.ppd\n", driver);

  gzclose(fp);

  stp_free_vars(v);
  return (0);
}

/*
 * End of "$Id: genppd.c,v 1.26 2001/09/06 20:24:59 easysw Exp $".
 */
