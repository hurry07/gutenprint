/*
 * "$Id: rastertoprinter.c,v 1.9.2.1 2001/02/20 04:41:32 rlk Exp $"
 *
 *   GIMP-print based raster filter for the Common UNIX Printing System.
 *
 *   Copyright 1993-2000 by Easy Software Products.
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
 *   main()                    - Main entry and processing of driver.
 *   Image_bpp()               - Return the bytes-per-pixel of an image.
 *   Image_get_appname()       - Get the application we are running.
 *   Image_get_row()           - Get one row of the image.
 *   Image_height()            - Return the height of an image.
 *   Image_init()              - Initialize an image.
 *   Image_note_progress()     - Notify the user of our progress.
 *   Image_progress_conclude() - Close the progress display.
 *   Image_progress_init()     - Initialize progress display.
 *   Image_rotate_ccw()        - Rotate the image counter-clockwise
 *                               (unsupported).
 *   Image_width()             - Return the width of an image.
 */

/*
 * Include necessary headers...
 */

#include <cups/cups.h>
#include <cups/ppd.h>
#include <cups/raster.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#ifdef INCLUDE_GIMP_PRINT_H
#include INCLUDE_GIMP_PRINT_H
#else
#include <gimp-print.h>
#endif
#include "../../lib/libprintut.h"

/*
 * Structure for page raster data...
 */

typedef struct
{
  cups_raster_t		*ras;		/* Raster stream to read from */
  int			page;		/* Current page number */
  int			row;		/* Current row number */
  cups_page_header_t	header;		/* Page header from file */
} cups_image_t;

static const char *Image_get_appname(stp_image_t *image);
static void Image_progress_conclude(stp_image_t *image);
static void Image_note_progress(stp_image_t *image,
				double current, double total);
static void Image_progress_init(stp_image_t *image);
static void Image_get_row(stp_image_t *image, unsigned char *data, int row);
static int Image_height(stp_image_t *image);
static int Image_width(stp_image_t *image);
static int Image_bpp(stp_image_t *image);
static void Image_rotate_180(stp_image_t *image);
static void Image_rotate_cw(stp_image_t *image);
static void Image_rotate_ccw(stp_image_t *image);
static void Image_init(stp_image_t *image);

static stp_image_t theImage =
{
  Image_init,
  NULL,				/* reset */
  NULL,				/* transpose */
  NULL,				/* hflip */
  NULL,				/* vflip */
  NULL,				/* crop */
  Image_rotate_ccw,
  Image_rotate_cw,
  Image_rotate_180,
  Image_bpp,
  Image_width,
  Image_height,
  Image_get_row,
  Image_get_appname,
  Image_progress_init,
  Image_note_progress,
  Image_progress_conclude,
  NULL
};

static void
cups_writefunc(void *file, const char *buf, size_t bytes)
{
  FILE *prn = (FILE *)file;
  fwrite(buf, 1, bytes, prn);
}

