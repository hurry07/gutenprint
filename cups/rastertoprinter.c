/*
 * "$Id: rastertoprinter.c,v 1.5 2000/09/10 15:05:29 easysw Exp $"
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
#include "cups-print.h"


/*
 * Structure for page raster data...
 */

typedef struct
{
  cups_raster_t		*ras;		/* Raster stream to read from */
  int			page;		/* Current page number */
  cups_page_header_t	header;		/* Page header from file */
} cups_image_t;


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
  const printer_t	*printer;	/* Printer driver */  
  vars_t		v;		/* Printer driver variables */
  const papersize_t	*size;		/* Paper size */
  static char		*qualities[] =	/* Quality strings for resolution */
			{
			  "",
			  " Softweave",
			  " Microweave",
			  " High Quality",
			  " Highest Quality",
			  " Emulated"
			  " DMT"
			};


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

  if ((printer = get_printer_by_driver(ppd->modelname)) == NULL)
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

    memcpy(&v, &(printer->printvars), sizeof(v));

    v.app_gamma   = 1.0;
    v.scaling     = 0; /* No scaling */
    v.cmap        = NULL;
    v.page_width  = cups.header.PageSize[0];
    v.page_height = cups.header.PageSize[1];
    v.orientation = ORIENT_PORTRAIT;
    v.gamma       = 1.7;

    if (cups.header.cupsColorSpace == CUPS_CSPACE_W)
      v.output_type = OUTPUT_GRAY;
    else
      v.output_type = OUTPUT_COLOR;

    strcpy(v.dither_algorithm, cups.header.OutputType);
    strcpy(v.media_source, cups.header.MediaClass);
    strcpy(v.media_type, cups.header.MediaType);

    fprintf(stderr, "DEBUG: PageSize = %dx%d\n", cups.header.PageSize[0],
            cups.header.PageSize[1]);

    if ((size = get_papersize_by_size(cups.header.PageSize[1],
                                      cups.header.PageSize[0])) != NULL)
      strcpy(v.media_size, size->name);
    else
      fprintf(stderr, "ERROR: Unable to get media size!\n");

   /*
    * The resolution variable needs a big overhaul...
    */

    if (cups.header.HWResolution[0] == cups.header.HWResolution[1])
      sprintf(v.resolution, "%d DPI%s",
	      cups.header.HWResolution[0],
	      qualities[cups.header.cupsCompression]);
    else
      sprintf(v.resolution, "%d x %d DPI%s",
	      cups.header.HWResolution[0],
	      cups.header.HWResolution[1],
	      qualities[cups.header.cupsCompression]);

   /*
    * Print the page...
    */

    (*printer->print)(printer, 1, stdout, &cups, &v);
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

int				/* O - Bytes per pixel */
Image_bpp(Image image)		/* I - Image */
{
  cups_image_t	*cups;		/* CUPS image */


  if ((cups = (cups_image_t *)image) == NULL)
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

const char *				/* O - Application name */
Image_get_appname(Image image)		/* I - Image */
{
  (void)image;

  return ("CUPS 1.1.x driver based on GIMP-print");
}


/*
 * 'Image_get_row()' - Get one row of the image.
 */

void
Image_get_row(Image         image,	/* I - Image */
              unsigned char *data,	/* O - Row */
	      int           row)	/* I - Row number (unused) */
{
  cups_image_t	*cups;			/* CUPS image */


  if ((cups = (cups_image_t *)image) == NULL)
    return;

  cupsRasterReadPixels(cups->ras, data, cups->header.cupsBytesPerLine);
}


/*
 * 'Image_height()' - Return the height of an image.
 */

int				/* O - Height in pixels */
Image_height(Image image)	/* I - Image */
{
  cups_image_t	*cups;		/* CUPS image */


  if ((cups = (cups_image_t *)image) == NULL)
    return (0);

  return (cups->header.cupsHeight);
}


/*
 * 'Image_init()' - Initialize an image.
 */

void
Image_init(Image image)		/* I - Image */
{
  (void)image;
}


/*
 * 'Image_note_progress()' - Notify the user of our progress.
 */

void
Image_note_progress(Image  image,	/* I - Image */
                    double current,	/* I - Current progress */
		    double total)	/* I - Maximum progress */
{
  cups_image_t	*cups;		/* CUPS image */


  if ((cups = (cups_image_t *)image) == NULL)
    return;

  fprintf(stderr, "INFO: Printing page %d, %.0f%%\n",
          cups->page, 100.0 * current / total);
}


/*
 * 'Image_progress_conclude()' - Close the progress display.
 */

void
Image_progress_conclude(Image image)	/* I - Image */
{
  cups_image_t	*cups;		/* CUPS image */


  if ((cups = (cups_image_t *)image) == NULL)
    return;

  fprintf(stderr, "INFO: Finished page %d...\n", cups->page);
}


/*
 * 'Image_progress_init()' - Initialize progress display.
 */

void
Image_progress_init(Image image)/* I - Image */
{
  cups_image_t	*cups;		/* CUPS image */


  if ((cups = (cups_image_t *)image) == NULL)
    return;

  fprintf(stderr, "INFO: Starting page %d...\n", cups->page);
}


/*
 * 'Image_rotate_180()' - Rotate the image 180 degrees (unsupported).
 */

void
Image_rotate_180(Image image)	/* I - Image */
{
  (void)image;
}


/*
 * 'Image_rotate_ccw()' - Rotate the image counter-clockwise (unsupported).
 */

void
Image_rotate_ccw(Image image)	/* I - Image */
{
  (void)image;
}


/*
 * 'Image_rotate_cw()' - Rotate the image clockwise (unsupported).
 */

void
Image_rotate_cw(Image image)	/* I - Image */
{
  (void)image;
}


/*
 * 'Image_width()' - Return the width of an image.
 */

int				/* O - Width in pixels */
Image_width(Image image)	/* I - Image */
{
  cups_image_t	*cups;		/* CUPS image */


  if ((cups = (cups_image_t *)image) == NULL)
    return (0);

  return (cups->header.cupsWidth);
}


/*
 * End of "$Id: rastertoprinter.c,v 1.5 2000/09/10 15:05:29 easysw Exp $".
 */