/*
 * 'main()' - Main entry and processing of driver.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  int			fd;		/* File descriptor */
  cups_image_t		cups;		/* CUPS image */
  const char		*ppdfile;	/* PPD environment variable */
  ppd_file_t		*ppd;		/* PPD file */
  const stp_printer_t	*printer;	/* Printer driver */  
  stp_vars_t		v;		/* Printer driver variables */
  const stp_papersize_t	*size;		/* Paper size */
  char			*buffer;	/* Overflow buffer */
  int		num_opts;		/* Number of printer options */
  char		**opts;			/* Printer options */

  theImage.rep = &cups;
 /*
  * Check for valid arguments...
  */

  if (argc < 6 || argc > 7)
  {
   /*
    * We don't have the correct number of arguments; write an error message
    * and return.
    */

    fputs("ERROR: rastertoprinter job-id user title copies options [file]\n", stderr);
    return (1);
  }

 /*
  * Get the PPD file and figure out which driver to use...
  */

  if ((ppdfile = getenv("PPD")) == NULL)
  {
    fputs("ERROR: Fatal error: PPD environment variable not set!\n", stderr);
    return (1);
  }

  if ((ppd = ppdOpenFile(ppdfile)) == NULL)
  {
    fprintf(stderr, "ERROR: Fatal error: Unable to load PPD file \"%s\"!\n",
            ppdfile);
    return (1);
  }

  if (ppd->modelname == NULL)
  {
    fprintf(stderr, "ERROR: Fatal error: No ModelName attribute in PPD file \"%s\"!\n",
            ppdfile);
    ppdClose(ppd);
    return (1);
  }

  if ((printer = stp_get_printer_by_driver(ppd->modelname)) == NULL)
  {
    fprintf(stderr, "ERROR: Fatal error: Unable to find driver named \"%s\"!\n",
            ppd->modelname);
    ppdClose(ppd);
    return (1);
  }

  ppdClose(ppd);

 /*
  * Open the page stream...
  */

  if (argc == 7)
  {
    if ((fd = open(argv[6], O_RDONLY)) == -1)
    {
      perror("ERROR: Unable to open raster file - ");
      sleep(1);
      return (1);
    }
  }
  else
    fd = 0;

  cups.ras = cupsRasterOpen(fd, CUPS_RASTER_READ);

 /*
  * Process pages as needed...
  */

  cups.page = 0;

  while (cupsRasterReadHeader(cups.ras, &cups.header))
  {
   /*
    * Update the current page...
    */

    cups.page ++;
    cups.row = 0;

   /*
    * Debugging info...
    */

    fprintf(stderr, "DEBUG: StartPage...\n");
    fprintf(stderr, "DEBUG: MediaClass = \"%s\"\n", cups.header.MediaClass);
    fprintf(stderr, "DEBUG: MediaColor = \"%s\"\n", cups.header.MediaColor);
    fprintf(stderr, "DEBUG: MediaType = \"%s\"\n", cups.header.MediaType);
    fprintf(stderr, "DEBUG: OutputType = \"%s\"\n", cups.header.OutputType);

    fprintf(stderr, "DEBUG: AdvanceDistance = %d\n", cups.header.AdvanceDistance);
    fprintf(stderr, "DEBUG: AdvanceMedia = %d\n", cups.header.AdvanceMedia);
    fprintf(stderr, "DEBUG: Collate = %d\n", cups.header.Collate);
    fprintf(stderr, "DEBUG: CutMedia = %d\n", cups.header.CutMedia);
    fprintf(stderr, "DEBUG: Duplex = %d\n", cups.header.Duplex);
    fprintf(stderr, "DEBUG: HWResolution = [ %d %d ]\n", cups.header.HWResolution[0],
            cups.header.HWResolution[1]);
    fprintf(stderr, "DEBUG: ImagingBoundingBox = [ %d %d %d %d ]\n",
            cups.header.ImagingBoundingBox[0], cups.header.ImagingBoundingBox[1],
            cups.header.ImagingBoundingBox[2], cups.header.ImagingBoundingBox[3]);
    fprintf(stderr, "DEBUG: InsertSheet = %d\n", cups.header.InsertSheet);
    fprintf(stderr, "DEBUG: Jog = %d\n", cups.header.Jog);
    fprintf(stderr, "DEBUG: LeadingEdge = %d\n", cups.header.LeadingEdge);
    fprintf(stderr, "DEBUG: Margins = [ %d %d ]\n", cups.header.Margins[0],
            cups.header.Margins[1]);
    fprintf(stderr, "DEBUG: ManualFeed = %d\n", cups.header.ManualFeed);
    fprintf(stderr, "DEBUG: MediaPosition = %d\n", cups.header.MediaPosition);
    fprintf(stderr, "DEBUG: MediaWeight = %d\n", cups.header.MediaWeight);
    fprintf(stderr, "DEBUG: MirrorPrint = %d\n", cups.header.MirrorPrint);
    fprintf(stderr, "DEBUG: NegativePrint = %d\n", cups.header.NegativePrint);
    fprintf(stderr, "DEBUG: NumCopies = %d\n", cups.header.NumCopies);
    fprintf(stderr, "DEBUG: Orientation = %d\n", cups.header.Orientation);
    fprintf(stderr, "DEBUG: OutputFaceUp = %d\n", cups.header.OutputFaceUp);
    fprintf(stderr, "DEBUG: PageSize = [ %d %d ]\n", cups.header.PageSize[0],
            cups.header.PageSize[1]);
    fprintf(stderr, "DEBUG: Separations = %d\n", cups.header.Separations);
    fprintf(stderr, "DEBUG: TraySwitch = %d\n", cups.header.TraySwitch);
    fprintf(stderr, "DEBUG: Tumble = %d\n", cups.header.Tumble);
    fprintf(stderr, "DEBUG: cupsWidth = %d\n", cups.header.cupsWidth);
    fprintf(stderr, "DEBUG: cupsHeight = %d\n", cups.header.cupsHeight);
    fprintf(stderr, "DEBUG: cupsMediaType = %d\n", cups.header.cupsMediaType);
    fprintf(stderr, "DEBUG: cupsBitsPerColor = %d\n", cups.header.cupsBitsPerColor);
    fprintf(stderr, "DEBUG: cupsBitsPerPixel = %d\n", cups.header.cupsBitsPerPixel);
    fprintf(stderr, "DEBUG: cupsBytesPerLine = %d\n", cups.header.cupsBytesPerLine);
    fprintf(stderr, "DEBUG: cupsColorOrder = %d\n", cups.header.cupsColorOrder);
    fprintf(stderr, "DEBUG: cupsColorSpace = %d\n", cups.header.cupsColorSpace);
    fprintf(stderr, "DEBUG: cupsCompression = %d\n", cups.header.cupsCompression);
    fprintf(stderr, "DEBUG: cupsRowCount = %d\n", cups.header.cupsRowCount);
    fprintf(stderr, "DEBUG: cupsRowFeed = %d\n", cups.header.cupsRowFeed);
    fprintf(stderr, "DEBUG: cupsRowStep = %d\n", cups.header.cupsRowStep);

   /*
    * Setup printer driver variables...
    */

    v = stp_allocate_copy(printer->printvars);

    stp_set_app_gamma(v, 1.0);
    stp_set_brightness(v, 1.0);
    stp_set_contrast(v, 1.0);
    stp_set_cyan(v, 1.0);
    stp_set_magenta(v, 1.0);
    stp_set_yellow(v, 1.0);
    stp_set_saturation(v, 1.0);
    stp_set_density(v, 1.0);
    stp_set_scaling(v, 0); /* No scaling */
    stp_set_cmap(v, NULL);
    stp_set_page_width(v, cups.header.PageSize[0]);
    stp_set_page_height(v, cups.header.PageSize[1]);
    stp_set_orientation(v, ORIENT_PORTRAIT);
    stp_set_gamma(v, 1.0);
    stp_set_image_type(v, cups.header.cupsRowCount);
    stp_set_outfunc(v, cups_writefunc);
    stp_set_errfunc(v, cups_writefunc);
    stp_set_outdata(v, stdout);
    stp_set_errdata(v, stderr);

    if (cups.header.cupsColorSpace == CUPS_CSPACE_W)
      stp_set_output_type(v, OUTPUT_GRAY);
    else
      stp_set_output_type(v, OUTPUT_COLOR);

    stp_set_dither_algorithm(v, cups.header.OutputType);
    stp_set_media_source(v, cups.header.MediaClass);
    stp_set_media_type(v, cups.header.MediaType);

    fprintf(stderr, "DEBUG: PageSize = %dx%d\n", cups.header.PageSize[0],
            cups.header.PageSize[1]);

    if ((size = stp_get_papersize_by_size(cups.header.PageSize[1],
					  cups.header.PageSize[0])) != NULL)
      stp_set_media_size(v, size->name);
    else
      fprintf(stderr, "ERROR: Unable to get media size!\n");

    opts = (*(printer->printfuncs->parameters))(printer, NULL, "Resolution",
						&num_opts);
    if (cups.header.cupsCompression < 0 ||
	cups.header.cupsCompression >= num_opts)
      fprintf(stderr, "ERROR: Unable to set printer resolution!\n");
    else
      stp_set_resolution(v, opts[cups.header.cupsCompression]);

   /*
    * Print the page...
    */

    stp_merge_printvars(v, printer->printvars);
    fprintf(stderr, "DEBUG: stp_get_output_to(v) |%s|\n", stp_get_output_to(v));
    fprintf(stderr, "DEBUG: stp_get_driver(v) |%s|\n", stp_get_driver(v));
    fprintf(stderr, "DEBUG: stp_get_ppd_file(v) |%s|\n", stp_get_ppd_file(v));
    fprintf(stderr, "DEBUG: stp_get_resolution(v) |%s|\n", stp_get_resolution(v));
    fprintf(stderr, "DEBUG: stp_get_media_size(v) |%s|\n", stp_get_media_size(v));
    fprintf(stderr, "DEBUG: stp_get_media_type(v) |%s|\n", stp_get_media_type(v));
    fprintf(stderr, "DEBUG: stp_get_media_source(v) |%s|\n", stp_get_media_source(v));
    fprintf(stderr, "DEBUG: stp_get_ink_type(v) |%s|\n", stp_get_ink_type(v));
    fprintf(stderr, "DEBUG: stp_get_dither_algorithm(v) |%s|\n", stp_get_dither_algorithm(v));
    if (stp_verify_printer_params(printer, v))
      (*printer->printfuncs->print)(printer, &theImage, v);
    else
      fputs("ERROR: Invalid printer settings!\n", stderr);

   /*
    * Purge any remaining bitmap data...
    */

    if (cups.row < cups.header.cupsHeight)
    {
      if ((buffer = xmalloc(cups.header.cupsBytesPerLine)) == NULL)
        break;

      while (cups.row < cups.header.cupsHeight)
      {
        cupsRasterReadPixels(cups.ras, (unsigned char *)buffer,
	                     cups.header.cupsBytesPerLine);
	cups.row ++;
      }
    }
    stp_free_vars(v);
  }

 /*
  * Close the raster stream...
  */

  cupsRasterClose(cups.ras);
  if (fd != 0)
    close(fd);

 /*
  * If no pages were printed, send an error message...
  */

  if (cups.page == 0)
    fputs("ERROR: No pages found!\n", stderr);
  else
    fputs("INFO: Ready to print.\n", stderr);

  return (cups.page == 0);
}


/*
 * 'Image_bpp()' - Return the bytes-per-pixel of an image.
 */

static int				/* O - Bytes per pixel */
Image_bpp(stp_image_t *image)		/* I - Image */
{
  cups_image_t	*cups;		/* CUPS image */


  if ((cups = (cups_image_t *)(image->rep)) == NULL)
    return (0);

 /*
  * For now, we only support RGB and grayscale input from the
  * raster filters.
  */

  if (cups->header.cupsColorSpace == CUPS_CSPACE_RGB)
    return (3);
  else
    return (1);
}


/*
 * 'Image_get_appname()' - Get the application we are running.
 */

static const char *				/* O - Application name */
Image_get_appname(stp_image_t *image)		/* I - Image */
{
  (void)image;

  return ("CUPS 1.1.x driver based on GIMP-print");
}


/*
 * 'Image_get_row()' - Get one row of the image.
 */

void
Image_get_row(stp_image_t   *image,	/* I - Image */
	      unsigned char *data,	/* O - Row */
	      int           row)	/* I - Row number (unused) */
{
  cups_image_t	*cups;			/* CUPS image */


  if ((cups = (cups_image_t *)(image->rep)) == NULL)
    return;

  if (cups->row < cups->header.cupsHeight)
  {
    cupsRasterReadPixels(cups->ras, data, cups->header.cupsBytesPerLine);
    cups->row ++;
  }
  else
    memset(data, 255, cups->header.cupsBytesPerLine);
}


/*
 * 'Image_height()' - Return the height of an image.
 */

static int				/* O - Height in pixels */
Image_height(stp_image_t *image)	/* I - Image */
{
  cups_image_t	*cups;		/* CUPS image */


  if ((cups = (cups_image_t *)(image->rep)) == NULL)
    return (0);

  return (cups->header.cupsHeight);
}


/*
 * 'Image_init()' - Initialize an image.
 */

static void
Image_init(stp_image_t *image)		/* I - Image */
{
  (void)image;
}


/*
 * 'Image_note_progress()' - Notify the user of our progress.
 */

void
Image_note_progress(stp_image_t *image,	/* I - Image */
		    double current,	/* I - Current progress */
		    double total)	/* I - Maximum progress */
{
  cups_image_t	*cups;		/* CUPS image */


  if ((cups = (cups_image_t *)(image->rep)) == NULL)
    return;

  fprintf(stderr, "INFO: Printing page %d, %.0f%%\n",
          cups->page, 100.0 * current / total);
}


/*
 * 'Image_progress_conclude()' - Close the progress display.
 */

static void
Image_progress_conclude(stp_image_t *image)	/* I - Image */
{
  cups_image_t	*cups;		/* CUPS image */


  if ((cups = (cups_image_t *)(image->rep)) == NULL)
    return;

  fprintf(stderr, "INFO: Finished page %d...\n", cups->page);
}


/*
 * 'Image_progress_init()' - Initialize progress display.
 */

static void
Image_progress_init(stp_image_t *image)/* I - Image */
{
  cups_image_t	*cups;		/* CUPS image */


  if ((cups = (cups_image_t *)(image->rep)) == NULL)
    return;

  fprintf(stderr, "INFO: Starting page %d...\n", cups->page);
}


/*
 * 'Image_rotate_180()' - Rotate the image 180 degrees (unsupported).
 */

static void
Image_rotate_180(stp_image_t *image)	/* I - Image */
{
  (void)image;
}


/*
 * 'Image_rotate_ccw()' - Rotate the image counter-clockwise (unsupported).
 */

static void
Image_rotate_ccw(stp_image_t *image)	/* I - Image */
{
  (void)image;
}


/*
 * 'Image_rotate_cw()' - Rotate the image clockwise (unsupported).
 */

static void
Image_rotate_cw(stp_image_t *image)	/* I - Image */
{
  (void)image;
}


/*
 * 'Image_width()' - Return the width of an image.
 */

static int				/* O - Width in pixels */
Image_width(stp_image_t *image)	/* I - Image */
{
  cups_image_t	*cups;		/* CUPS image */


  if ((cups = (cups_image_t *)(image->rep)) == NULL)
    return (0);

  return (cups->header.cupsWidth);
}

/*
 * End of "$Id: rastertoprinter.c,v 1.9.2.1 2001/02/20 04:41:32 rlk Exp $".
 */
