/*
 * "$Id: print-pcl.c,v 1.46 2000/04/20 02:49:25 rlk Exp $"
 *
 *   Print plug-in HP PCL driver for the GIMP.
 *
 *   Copyright 1997-2000 Michael Sweet (mike@easysw.com) and
 *	Robert Krawitz (rlk@alum.mit.edu)
 *
 *   This program is free software; you can redistribute it and/or modify it
 *   under the terms of the GNU General Public License as published by the Free
 *   Software Foundation; either version 2 of the License, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 *   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *   for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Contents:
 *
 *   pcl_parameters()     - Return the parameter values for the given
 *                          parameter.
 *   pcl_imageable_area() - Return the imageable area of the page.
 *   pcl_get_model_capabilities()
 *                        - Return the capabilities of the printer.
 *   pcl_convert_media_size()
 *                        - Convert media size name into PCL code.
 *   pcl_print()          - Print an image to an HP printer.
 *   pcl_mode0()          - Send PCL graphics using mode 0 (no) compression.
 *   pcl_mode2()          - Send PCL graphics using mode 2 (TIFF) compression.
 *
 * Revision History:
 *
 * See bottom
 */

#include "print.h"

/*
 * Local functions...
 */
static void	pcl_mode0(FILE *, unsigned char *, int, int);
static void	pcl_mode2(FILE *, unsigned char *, int, int);

/*
 * Generic define for a name/value set
 */

typedef struct
{
    char  *pcl_name;
    int   pcl_code;
} pcl_t;

/*
 * Media size to PCL media size code table
 *
 * Note, you can force the list of papersizes given in the GUI to be only those
 * supported by defining PCL_NO_CUSTOM_PAPERSIZES
 */

/* #define PCL_NO_CUSTOM_PAPERSIZES */

#define PCL_PAPERSIZE_LETTER		2
#define PCL_PAPERSIZE_LEGAL		3
#define PCL_PAPERSIZE_TABLOID		4
#define PCL_PAPERSIZE_A5		25
#define PCL_PAPERSIZE_A4		26
#define PCL_PAPERSIZE_A3		27
#define PCL_PAPERSIZE_JIS_B5		45
#define PCL_PAPERSIZE_JIS_B4		46
#define PCL_PAPERSIZE_4x6		74
#define PCL_PAPERSIZE_5x8		75
#define PCL_PAPERSIZE_CUSTOM		101	/* Custom size */

#define MAX_PRINTER_PAPER_SIZES	12	/* Max number of paper types supported
					   by any printer */

/*
 * This data comes from the hpdj ghostscript driver by Martin Lottermoser,
 * which in turn comes from the HP documentation.
 *
 * Not all these are supported in gimp print and not all printers
 * support all sizes! The ones not supported by print are commented out.
 */

const static pcl_t pcl_media_sizes[] =
{
    {"Letter", PCL_PAPERSIZE_LETTER},			/* US Letter (8.5 x 11 in) */
    {"Legal", PCL_PAPERSIZE_LEGAL},			/* US Legal (8.5 x 14 in) */
    {"Tabloid", PCL_PAPERSIZE_TABLOID},			/* US Tabloid (11 x 17 in) */
    {"A5", PCL_PAPERSIZE_A5},				/* ISO/JIS A5 (148 x 210 mm) */
    {"A4", PCL_PAPERSIZE_A4},				/* ISO/JIS A4 (210 x 297 mm) */
    {"A3", PCL_PAPERSIZE_A3},				/* ISO/JIS A3 (297 x 420 mm) */
    {"B5", PCL_PAPERSIZE_JIS_B5},			/* JIS B5 (182 x 257 mm). */
    {"B4", PCL_PAPERSIZE_JIS_B4},			/* JIS B4 (257 x 364 mm). */
    {"4x6", PCL_PAPERSIZE_4x6},				/* US Index card (4 x 6 in) */
    {"5x8", PCL_PAPERSIZE_5x8},				/* US Index card (5 x 8 in) */
};

/*
 * Media type to code table
 */

#define PCL_PAPERTYPE_PLAIN	0
#define PCL_PAPERTYPE_BOND	1
#define PCL_PAPERTYPE_PREMIUM	2
#define PCL_PAPERTYPE_GLOSSY	3	/* or photo */
#define PCL_PAPERTYPE_TRANS	4
#define PCL_PAPERTYPE_QPHOTO	5	/* Quick dry photo (2000 only) */
#define PCL_PAPERTYPE_QTRANS	6	/* Quick dry transparency (2000 only) */
#define MAX_PRINTER_PAPER_TYPES	8	/* Max number of paper types supported */

const static pcl_t pcl_media_types[] =
{
    {"Plain", PCL_PAPERTYPE_PLAIN},
    {"Bond", PCL_PAPERTYPE_BOND},
    {"Premium", PCL_PAPERTYPE_PREMIUM},
    {"Glossy/Photo", PCL_PAPERTYPE_GLOSSY},
    {"Transparency", PCL_PAPERTYPE_TRANS},
    {"Quick-dry Photo", PCL_PAPERTYPE_QPHOTO},
    {"Quick-dry Transparency", PCL_PAPERTYPE_QTRANS},
};

/*
 * Media feed to code table. There are different names for the same code,
 * so we encode them by adding "lumps" of "PAPERSOURCE_MOD".
 * This is removed later to get back to the main codes.
 */

#define PAPERSOURCE_MOD			16

#define PCL_PAPERSOURCE_MANUAL		2
#define PCL_PAPERSOURCE_ENVELOPE	3	/* Not used */

/* LaserJet types */
#define PCL_PAPERSOURCE_LJ_TRAY2	1
#define PCL_PAPERSOURCE_LJ_TRAY3	4
#define PCL_PAPERSOURCE_LJ_TRAY4	5
#define PCL_PAPERSOURCE_LJ_TRAY1	8

/* Deskjet 340 types */
#define PCL_PAPERSOURCE_340_PCSF	1 + PAPERSOURCE_MOD
						/* Portable sheet feeder for 340 */
#define PCL_PAPERSOURCE_340_DCSF	4 + PAPERSOURCE_MOD
						/* Desktop sheet feeder for 340 */

/* Other Deskjet types */
#define PCL_PAPERSOURCE_DJ_TRAY		1 + PAPERSOURCE_MOD + PAPERSOURCE_MOD
#define PCL_PAPERSOURCE_DJ_TRAY2	4 + PAPERSOURCE_MOD + PAPERSOURCE_MOD
						/* Tray 2 for 2500 */
#define PCL_PAPERSOURCE_DJ_OPTIONAL	5 + PAPERSOURCE_MOD + PAPERSOURCE_MOD
						/* Optional source for 2500 */
#define PCL_PAPERSOURCE_DJ_AUTO		7 + PAPERSOURCE_MOD + PAPERSOURCE_MOD
						/* Autoselect for 2500 */
#define MAX_PRINTER_PAPER_SOURCES	6	/* Max number of paper sources supported */

const static pcl_t pcl_media_sources[] =
{
    {"Manual", PCL_PAPERSOURCE_MANUAL},
/*  {"Envelope", PCL_PAPERSOURCE_ENVELOPE}, */
    {"Tray 1", PCL_PAPERSOURCE_LJ_TRAY1},
    {"Tray 2", PCL_PAPERSOURCE_LJ_TRAY2},
    {"Tray 3", PCL_PAPERSOURCE_LJ_TRAY3},
    {"Tray 4", PCL_PAPERSOURCE_LJ_TRAY4},
    {"Portable Sheet Feeder", PCL_PAPERSOURCE_340_PCSF},
    {"Desktop Sheet Feeder", PCL_PAPERSOURCE_340_DCSF},
    {"Tray", PCL_PAPERSOURCE_DJ_TRAY},
    {"Tray 2", PCL_PAPERSOURCE_DJ_TRAY2},
    {"Optional Source", PCL_PAPERSOURCE_DJ_OPTIONAL},
    {"Autoselect", PCL_PAPERSOURCE_DJ_AUTO},
};

/*
 * Printer capability data
 */

typedef struct {
  int model;
  int max_width;
  int max_height;
  int resolutions;
  int top_margin;
  int bottom_margin;
  int left_margin;
  int right_margin;
  int color_type;		/* 2 print head or one, 2 level or 4 */
  int printer_type;		/* Deskjet/Laserjet and quirks */
  int paper_sizes[MAX_PRINTER_PAPER_SIZES];
				/* Paper sizes */
  int paper_types[MAX_PRINTER_PAPER_TYPES];
				/* Paper types */
  int paper_sources[MAX_PRINTER_PAPER_SOURCES];
				/* Paper sources */
  } pcl_cap_t;

#define PCL_RES_150_150		1
#define PCL_RES_300_300		2
#define PCL_RES_600_300		4	/* DJ 600 series */
#define PCL_RES_600_600_MONO	8	/* DJ 600/800/1100/2000 b/w only */
#define PCL_RES_600_600		16	/* DJ 9xx ??*/
#define PCL_RES_1200_1200	32	/* DJ 9xx ??*/
#define PCL_RES_2400_1200	64	/* DJ 9xx */
#define MAX_RESOLUTIONS		7	/* for malloc() */

#define PCL_COLOR_NONE		0
#define PCL_COLOR_CMY		1	/* One print head */
#define PCL_COLOR_CMYK		2	/* Two print heads */
#define PCL_COLOR_CMYK4		4	/* CRet printing */

#define PCL_PRINTER_LJ		1
#define PCL_PRINTER_DJ		2
#define PCL_PRINTER_NEW_ERG	4	/* use "\033*rC" to end raster graphics,
					   instead of "\033*rB" */
#define PCL_PRINTER_TIFF	8	/* Use TIFF compression */
#define PCL_PRINTER_MEDIATYPE	16	/* Use media type & print quality */
#define PCL_PRINTER_CUSTOM_SIZE	32	/* Custom sizes supported */

/*
 * FIXME - the 520 shouldn't be lumped in with the 500 as it supports
 * more paper sizes. The same with the 540C and 500C.
 *
 * FIXME: the 540c is lumped in with the 500c in this driver, but the
 * 540c supports media type and print quality but the 500c doesn't
 *
 * The following models use depletion, raster quality and shingling:-
 * 500, 500c, 510, 520, 550c, 560c.
 * The rest use Media Type and Print Quality.
 *
 * This data comes from the hpdj ghostscript driver by Martin Lottermoser,
 * which in turn comes from the HP documentation.
 */

pcl_cap_t pcl_model_capabilities[] =
{
  /* Default/unknown printer - assume laserjet */
  { 0,
    17 * 72 / 2, 14 * 72,		/* Max paper size */
    PCL_RES_150_150 | PCL_RES_300_300,	/* Resolutions */
    12, 12, 18, 18,			/* Margins */
    PCL_COLOR_NONE,
    PCL_PRINTER_LJ,
    { 
      PCL_PAPERSIZE_LETTER,
      PCL_PAPERSIZE_LEGAL,
      PCL_PAPERSIZE_A4,
      0,
    },
    { -1,			/* No selectable paper types */
    },
    { -1,			/* No selectable paper sources */
    },
  },
  /* Deskjet 340 */
  { 340,
    17 * 72 / 2, 14 * 72,
    PCL_RES_150_150 | PCL_RES_300_300,
    7, 41, 18, 18,
    PCL_COLOR_CMY,
    PCL_PRINTER_DJ,
    { 
      PCL_PAPERSIZE_LETTER,
      PCL_PAPERSIZE_LEGAL,
      PCL_PAPERSIZE_A4,
      -1,
    },
    { 
      PCL_PAPERTYPE_PLAIN,
      PCL_PAPERTYPE_BOND,
      PCL_PAPERTYPE_PREMIUM,
      PCL_PAPERTYPE_GLOSSY,
      PCL_PAPERTYPE_TRANS,
      -1,
    },
    { 
      PCL_PAPERSOURCE_MANUAL,
      PCL_PAPERSOURCE_340_PCSF,
      PCL_PAPERSOURCE_340_DCSF,
      -1,
    },
  },
  /* Deskjet 400 */
  { 400,
    17 * 72 / 2, 14 * 72,
    PCL_RES_150_150 | PCL_RES_300_300,
    7, 41, 18, 18,
    PCL_COLOR_CMY,
    PCL_PRINTER_DJ,
    { 
      PCL_PAPERSIZE_LETTER,
      PCL_PAPERSIZE_LEGAL,
      PCL_PAPERSIZE_A4,
      PCL_PAPERSIZE_JIS_B5,
      -1,
    },
    { 
      PCL_PAPERTYPE_PLAIN,
      PCL_PAPERTYPE_BOND,
      PCL_PAPERTYPE_PREMIUM,
      PCL_PAPERTYPE_GLOSSY,
      PCL_PAPERTYPE_TRANS,
      -1,
    },
    { -1,			/* No selectable paper sources */
    },
  },
  /* Deskjet 500, 520 */
  { 500,
    17 * 72 / 2, 14 * 72,
    PCL_RES_150_150 | PCL_RES_300_300,
    7, 41, 18, 18,
    PCL_COLOR_NONE,
    PCL_PRINTER_DJ,
    { 
      PCL_PAPERSIZE_LETTER,
      PCL_PAPERSIZE_LEGAL,
      PCL_PAPERSIZE_A4,
      -1,
    },
    { 
      PCL_PAPERTYPE_PLAIN,
      PCL_PAPERTYPE_BOND,
      PCL_PAPERTYPE_PREMIUM,
      PCL_PAPERTYPE_GLOSSY,
      PCL_PAPERTYPE_TRANS,
      -1,
    },
    { 
      PCL_PAPERSOURCE_MANUAL,
      PCL_PAPERSOURCE_DJ_TRAY,
      -1,
    },
  },
  /* Deskjet 500C, 540C */
  { 501,
    17 * 72 / 2, 14 * 72,
    PCL_RES_150_150 | PCL_RES_300_300,
    7, 33, 18, 18,
    PCL_COLOR_CMY,
    PCL_PRINTER_DJ | PCL_PRINTER_NEW_ERG | PCL_PRINTER_TIFF,
    { 
      PCL_PAPERSIZE_LETTER,
      PCL_PAPERSIZE_LEGAL,
      PCL_PAPERSIZE_A4,
      -1,
    },
    { 
      PCL_PAPERTYPE_PLAIN,
      PCL_PAPERTYPE_BOND,
      PCL_PAPERTYPE_PREMIUM,
      PCL_PAPERTYPE_GLOSSY,
      PCL_PAPERTYPE_TRANS,
      -1,
    },
    { 
      PCL_PAPERSOURCE_MANUAL,
      PCL_PAPERSOURCE_DJ_TRAY,
      -1,
    },
  },
  /* Deskjet 550C, 560C */
  { 550,
    17 * 72 / 2, 14 * 72,
    PCL_RES_150_150 | PCL_RES_300_300,
    3, 33, 18, 18,
    PCL_COLOR_CMYK,
    PCL_PRINTER_DJ | PCL_PRINTER_NEW_ERG | PCL_PRINTER_TIFF,
    { 
      PCL_PAPERSIZE_LETTER,
      PCL_PAPERSIZE_LEGAL,
      PCL_PAPERSIZE_A4,
      -1,
    },
    { 
      PCL_PAPERTYPE_PLAIN,
      PCL_PAPERTYPE_BOND,
      PCL_PAPERTYPE_PREMIUM,
      PCL_PAPERTYPE_GLOSSY,
      PCL_PAPERTYPE_TRANS,
      -1,
    },
    { 
      PCL_PAPERSOURCE_MANUAL,
      PCL_PAPERSOURCE_DJ_TRAY,
      -1,
    },
  },
  /* Deskjet 600/600C */
  { 600,
    17 * 72 / 2, 14 * 72,
    PCL_RES_150_150 | PCL_RES_300_300 | PCL_RES_600_300 | PCL_RES_600_600_MONO,
    0, 33, 18, 18,
    PCL_COLOR_CMY,
    PCL_PRINTER_DJ | PCL_PRINTER_NEW_ERG | PCL_PRINTER_TIFF | PCL_PRINTER_MEDIATYPE |
    PCL_PRINTER_CUSTOM_SIZE,
    { 
      PCL_PAPERSIZE_LETTER,
      PCL_PAPERSIZE_LEGAL,
      PCL_PAPERSIZE_A5,
      PCL_PAPERSIZE_A4,
      PCL_PAPERSIZE_4x6,
      PCL_PAPERSIZE_5x8,
      -1,
    },
    { 
      PCL_PAPERTYPE_PLAIN,
      PCL_PAPERTYPE_BOND,
      PCL_PAPERTYPE_PREMIUM,
      PCL_PAPERTYPE_GLOSSY,
      PCL_PAPERTYPE_TRANS,
      -1,
    },
    { -1,			/* No selectable paper sources */
    },
  },
  /* Deskjet 6xx series */
  { 601,
    17 * 72 / 2, 14 * 72,
    PCL_RES_150_150 | PCL_RES_300_300 | PCL_RES_600_300 | PCL_RES_600_600_MONO,
    0, 33, 18, 18,
    PCL_COLOR_CMYK,
    PCL_PRINTER_DJ | PCL_PRINTER_NEW_ERG | PCL_PRINTER_TIFF | PCL_PRINTER_MEDIATYPE |
    PCL_PRINTER_CUSTOM_SIZE,
    { 
      PCL_PAPERSIZE_LETTER,
      PCL_PAPERSIZE_LEGAL,
      PCL_PAPERSIZE_A5,
      PCL_PAPERSIZE_A4,
      PCL_PAPERSIZE_JIS_B5,
      PCL_PAPERSIZE_4x6,
      PCL_PAPERSIZE_5x8,
      -1,
    },
    { 
      PCL_PAPERTYPE_PLAIN,
      PCL_PAPERTYPE_BOND,
      PCL_PAPERTYPE_PREMIUM,
      PCL_PAPERTYPE_GLOSSY,
      PCL_PAPERTYPE_TRANS,
      -1,
    },
    { 
      -1,
    },
  },
  /* Deskjet 800 series */
  { 800,
    17 * 72 / 2, 14 * 72,
    PCL_RES_150_150 | PCL_RES_300_300 | PCL_RES_600_600_MONO | PCL_RES_600_600_MONO,
    3, 33, 18, 18,
    PCL_COLOR_CMYK | PCL_COLOR_CMYK4,
    PCL_PRINTER_DJ | PCL_PRINTER_NEW_ERG | PCL_PRINTER_TIFF | PCL_PRINTER_MEDIATYPE |
    PCL_PRINTER_CUSTOM_SIZE,
    { 
      PCL_PAPERSIZE_LETTER,
      PCL_PAPERSIZE_LEGAL,
      PCL_PAPERSIZE_A5,
      PCL_PAPERSIZE_A4,
      PCL_PAPERSIZE_JIS_B5,
      PCL_PAPERSIZE_4x6,
      PCL_PAPERSIZE_5x8,
      -1,
    },
    { 
      PCL_PAPERTYPE_PLAIN,
      PCL_PAPERTYPE_BOND,
      PCL_PAPERTYPE_PREMIUM,
      PCL_PAPERTYPE_GLOSSY,
      PCL_PAPERTYPE_TRANS,
      -1,
    },
    { 
      -1,
    },
  },
  /* Deskjet 900 series */
  { 900,
    17 * 72 / 2, 14 * 72,
    PCL_RES_150_150 | PCL_RES_300_300 | PCL_RES_600_600 | PCL_RES_1200_1200 | PCL_RES_2400_1200,
    3, 33, 18, 18,
    PCL_COLOR_CMYK | PCL_COLOR_CMYK4,
    PCL_PRINTER_DJ | PCL_PRINTER_NEW_ERG | PCL_PRINTER_TIFF | PCL_PRINTER_MEDIATYPE |
    PCL_PRINTER_CUSTOM_SIZE,
    { 
      PCL_PAPERSIZE_LETTER,
      PCL_PAPERSIZE_LEGAL,
      PCL_PAPERSIZE_A5,
      PCL_PAPERSIZE_A4,
      PCL_PAPERSIZE_JIS_B5,
      PCL_PAPERSIZE_4x6,
      PCL_PAPERSIZE_5x8,
      -1,
    },
    { 
      PCL_PAPERTYPE_PLAIN,
      PCL_PAPERTYPE_BOND,
      PCL_PAPERTYPE_PREMIUM,
      PCL_PAPERTYPE_GLOSSY,
      PCL_PAPERTYPE_TRANS,
      -1,
    },
    { -1,			/* No selectable paper sources */
    },
  },
  /* Deskjet 1100C, 1120C, 1220C */
  { 1100,
    12 * 72, 18 * 72,
    PCL_RES_150_150 | PCL_RES_300_300 | PCL_RES_600_600_MONO,
    3, 33, 18, 18,
    PCL_COLOR_CMYK | PCL_COLOR_CMYK4,
    PCL_PRINTER_DJ | PCL_PRINTER_NEW_ERG | PCL_PRINTER_TIFF | PCL_PRINTER_MEDIATYPE |
    PCL_PRINTER_CUSTOM_SIZE,
    { 
      PCL_PAPERSIZE_LETTER,
      PCL_PAPERSIZE_LEGAL,
      PCL_PAPERSIZE_TABLOID,
      PCL_PAPERSIZE_A5,
      PCL_PAPERSIZE_A4,
      PCL_PAPERSIZE_A3,
      PCL_PAPERSIZE_JIS_B5,
      PCL_PAPERSIZE_JIS_B4,
      PCL_PAPERSIZE_4x6,
      PCL_PAPERSIZE_5x8,
      -1,
    },
    { 
      PCL_PAPERTYPE_PLAIN,
      PCL_PAPERTYPE_BOND,
      PCL_PAPERTYPE_PREMIUM,
      PCL_PAPERTYPE_GLOSSY,
      PCL_PAPERTYPE_TRANS,
      -1,
    },
    { 
      PCL_PAPERSOURCE_MANUAL,
      PCL_PAPERSOURCE_DJ_TRAY,
      -1,
    },
  },
  /* Deskjet 1200C, 1600C */
  { 1200,
    17 * 72 / 2, 14 * 72,
    PCL_RES_150_150 | PCL_RES_300_300,
    12, 12, 18, 18,
    PCL_COLOR_CMY,
    PCL_PRINTER_DJ | PCL_PRINTER_NEW_ERG | PCL_PRINTER_TIFF | PCL_PRINTER_MEDIATYPE |
    PCL_PRINTER_CUSTOM_SIZE,
    { 
      PCL_PAPERSIZE_LETTER,
      PCL_PAPERSIZE_LEGAL,
      PCL_PAPERSIZE_A5,
      PCL_PAPERSIZE_A4,
      PCL_PAPERSIZE_JIS_B5,
      PCL_PAPERSIZE_4x6,
      PCL_PAPERSIZE_5x8,
      -1,
    },
    { 
      PCL_PAPERTYPE_PLAIN,
      PCL_PAPERTYPE_BOND,
      PCL_PAPERTYPE_PREMIUM,
      PCL_PAPERTYPE_GLOSSY,
      PCL_PAPERTYPE_TRANS,
      -1,
    },
    { 
      PCL_PAPERSOURCE_MANUAL,
      PCL_PAPERSOURCE_DJ_TRAY,
      -1,
    },
  },
  /* Deskjet 2000 */
  { 2000,
    17 * 72 / 2, 14 * 72,
    PCL_RES_150_150 | PCL_RES_300_300 | PCL_RES_600_600_MONO,
    12, 12, 18, 18,
    PCL_COLOR_CMYK,
    PCL_PRINTER_DJ | PCL_PRINTER_NEW_ERG | PCL_PRINTER_TIFF | PCL_PRINTER_MEDIATYPE |
    PCL_PRINTER_CUSTOM_SIZE,
    { 
      PCL_PAPERSIZE_LETTER,
      PCL_PAPERSIZE_LEGAL,
      PCL_PAPERSIZE_A5,
      PCL_PAPERSIZE_A4,
      PCL_PAPERSIZE_4x6,
      PCL_PAPERSIZE_5x8,
      -1,
    },
    { 
      PCL_PAPERTYPE_PLAIN,
      PCL_PAPERTYPE_BOND,
      PCL_PAPERTYPE_PREMIUM,
      PCL_PAPERTYPE_GLOSSY,
      PCL_PAPERTYPE_TRANS,
      PCL_PAPERTYPE_QPHOTO,
      PCL_PAPERTYPE_QTRANS,
      -1,
    },
    { 
      PCL_PAPERSOURCE_MANUAL,
      PCL_PAPERSOURCE_DJ_TRAY,
      -1,
    },
  },
  /* Deskjet 2500 */
  { 2500,
    12 * 72, 18 * 72,
    PCL_RES_150_150 | PCL_RES_300_300 | PCL_RES_600_600_MONO,
    12, 12, 18, 18,
    PCL_COLOR_CMYK,
    PCL_PRINTER_DJ | PCL_PRINTER_NEW_ERG | PCL_PRINTER_TIFF | PCL_PRINTER_MEDIATYPE |
    PCL_PRINTER_CUSTOM_SIZE,
    { 
      PCL_PAPERSIZE_LETTER,
      PCL_PAPERSIZE_LEGAL,
      PCL_PAPERSIZE_A5,
      PCL_PAPERSIZE_A4,
      PCL_PAPERSIZE_A3,
      PCL_PAPERSIZE_JIS_B5,
      PCL_PAPERSIZE_JIS_B4,
      PCL_PAPERSIZE_4x6,
      PCL_PAPERSIZE_5x8,
      -1,
    },
    { 
      PCL_PAPERTYPE_PLAIN,
      PCL_PAPERTYPE_BOND,
      PCL_PAPERTYPE_PREMIUM,
      PCL_PAPERTYPE_GLOSSY,
      PCL_PAPERTYPE_TRANS,
      PCL_PAPERTYPE_QPHOTO,
      PCL_PAPERTYPE_QTRANS,
      -1,
    },
    { 
      PCL_PAPERSOURCE_MANUAL,
      PCL_PAPERSOURCE_DJ_AUTO,
      PCL_PAPERSOURCE_DJ_TRAY,
      PCL_PAPERSOURCE_DJ_TRAY2,
      PCL_PAPERSOURCE_DJ_OPTIONAL,
      -1,
    },
  },
  /* LaserJet II series */
  { 2,
    17 * 72 / 2, 14 * 72,
    PCL_RES_150_150 | PCL_RES_300_300,
    12, 12, 18, 18,
    PCL_COLOR_NONE,
    PCL_PRINTER_LJ,
    { 
      PCL_PAPERSIZE_LETTER,
      PCL_PAPERSIZE_LEGAL,
      PCL_PAPERSIZE_A4,
      -1,
    },
    { -1,			/* No selectable paper types */
    },
    { 
      PCL_PAPERSOURCE_MANUAL,
      PCL_PAPERSOURCE_LJ_TRAY1,
      PCL_PAPERSOURCE_LJ_TRAY2,
      PCL_PAPERSOURCE_LJ_TRAY3,
      PCL_PAPERSOURCE_LJ_TRAY4,
      -1,
    },
  },
  /* LaserJet III series */
  { 3,
    17 * 72 / 2, 14 * 72,
    PCL_RES_150_150 | PCL_RES_300_300,
    12, 12, 18, 18,
    PCL_COLOR_NONE,
    PCL_PRINTER_LJ | PCL_PRINTER_TIFF,
    { 
      PCL_PAPERSIZE_LETTER,
      PCL_PAPERSIZE_LEGAL,
      PCL_PAPERSIZE_A4,
      -1,
    },
    { -1,			/* No selectable paper types */
    },
    { 
      PCL_PAPERSOURCE_MANUAL,
      PCL_PAPERSOURCE_LJ_TRAY1,
      PCL_PAPERSOURCE_LJ_TRAY2,
      PCL_PAPERSOURCE_LJ_TRAY3,
      PCL_PAPERSOURCE_LJ_TRAY4,
      -1,
    },
  },
  /* LaserJet 4 series, 5 series, 6 series */
  { 4,
    17 * 72 / 2, 14 * 72,
    PCL_RES_150_150 | PCL_RES_300_300,
    12, 12, 18, 18,
    PCL_COLOR_NONE,
    PCL_PRINTER_LJ | PCL_PRINTER_NEW_ERG | PCL_PRINTER_TIFF,
    { 
      PCL_PAPERSIZE_LETTER,
      PCL_PAPERSIZE_LEGAL,
      PCL_PAPERSIZE_A4,
      -1,
    },
    { -1,			/* No selectable paper types */
    },
    { 
      PCL_PAPERSOURCE_MANUAL,
      PCL_PAPERSOURCE_LJ_TRAY1,
      PCL_PAPERSOURCE_LJ_TRAY2,
      PCL_PAPERSOURCE_LJ_TRAY3,
      PCL_PAPERSOURCE_LJ_TRAY4,
      -1,
    },
  },
  /* LaserJet 4V, 4Si, 5Si */
  { 5,
    12 * 72, 18 * 72,
    PCL_RES_150_150 | PCL_RES_300_300,
    12, 12, 18, 18,
    PCL_COLOR_NONE,
    PCL_PRINTER_LJ | PCL_PRINTER_NEW_ERG | PCL_PRINTER_TIFF,
    { 
      PCL_PAPERSIZE_LETTER,
      PCL_PAPERSIZE_LEGAL,
      PCL_PAPERSIZE_TABLOID,
      PCL_PAPERSIZE_A5,
      PCL_PAPERSIZE_A4,
      PCL_PAPERSIZE_A3,
      PCL_PAPERSIZE_JIS_B5,
      PCL_PAPERSIZE_JIS_B4,		/* Guess */
      PCL_PAPERSIZE_4x6,
      PCL_PAPERSIZE_5x8,
      -1,
    },
    { -1,			/* No selectable paper types */
    },
    { 
      PCL_PAPERSOURCE_MANUAL,
      PCL_PAPERSOURCE_LJ_TRAY1,
      PCL_PAPERSOURCE_LJ_TRAY2,
      PCL_PAPERSOURCE_LJ_TRAY3,
      PCL_PAPERSOURCE_LJ_TRAY4,
      -1,
    },
  },
};

/*
 * Convert a name into it's option value
 */

int pcl_string_to_val(const char *string,		/* I: String */
                           const pcl_t *options,	/* I: Options */
			   int num_options)		/* I: Num options */
{

  int i;
  int code = -1;

 /*
  * Look up the string in the table and convert to the code.
  */

  for (i=0; i<num_options; i++) {
    if (!strcmp(string, options[i].pcl_name)) {
       code=options[i].pcl_code;
       break;
       }
  }

#ifdef DEBUG
  fprintf(stderr, "String: %s, Code: %d\n", string, code);
#endif

  return(code);
}

/*
 * Convert a value into it's option name
 */

char * pcl_val_to_string(int code,			/* I: Code */
                           const pcl_t *options,	/* I: Options */
			   int num_options)		/* I: Num options */
{

  int i;
  char *string = NULL;

 /*
  * Look up the code in the table and convert to the string.
  */

  for (i=0; i<num_options; i++) {
    if (code == options[i].pcl_code) {
       string=options[i].pcl_name;
       break;
       }
  }

#ifdef DEBUG
  fprintf(stderr, "Code: %d, String: %s\n", string, code);
#endif

  return(string);
}

static double dot_sizes[] = { 0.5, 0.832, 1.0 };

/*
 * pcl_get_model_capabilities() - Return struct of model capabilities
 */

pcl_cap_t				/* O: Capabilities */
pcl_get_model_capabilities(int model)	/* I: Model */
{
  int i;
  int models= sizeof(pcl_model_capabilities) / sizeof(pcl_cap_t);
  for (i=0; i<models; i++) {
    if (pcl_model_capabilities[i].model == model) {
      return pcl_model_capabilities[i];
    }
  }
  fprintf(stderr,"pcl: model %d not found in capabilities list.\n",model);
  return pcl_model_capabilities[0];
}

static char *
c_strdup(const char *s)
{
  char *ret = malloc(strlen(s) + 1);
  strcpy(ret, s);
  return ret;
}

/*
 * Convert Media size name into PCL media code for printer
 */

int pcl_convert_media_size(const char *media_size,	/* I: Media size string */
                           int  model)			/* I: model number */
{

  int i;
  int media_code = 0;
  pcl_cap_t caps;

 /*
  * First look up the media size in the table and convert to the code.
  */

  media_code = pcl_string_to_val(media_size, pcl_media_sizes,
                                 sizeof(pcl_media_sizes) / sizeof(pcl_t));

#ifdef DEBUG
  fprintf(stderr, "Media Size: %s, Code: %d\n", media_size, media_code);
#endif

 /*
  * Now see if the printer supports the code found.
  */

  if (media_code != -1) {
    caps = pcl_get_model_capabilities(model);

    for (i=0; (i<MAX_PRINTER_PAPER_SIZES) && (caps.paper_sizes[i] != -1); i++) {
      if (media_code == caps.paper_sizes[i])
        return(media_code);		/* Is supported */
    }

#ifdef DEBUG
    fprintf(stderr, "Media Code %d not supported by printer model %d.\n",
      media_code, model);
#endif
    return(-1);				/* Not supported */
  }
  else
    return(-1);				/* Not supported */
}

/*
 * 'pcl_parameters()' - Return the parameter values for the given parameter.
 */

char **				/* O - Parameter values */
pcl_parameters(int  model,	/* I - Printer model */
               char *ppd_file,	/* I - PPD file (not used) */
               char *name,	/* I - Name of parameter */
               int  *count)	/* O - Number of values */
{
  int		i;
  char		**valptrs;
  pcl_cap_t caps;

  if (count == NULL)
    return (NULL);

  *count = 0;

  if (name == NULL)
    return (NULL);

  caps = pcl_get_model_capabilities(model);

#ifdef DEBUG
  fprintf(stderr, "Printer model = %d\n", model);
  fprintf(stderr, "PageWidth = %d, PageLength = %d\n", caps.max_width, caps.max_height);
  fprintf(stderr, "Margins: top = %d, bottom = %d, left = %d, right = %d\n",
    caps.top_margin, caps.bottom_margin, caps.left_margin, caps.right_margin);
  fprintf(stderr, "Resolutions: %d\n", caps.resolutions);
  fprintf(stderr, "ColorType = %d, PrinterType = %d\n", caps.color_type, caps.printer_type);
#endif

  if (strcmp(name, "PageSize") == 0)
    {
      const papersize_t *papersizes = get_papersizes();
#ifdef PCL_NO_CUSTOM_PAPERSIZES
      int use_custom = 0;
#else
      int use_custom = ((caps.printer_type & PCL_PRINTER_CUSTOM_SIZE)
                         == PCL_PRINTER_CUSTOM_SIZE);
#endif
      valptrs = malloc(sizeof(char *) * known_papersizes());
      *count = 0;
      for (i = 0; i < known_papersizes(); i++)
	{
	  if (strlen(papersizes[i].name) > 0 &&
	      papersizes[i].width <= caps.max_width &&
	      papersizes[i].length <= caps.max_height &&
              ((use_custom == 1) || ((use_custom == 0) && 
              (pcl_convert_media_size(papersizes[i].name, model) != -1)))
             )
	    {
	      valptrs[*count] = malloc(strlen(papersizes[i].name) + 1);
	      strcpy(valptrs[*count], papersizes[i].name);
	      (*count)++;
	    }
	}
      return (valptrs);
    }
  else if (strcmp(name, "MediaType") == 0)
  {
    if (caps.paper_types[0] == -1)
    {
      *count = 0;
      return (NULL);
    }
    else
    {
      valptrs = malloc(sizeof(char *) * MAX_PRINTER_PAPER_TYPES);
      *count = 0;
      for (i=0; (i < MAX_PRINTER_PAPER_TYPES) && (caps.paper_types[i] != -1); i++) {
        valptrs[i] = c_strdup(pcl_val_to_string(caps.paper_types[i], pcl_media_types,
                                        sizeof(pcl_media_types) / sizeof(pcl_t)));
        (*count)++;
      }
      return(valptrs);
    }
  }
  else if (strcmp(name, "InputSlot") == 0)
  {
    if (caps.paper_sources[0] == -1)
    {
      *count = 0;
      return (NULL);
    }
    else
    {
      valptrs = malloc(sizeof(char *) * MAX_PRINTER_PAPER_SOURCES);
      *count = 0;
      for (i=0; (i < MAX_PRINTER_PAPER_SOURCES) && (caps.paper_sources[i] != -1); i++) {
        valptrs[i] = c_strdup(pcl_val_to_string(caps.paper_sources[i], pcl_media_sources,
                                        sizeof(pcl_media_sources) / sizeof(pcl_t)));
        (*count)++;
      }
      return(valptrs);
    }
  }
  else if (strcmp(name, "Resolution") == 0)
  {
    i = 0;
    valptrs = malloc(sizeof(char *) * MAX_RESOLUTIONS);
    if (caps.resolutions & PCL_RES_150_150)
      valptrs[i++]= c_strdup("150x150 DPI");
    if (caps.resolutions & PCL_RES_300_300)
      valptrs[i++]= c_strdup("300x300 DPI");
    if (caps.resolutions & PCL_RES_600_300)
      valptrs[i++]= c_strdup("600x300 DPI");
    if (caps.resolutions & PCL_RES_600_600_MONO)
      valptrs[i++]= c_strdup("600x600 DPI (mono only)");
    if (caps.resolutions & PCL_RES_600_600)
      valptrs[i++]= c_strdup("600x600 DPI");
    if (caps.resolutions & PCL_RES_1200_1200)
      valptrs[i++]= c_strdup("1200x1200 DPI");
    if (caps.resolutions & PCL_RES_2400_1200)
      valptrs[i++]= c_strdup("2400x1200 DPI");
    *count= i;
    return(valptrs);
  }
  else
    return (NULL);
}


/*
 * 'pcl_imageable_area()' - Return the imageable area of the page.
 */

void
pcl_imageable_area(int  model,		/* I - Printer model */
                   char *ppd_file,	/* I - PPD file (not used) */
                   char *media_size,	/* I - Media size */
                   int  *left,		/* O - Left position in points */
                   int  *right,		/* O - Right position in points */
                   int  *bottom,	/* O - Bottom position in points */
                   int  *top)		/* O - Top position in points */
{
  int	width, length;			/* Size of page */
  pcl_cap_t caps;			/* Printer caps */

  caps = pcl_get_model_capabilities(model);

  default_media_size(model, ppd_file, media_size, &width, &length);

/*
 * Note: The margins actually vary with paper size, but since you can
 * move the image around on the page anyway, it hardly matters.
 */

  *left   = caps.left_margin;
  *right  = width - caps.right_margin;
  *top    = length - caps.top_margin;
  *bottom = caps.bottom_margin;
}


/*
 * 'pcl_print()' - Print an image to an HP printer.
 */

void
pcl_print(const printer_t *printer,		/* I - Model */
          int       copies,		/* I - Number of copies */
          FILE      *prn,		/* I - File to print to */
          Image     image,		/* I - Image to print */
	  unsigned char    *cmap,	/* I - Colormap (for indexed images) */
	  vars_t    *v)
{
  int		model = printer->model;
  char 		*ppd_file = v->ppd_file;
  char 		*resolution = v->resolution;
  char 		*media_size = v->media_size;
  char 		*media_type = v->media_type;
  char 		*media_source = v->media_source;
  int 		output_type = v->output_type;
  int		orientation = v->orientation;
  float 	scaling = v->scaling;
  int		top = v->top;
  int		left = v->left;
  int		x, y;		/* Looping vars */
  int		xdpi, ydpi;	/* Resolution */
  unsigned short *out;
  unsigned char	*in,		/* Input pixels */
		*black,		/* Black bitmap data */
		*cyan,		/* Cyan bitmap data */
		*magenta,	/* Magenta bitmap data */
		*yellow;	/* Yellow bitmap data */
  int		page_left,	/* Left margin of page */
		page_right,	/* Right margin of page */
		page_top,	/* Top of page */
		page_bottom,	/* Bottom of page */
		page_width,	/* Width of page */
		page_height,	/* Height of page */
		out_width,	/* Width of image on page */
		out_height,	/* Height of image on page */
		out_bpp,	/* Output bytes per pixel */
		temp_width,	/* Temporary width of image on page */
		temp_height,	/* Temporary height of image on page */
		landscape,	/* True if we rotate the output 90 degrees */
		length,		/* Length of raster data */
		errdiv,		/* Error dividend */
		errmod,		/* Error modulus */
		errval,		/* Current error value */
		errline,	/* Current raster line */
		errlast;	/* Last raster line loaded */
  convert_t	colorfunc;	/* Color conversion function... */
  void		(*writefunc)(FILE *, unsigned char *, int, int);
				/* PCL output function */
  int           image_height,
                image_width,
                image_bpp;
  void *	dither;
  pcl_cap_t	caps;		/* Printer capabilities */
  int		do_cret,	/* 300 DPI CRet printing */
		planes;		/* # of outut planes */
  int           tempwidth,	/* From default_media_size() */
                templength,	/* From default_media_size() */
                pcl_media_size, /* PCL media size code */
		pcl_media_type, /* PCL media type code */
		pcl_media_source;	/* PCL media source code */

  caps = pcl_get_model_capabilities(model);

 /*
  * Setup a read-only pixel region for the entire image...
  */

  Image_init(image);
  image_height = Image_height(image);
  image_width = Image_width(image);
  image_bpp = Image_bpp(image);

 /*
  * Choose the correct color conversion function...
  */
  if (v->image_type == IMAGE_MONOCHROME)
    {
      output_type = OUTPUT_GRAY;
    }

  if (caps.color_type == PCL_COLOR_NONE)
    output_type = OUTPUT_GRAY;
  else if (image_bpp < 3 && cmap == NULL && output_type == OUTPUT_COLOR)
    output_type = OUTPUT_GRAY_COLOR;		/* Force grayscale output */

  if (output_type == OUTPUT_COLOR)
  {
    out_bpp = 3;

    if (image_bpp >= 3)
      colorfunc = rgb_to_rgb;
    else
      colorfunc = indexed_to_rgb;
  }
  else if (output_type == OUTPUT_GRAY_COLOR)
  {
    out_bpp = 3;
    colorfunc = gray_to_rgb;
  }
  else
  {
    out_bpp = 1;

    if (image_bpp >= 3)
      colorfunc = rgb_to_gray;
    else if (cmap == NULL)
      colorfunc = gray_to_gray;
    else
      colorfunc = indexed_to_gray;
  }

 /*
  * Figure out the output resolution...
  */

  sscanf(resolution,"%dx%d",&xdpi,&ydpi);
  
#ifdef DEBUG
  fprintf(stderr,"pcl: resolution=%dx%d\n",xdpi,ydpi);
#endif
  
  if (((caps.resolutions & PCL_RES_600_600_MONO) == PCL_RES_600_600_MONO) &&
      output_type != OUTPUT_GRAY && xdpi == 600 && ydpi == 600) {
      fprintf(stderr, "600x600 resolution only available in MONO, changed to 300x300\n");
      xdpi = 300;
      ydpi = 300;
  }

  do_cret = (xdpi >= 300 && ((caps.color_type & PCL_COLOR_CMYK4) == PCL_COLOR_CMYK4) &&
	     v->image_type != IMAGE_MONOCHROME);

#ifdef DEBUG
  fprintf(stderr, "do_cret = %d\n", do_cret);
#endif
  
 /*
  * Compute the output size...
  */

  landscape = 0;
  pcl_imageable_area(model, ppd_file, media_size, &page_left, &page_right,
                     &page_bottom, &page_top);

  page_width  = page_right - page_left;
  page_height = page_top - page_bottom;

 /*
  * Portrait width/height...
  */

  if (scaling < 0.0)
  {
   /*
    * Scale to pixels per inch...
    */

    out_width  = image_width * -72.0 / scaling;
    out_height = image_height * -72.0 / scaling;
  }
  else
  {
   /*
    * Scale by percent...
    */

    out_width  = page_width * scaling / 100.0;
    out_height = out_width * image_height / image_width;
    if (out_height > page_height)
    {
      out_height = page_height * scaling / 100.0;
      out_width  = out_height * image_width / image_height;
    }
  }

 /*
  * Landscape width/height...
  */

  if (scaling < 0.0)
  {
   /*
    * Scale to pixels per inch...
    */

    temp_width  = image_height * -72.0 / scaling;
    temp_height = image_width * -72.0 / scaling;
  }
  else
  {
   /*
    * Scale by percent...
    */

    temp_width  = page_width * scaling / 100.0;
    temp_height = temp_width * image_width / image_height;
    if (temp_height > page_height)
    {
      temp_height = page_height;
      temp_width  = temp_height * image_height / image_width;
    }
  }

 /*
  * See which orientation has the greatest area (or if we need to rotate the
  * image to fit it on the page...)
  */

  if (orientation == ORIENT_AUTO)
  {
    if (scaling < 0.0)
    {
      if ((out_width > page_width && out_height < page_width) ||
          (out_height > page_height && out_width < page_height))
	orientation = ORIENT_LANDSCAPE;
      else
	orientation = ORIENT_PORTRAIT;
    }
    else
    {
      if ((temp_width * temp_height) > (out_width * out_height))
	orientation = ORIENT_LANDSCAPE;
      else
	orientation = ORIENT_PORTRAIT;
    }
  }

  if (orientation == ORIENT_LANDSCAPE)
  {
    out_width  = temp_width;
    out_height = temp_height;
    landscape  = 1;

   /*
    * Swap left/top offsets...
    */

    x    = top;
    top  = left;
    left = page_width - x - out_width;
  }

  if (left < 0)
    left = (page_width - out_width) / 2 + page_left;
  else
    left = left + page_left;

  if (top < 0)
    top  = (page_height + out_height) / 2 + page_bottom;
  else
    top = page_height - top + page_bottom;

#ifdef DEBUG
  printf("page_width = %d, page_height = %d\n", page_width, page_height);
  printf("out_width = %d, out_height = %d\n", out_width, out_height);
  printf("xdpi = %d, ydpi = %d, landscape = %d\n", xdpi, ydpi, landscape);
#endif /* DEBUG */

 /*
  * Let the user know what we're doing...
  */

  Image_progress_init(image);

 /*
  * Send PCL initialization commands...
  */

  fputs("\033E", prn); 				/* PCL reset */

 /*
  * Set media size
  *
  * We need to get the length of the page because we need
  * to correct "top" below.
  */

  default_media_size(model, ppd_file, media_size, &tempwidth, &templength);

  pcl_media_size = pcl_convert_media_size(media_size, model);

#ifdef DEBUG
  printf("Templength = %d, pcl_media_size = %d, media_size = %s\n",
    templength, pcl_media_size, media_size);
#endif

 /*
  * If the media size requested is unknown, try it as a custom size.
  *
  * Warning: The margins may need to be fixed for this!
  */

  if (pcl_media_size == -1) {
    fprintf(stderr, "Paper size %s is not directly supported by printer.\n",
      media_size);
    fprintf(stderr, "Trying as custom pagesize (watch the margins!)\n");
    pcl_media_size = PCL_PAPERSIZE_CUSTOM;			/* Custom */
  }

  fprintf(prn, "\033&l%dA", pcl_media_size);
  top = templength - top;

  fputs("\033&l0L", prn);			/* Turn off perforation skip */
  fputs("\033&l0E", prn);			/* Reset top margin to 0 */

 /*
  * Convert media type string to the code, if specified.
  */

  if (strlen(media_source) != 0) {
    pcl_media_source = pcl_string_to_val(media_source, pcl_media_sources,
                         sizeof(pcl_media_sources) / sizeof(pcl_t));

#ifdef DEBUG
    printf("pcl_media_source = %d, media_source = %s\n", pcl_media_source,
           media_source);
#endif

    if (pcl_media_source == -1)
      fprintf(stderr, "Unknown media source %s, ignored.\n", media_source);
    else {

/* Correct the value by taking the modulus */

      pcl_media_source = pcl_media_source % PAPERSOURCE_MOD;
      fprintf(prn, "\033&l%dH", pcl_media_source);
    }
  }

 /*
  * Convert media type string to the code, if specified.
  */

  if (strlen(media_type) != 0) {
    pcl_media_type = pcl_string_to_val(media_type, pcl_media_types,
                       sizeof(pcl_media_types) / sizeof(pcl_t));

#ifdef DEBUG
    printf("pcl_media_type = %d, media_type = %s\n", pcl_media_type,
           media_type);
#endif

    if (pcl_media_type == -1) {
      fprintf(stderr, "Unknown media type %s, set to PLAIN.\n", media_type);
      pcl_media_type = PCL_PAPERTYPE_PLAIN;
    }
  }
  else
    pcl_media_type = PCL_PAPERTYPE_PLAIN;

 /*
  * Set DJ print quality to "best" if resolution >= 300
  */

  if ((xdpi >= 300) && ((caps.printer_type & PCL_PRINTER_DJ) == PCL_PRINTER_DJ))
  {
    if ((caps.printer_type & PCL_PRINTER_MEDIATYPE) == PCL_PRINTER_MEDIATYPE)
    {
      fputs("\033*o1M", prn);			/* Quality = presentation */
      fprintf(prn, "\033&l%dM", pcl_media_type);
    }
    else
    {
      fputs("\033*r2Q", prn);			/* Quality (high) */
      fputs("\033*o2Q", prn);			/* Shingling (4 passes) */

 /* Depletion depends on media type and cart type. */

      if ((pcl_media_type == PCL_PAPERTYPE_PLAIN)
	|| (pcl_media_type == PCL_PAPERTYPE_BOND)) {
      if ((caps.color_type & PCL_COLOR_CMY) == PCL_COLOR_CMY)
          fputs("\033*o2D", prn);			/* Depletion 25% */
        else
          fputs("\033*o5D", prn);			/* Depletion 50% with gamma correction */
      }

      else if ((pcl_media_type == PCL_PAPERTYPE_PREMIUM)
             || (pcl_media_type == PCL_PAPERTYPE_GLOSSY)
             || (pcl_media_type == PCL_PAPERTYPE_TRANS))
        fputs("\033*o1D", prn);			/* Depletion none */
    }
  }

  if ((xdpi != ydpi) || (do_cret == 1))		/* Set resolution using CRD */
  {

   /*
    * Send configure image data command with horizontal and
    * vertical resolutions as well as a color count...
    */

    if (output_type != OUTPUT_GRAY)
      if ((caps.color_type & PCL_COLOR_CMY) == PCL_COLOR_CMY)
        planes = 3;
      else
        planes = 4;
    else
      planes = 1;

    fprintf(prn, "\033*g%dW", 2 + (planes * 6));
    putc(2, prn);				/* Format 2 (Complex Direct Planar) */
    putc(planes, prn);				/* # output planes */

    if (planes != 3) {
      putc(xdpi >> 8, prn);			/* Black resolution */
      putc(xdpi, prn);
      putc(ydpi >> 8, prn);
      putc(ydpi, prn);
      putc(0, prn);
      putc(do_cret ? 4 : 2, prn);
    }

    if (planes != 1) {
      putc(xdpi >> 8, prn);			/* Cyan resolution */
      putc(xdpi, prn);
      putc(ydpi >> 8, prn);
      putc(ydpi, prn);
      putc(0, prn);
      putc(do_cret ? 4 : 2, prn);

      putc(xdpi >> 8, prn);			/* Magenta resolution */
      putc(xdpi, prn);
      putc(ydpi >> 8, prn);
      putc(ydpi, prn);
      putc(0, prn);
      putc(do_cret ? 4 : 2, prn);

      putc(xdpi >> 8, prn);			/* Yellow resolution */
      putc(xdpi, prn);
      putc(ydpi >> 8, prn);
      putc(ydpi, prn);
      putc(0, prn);
      putc(do_cret ? 4 : 2, prn);
    }
  }
  else
  {
    fprintf(prn, "\033*t%dR", xdpi);		/* Simple resolution */
    if (output_type != OUTPUT_GRAY)
    {
      if ((caps.color_type & PCL_COLOR_CMY) == PCL_COLOR_CMY)
        fputs("\033*r-3U", prn);		/* Simple CMY color */
      else
        fputs("\033*r-4U", prn);		/* Simple KCMY color */
    }
  }

  if ((caps.printer_type & PCL_PRINTER_TIFF) == PCL_PRINTER_TIFF)
  {
    fputs("\033*b2M", prn);			/* Mode 2 (TIFF) */
    writefunc = pcl_mode2;
  }
  else
  {
    fputs("\033*b0M", prn);			/* Mode 0 (no compression) */
    writefunc = pcl_mode0;
  }

 /*
  * Convert image size to printer resolution and setup the page for printing...
  */

  out_width  = xdpi * out_width / 72;
  out_height = ydpi * out_height / 72;

  fprintf(prn, "\033&a%dH", 10 * left - 180);	/* Set left raster position */
  fprintf(prn, "\033&a%dV", 10 * top);		/* Set top raster position */
  fprintf(prn, "\033*r%dS", out_width);		/* Set raster width */
  fprintf(prn, "\033*r%dT", out_height);	/* Set raster height */

  fputs("\033*r1A", prn); 			/* Start GFX */

 /*
  * Allocate memory for the raster data...
  */

  length = (out_width + 7) / 8;
  if (do_cret)
    length *= 2;

  if (output_type == OUTPUT_GRAY)
  {
    black   = malloc(length);
    cyan    = NULL;
    magenta = NULL;
    yellow  = NULL;
  }
  else
  {
    cyan    = malloc(length);
    magenta = malloc(length);
    yellow  = malloc(length);
  
    if ((caps.color_type & PCL_COLOR_CMY) == PCL_COLOR_CMY)
      black = NULL;
    else
      black = malloc(length);
  }
    
 /*
  * Output the page, rotating as necessary...
  */

  v->density *= printer->printvars.density;
  v->saturation *= printer->printvars.saturation;

  if (landscape)
    dither = init_dither(image_height, out_width, v);
  else
    dither = init_dither(image_width, out_width, v);
  switch (v->image_type)
    {
    case IMAGE_LINE_ART:
      dither_set_ink_spread(dither, 19);
      break;
    case IMAGE_SOLID_TONE:
      dither_set_ink_spread(dither, 15);
      break;
    case IMAGE_CONTINUOUS:
      dither_set_ink_spread(dither, 14);
      break;
    }	    
  if (do_cret)
    {
      dither_set_c_ranges_simple(dither, 3, dot_sizes, v->density);
      dither_set_m_ranges_simple(dither, 3, dot_sizes, v->density);
      dither_set_y_ranges_simple(dither, 3, dot_sizes, v->density);
      dither_set_k_ranges_simple(dither, 3, dot_sizes, v->density);
    }

  dither_set_density(dither, v->density);

  if (landscape)
  {
    in  = malloc(image_height * image_bpp);
    out = malloc(image_height * out_bpp * 2);

    errdiv  = image_width / out_height;
    errmod  = image_width % out_height;
    errval  = 0;
    errlast = -1;
    errline  = image_width - 1;
    
    for (x = 0; x < out_height; x ++)
    {
#ifdef DEBUG
      printf("pcl_print: x = %d, line = %d, val = %d, mod = %d, height = %d\n",
             x, errline, errval, errmod, out_height);
#endif /* DEBUG */

      if ((x & 255) == 0)
	Image_note_progress(image, x, out_height);

      if (errline != errlast)
      {
        errlast = errline;
	Image_get_col(image, in, errline);
      }

      (*colorfunc)(in, out, image_height, image_bpp, cmap, v);

      if (do_cret)
      {
       /*
        * 4-level (CRet) dithers...
	*/

	if (output_type == OUTPUT_GRAY)
	{
          dither_black(out, x, dither, black);
          (*writefunc)(prn, black + length / 2, length / 2, 1);
          (*writefunc)(prn, black, length / 2, 0);
	}
	else 
	{
          dither_cmyk(out, x, dither, cyan, NULL, magenta, NULL,
		      yellow, NULL, black);

          (*writefunc)(prn, black + length / 2, length / 2, 0);
          (*writefunc)(prn, black, length / 2, 0);
          (*writefunc)(prn, cyan + length / 2, length / 2, 0);
          (*writefunc)(prn, cyan, length / 2, 0);
          (*writefunc)(prn, magenta + length / 2, length / 2, 0);
          (*writefunc)(prn, magenta, length / 2, 0);
          (*writefunc)(prn, yellow + length / 2, length / 2, 1);
          (*writefunc)(prn, yellow, length / 2, 0);
	}
      }
      else
      {
       /*
        * Standard 2-level dithers...
	*/

	if (output_type == OUTPUT_GRAY)
	{
	  if (v->image_type == IMAGE_MONOCHROME)
	    dither_fastblack(out, x, dither, black);
	  else
	    dither_black(out, x, dither, black);
          (*writefunc)(prn, black, length, 1);
	}
	else
	{
          dither_cmyk(out, x, dither, cyan, NULL, magenta, NULL,
		      yellow, NULL, black);

          if (black != NULL)
            (*writefunc)(prn, black, length, 0);
          (*writefunc)(prn, cyan, length, 0);
          (*writefunc)(prn, magenta, length, 0);
          (*writefunc)(prn, yellow, length, 1);
	}
      }

      errval += errmod;
      errline -= errdiv;
      if (errval >= out_height)
      {
        errval -= out_height;
        errline --;
      }
    }
  }
  else
  {
    in  = malloc(image_width * image_bpp);
    out = malloc(image_width * out_bpp * 2);

    errdiv  = image_height / out_height;
    errmod  = image_height % out_height;
    errval  = 0;
    errlast = -1;
    errline  = 0;
    
    for (y = 0; y < out_height; y ++)
    {
#ifdef DEBUG
      printf("pcl_print: y = %d, line = %d, val = %d, mod = %d, height = %d\n",
             y, errline, errval, errmod, out_height);
#endif /* DEBUG */

      if ((y & 255) == 0)
	Image_note_progress(image, y, out_height);

      if (errline != errlast)
      {
        errlast = errline;
	Image_get_row(image, in, errline);
      }

      (*colorfunc)(in, out, image_width, image_bpp, cmap, v);

      if (do_cret)
      {
       /*
        * 4-level (CRet) dithers...
	*/

	if (output_type == OUTPUT_GRAY)
	{
          dither_black(out, y, dither, black);
          (*writefunc)(prn, black + length / 2, length / 2, 1);
          (*writefunc)(prn, black, length / 2, 0);
	}
	else 
	{
          dither_cmyk(out, y, dither, cyan, NULL, magenta, NULL,
		      yellow, NULL, black);

          (*writefunc)(prn, black + length / 2, length / 2, 0);
          (*writefunc)(prn, black, length / 2, 0);
          (*writefunc)(prn, cyan + length / 2, length / 2, 0);
          (*writefunc)(prn, cyan, length / 2, 0);
          (*writefunc)(prn, magenta + length / 2, length / 2, 0);
          (*writefunc)(prn, magenta, length / 2, 0);
          (*writefunc)(prn, yellow + length / 2, length / 2, 1);
          (*writefunc)(prn, yellow, length / 2, 0);
	}
      }
      else
      {
       /*
        * Standard 2-level dithers...
	*/

	if (output_type == OUTPUT_GRAY)
	{
	  if (v->image_type == IMAGE_MONOCHROME)
	    dither_fastblack(out, y, dither, black);
	  else
	    dither_black(out, y, dither, black);
          (*writefunc)(prn, black, length, 1);
	}
	else
	{
          dither_cmyk(out, y, dither, cyan, NULL, magenta, NULL,
		      yellow, NULL, black);

          if (black != NULL)
            (*writefunc)(prn, black, length, 0);
          (*writefunc)(prn, cyan, length, 0);
          (*writefunc)(prn, magenta, length, 0);
          (*writefunc)(prn, yellow, length, 1);
	}
      }

      errval += errmod;
      errline += errdiv;
      if (errval >= out_height)
      {
        errval -= out_height;
        errline ++;
      }
    }
  }
  free_dither(dither);


 /*
  * Cleanup...
  */

  free(in);
  free(out);

  if (black != NULL)
    free(black);
  if (cyan != NULL)
  {
    free(cyan);
    free(magenta);
    free(yellow);
  }

  if ((caps.printer_type & PCL_PRINTER_NEW_ERG) == PCL_PRINTER_NEW_ERG)
    fputs("\033*rC", prn);
  else
    fputs("\033*rB", prn);

  fputs("\033&l0H", prn);		/* Eject page */
  fputs("\033E", prn);			/* PCL reset */
}



/*
 * 'pcl_mode0()' - Send PCL graphics using mode 0 (no) compression.
 */

void
pcl_mode0(FILE          *prn,		/* I - Print file or command */
          unsigned char *line,		/* I - Output bitmap data */
          int           length,		/* I - Length of bitmap data */
          int           last_plane)	/* I - True if this is the last plane */
{
  fprintf(prn, "\033*b%d%c", length, last_plane ? 'W' : 'V');
  fwrite(line, length, 1, prn);
}


/*
 * 'pcl_mode2()' - Send PCL graphics using mode 2 (TIFF) compression.
 */

void
pcl_mode2(FILE          *prn,		/* I - Print file or command */
          unsigned char *line,		/* I - Output bitmap data */
          int           length,		/* I - Length of bitmap data */
          int           last_plane)	/* I - True if this is the last plane */
{
  unsigned char	comp_buf[1536],		/* Compression buffer */
		*comp_ptr,		/* Current slot in buffer */
		*start,			/* Start of compressed data */
		repeat;			/* Repeating char */
  int		count,			/* Count of compressed bytes */
		tcount;			/* Temporary count < 128 */


 /*
  * Compress using TIFF "packbits" run-length encoding...
  */

  comp_ptr = comp_buf;

  while (length > 0)
  {
   /*
    * Get a run of non-repeated chars...
    */

    start  = line;
    line   += 2;
    length -= 2;

    while (length > 0 && (line[-2] != line[-1] || line[-1] != line[0]))
    {
      line ++;
      length --;
    }

    line   -= 2;
    length += 2;

   /*
    * Output the non-repeated sequences (max 128 at a time).
    */

    count = line - start;
    while (count > 0)
    {
      tcount = count > 128 ? 128 : count;

      comp_ptr[0] = tcount - 1;
      memcpy(comp_ptr + 1, start, tcount);

      comp_ptr += tcount + 1;
      start    += tcount;
      count    -= tcount;
    }

    if (length <= 0)
      break;

   /*
    * Find the repeated sequences...
    */

    start  = line;
    repeat = line[0];

    line ++;
    length --;

    while (length > 0 && *line == repeat)
    {
      line ++;
      length --;
    }

   /*
    * Output the repeated sequences (max 128 at a time).
    */

    count = line - start;
    while (count > 0)
    {
      tcount = count > 128 ? 128 : count;

      comp_ptr[0] = 1 - tcount;
      comp_ptr[1] = repeat;

      comp_ptr += 2;
      count    -= tcount;
    }
  }

 /*
  * Send a line of raster graphics...
  */

  fprintf(prn, "\033*b%d%c", (int)(comp_ptr - comp_buf), last_plane ? 'W' : 'V');
  fwrite(comp_buf, comp_ptr - comp_buf, 1, prn);
}


/*
 *   $Log: print-pcl.c,v $
 *   Revision 1.46  2000/04/20 02:49:25  rlk
 *   Clean up the solid color and line art modes a bit.
 *
 *   Revision 1.45  2000/04/20 02:42:54  rlk
 *   Reduce initial memory footprint.
 *
 *   Add random Floyd-Steinberg dither.
 *
 *   Revision 1.44  2000/04/18 12:21:53  rlk
 *   Fix incorrect printing for variable drop sizes
 *
 *   Revision 1.43  2000/04/16 21:31:32  rlk
 *   Choice of dithering algorithms
 *
 *   Revision 1.42  2000/04/16 02:52:39  rlk
 *   New dithering code
 *
 *   Revision 1.41  2000/04/13 19:08:11  davehill
 *   Added DJ340, DJ400, DJ2000, DJ2500.
 *   Added Quick-dry photo & transparency media.
 *   Added Media Source selection for Deskjets where supported.
 *   600x600 mono mode is supported in DJ600/800/1100/2000.
 *   Some printers don't support "custom" media sizes.
 *   Added 1220C to 1120C case (A3 paper size).
 *   Removed A6 paper size, it is not supported directly.
 *
 *   Tidy-up:
 *   Removed all papersizes not mentioned in print-utils.c.
 *   Fixed coding mistakes in "for" statements.
 *   Removed double-strdup in pcl_parameters (memory leak).
 *   Added more warning messages.
 *
 *   Revision 1.40.4.1  2000/04/11 01:53:06  rlk
 *   Yet another dither hack
 *
 *   Revision 1.40  2000/03/21 19:09:02  davehill
 *   Added Deskjet 9xx series.
 *
 *   Revision 1.39  2000/03/20 21:03:31  davehill
 *   Added "Bond" and "Photo" paper types to pcl-unprint and print-pcl.
 *   Corrected Depletion output for old Deskjets in print-pcl.
 *
 *   Revision 1.38  2000/03/13 13:31:26  rlk
 *   Add monochrome mode
 *
 *   Revision 1.37  2000/03/11 17:30:15  rlk
 *   Significant dither changes; addition of line art/solid color/continuous tone modes
 *
 *   Revision 1.36  2000/03/07 02:54:05  rlk
 *   Move CVS history logs to the end of the file
 *
 *   Revision 1.35  2000/03/06 01:32:05  rlk
 *   more rearrangement
 *
 *   Revision 1.34  2000/02/28 18:37:31  davehill
 *   Fixed the "configure data" command again!
 *
 *   Revision 1.33  2000/02/26 00:14:44  rlk
 *   Rename dither_{black,cmyk}4 to dither_{black,cmyk}_n, and add argument to specify how levels are to be encoded
 *
 *   Revision 1.32  2000/02/25 22:13:08  davehill
 *   Added Paper size database to handle more of the new paper sizes
 *   added a while ago, anything else is handled as "custom".
 *
 *   Revision 1.31  2000/02/23 20:29:08  davehill
 *   Replaced all "model ==" code with a capabilities database.
 *   According to the ghostscript driver (and the HP windows driver), the
 *   "new" end raster graphics command is *rC, not *rbC.
 *   Use correct commands to set high quality output if dpi >= 300
 *
 *   Revision 1.30  2000/02/19 12:45:27  davehill
 *   Fixed OUTPUT_COLOR vs OUTPUT_GRAY.
 *   Fixed number of planes output for DJ600 in 600dpi mode.
 *
 *   Revision 1.29  2000/02/16 00:59:19  rlk
 *   1) Use correct convert functions (canon, escp2, pcl, ps).
 *
 *   2) Fix gray_to_rgb increment (print-util)
 *
 *   3) Fix dither update (print-dither)
 *
 *   Revision 1.28  2000/02/15 22:04:08  davehill
 *   Added fix when (left < 0)
 *
 *   Revision 1.27  2000/02/15 03:51:40  rlk
 *
 *   1) It wasn't possible to print to the edge of the page (as defined by
 *      the printer).
 *
 *   2) The page top/bottom/left/right (particularly bottom and right) in
 *      the size boxes wasn't displayed accurately (it *had* been coded in
 *      1/10", because that's the units used to print out the pager --
 *      really sillyl, that -- now it's all in points, which is more
 *      reasonable if still not all that precise).
 *
 *   3) The behavior of landscape mode was weird, to say the least.
 *
 *   4) Calculating the size based on scaling was also weird -- in portrait
 *      mode it just looked at the height of the page vs. the height of the
 *      image, and in landscape it just looked at width of the page and
 *      height of the image.  Now it looks at both axes and scales so that
 *      the larger of the two ratios (widths and heights) is set equal to
 *      the scale factor.  That seems more intuitive to me, at any rate.
 *      It avoids flipping between landscape and portrait mode as you
 *      rescale the image in auto mode (which seems just plain bizarre to
 *      me).
 *
 *   5) I changed the escp2 stuff so that the distance from the paper edge
 *      will be identical in softweave and in microweave mode.  Henryk,
 *      that might not quite be what you intended (it's the opposite of
 *      what you actually did), but at least microweave and softweave
 *      should generate stuff that looks consistent.
 *
 *   Revision 1.26  2000/02/13 03:14:26  rlk
 *   Bit of an oops here about printer models; also start on print-gray-using-color mode for better quality
 *
 *   Revision 1.25  2000/02/10 00:28:32  rlk
 *   Fix landscape vs. portrait problem
 *
 *   Revision 1.24  2000/02/09 02:56:27  rlk
 *   Put lut inside vars
 *
 *   Revision 1.23  2000/02/08 12:09:22  davehill
 *   Deskjet 600C is CMY, the rest of the 6xxC series are CMYK.
 *
 *   Revision 1.22  2000/02/06 22:31:04  rlk
 *   1) Use old methods only for microweave printing.
 *
 *   2) remove MAX_DPI from print.h since it's no longer necessary.
 *
 *   3) Remove spurious CVS logs that were just clutter.
 *
 *   Revision 1.21  2000/02/06 21:25:10  davehill
 *   Fixed max paper sizes.
 *
 *   Revision 1.20  2000/02/06 03:59:09  rlk
 *   More work on the generalized dithering parameters stuff.  At this point
 *   it really looks like a proper object.  Also dynamically allocate the error
 *   buffers.  This segv'd a lot, which forced me to efence it, which was just
 *   as well because I found a few problems as a result...
 *
 *   Revision 1.19  2000/02/02 03:03:55  rlk
 *   Move all the constants into members of a struct.  This will eventually permit
 *   us to use different dithering constants for each printer, or even vary them
 *   on the fly.  Currently there's a static dither_t that contains constants,
 *   but that's the easy part to fix...
 *
 *   Revision 1.18  2000/01/29 02:34:30  rlk
 *   1) Remove globals from everything except print.c.
 *
 *   2) Remove broken 1440x720 and 2880x720 microweave modes.
 *
 *   Revision 1.17  2000/01/25 19:51:27  rlk
 *   1) Better attempt at supporting newer Epson printers.
 *
 *   2) Generalized paper size support.
 *
 *   Revision 1.16  2000/01/17 22:23:31  rlk
 *   Print 3.1.0
 *
 *   Revision 1.15  2000/01/17 02:05:47  rlk
 *   Much stuff:
 *
 *   1) Fixes from 3.0.5
 *
 *   2) First cut at enhancing monochrome and four-level printing with stuff from
 *   the color print function.
 *
 *   3) Preliminary support (pre-support) for 440/640/740/900/750/1200.
 *
 *   Revision 1.14.2.1  2000/01/15 14:33:02  rlk
 *   PCL and Gimp 1.0 patches from Dave Hill
 *
 *   Revision 1.14  2000/01/08 23:30:56  rlk
 *   Y2K copyright
 *
 *   Revision 1.13  1999/11/23 02:11:37  rlk
 *   Rationalize variables, pass 3
 *
 *   Revision 1.12  1999/11/23 01:45:00  rlk
 *   Rationalize variables -- pass 2
 *
 *   Revision 1.11  1999/11/10 01:13:44  rlk
 *   multi-pass
 *
 *   Revision 1.10  1999/10/26 23:36:51  rlk
 *   Comment out all remaining 16-bit code, and rename 16-bit functions to "standard" names
 *
 *   Revision 1.9  1999/10/26 02:10:30  rlk
 *   Mostly fix save/load
 *
 *   Move all gimp, glib, gtk stuff into print.c (take it out of everything else).
 *   This should help port it to more general purposes later.
 *
 *   Revision 1.8  1999/10/25 23:31:59  rlk
 *   16-bit clean
 *
 *   Revision 1.7  1999/10/21 01:27:37  rlk
 *   More progress toward full 16-bit rendering
 *
 *   Revision 1.6  1999/10/19 02:04:59  rlk
 *   Merge all of the single-level print_cmyk functions
 *
 *   Revision 1.5  1999/10/17 23:44:07  rlk
 *   16-bit everything (untested)
 *
 *   Revision 1.4  1999/10/17 23:01:01  rlk
 *   Move various dither functions into print-utils.c
 *
 *   Revision 1.3  1999/10/14 01:59:59  rlk
 *   Saturation
 *
 *   Revision 1.2  1999/09/12 00:12:24  rlk
 *   Current best stuff
 *
 *   Revision 1.12  1998/05/16  18:27:59  mike
 *   Added support for 4-level "CRet" mode of 800/1100 series printers.
 *
 *   Revision 1.11  1998/05/15  21:01:51  mike
 *   Updated image positioning code (invert top and center left/top independently)
 *
 *   Revision 1.10  1998/05/08  21:22:00  mike
 *   Added quality mode command for DeskJet printers (high quality for 300
 *   DPI or higher).
 *
 *   Revision 1.9  1998/05/08  19:20:50  mike
 *   Updated to support media size, imageable area, and parameter functions.
 *   Added support for scaling modes - scale by percent or scale by PPI.
 *
 *   Revision 1.8  1998/01/21  21:33:47  mike
 *   Updated copyright.
 *
 *   Revision 1.7  1997/11/12  15:57:48  mike
 *   Minor changes for clean compiles under Digital UNIX.
 *
 *   Revision 1.7  1997/11/12  15:57:48  mike
 *   Minor changes for clean compiles under Digital UNIX.
 *
 *   Revision 1.6  1997/10/02  17:57:26  mike
 *   Updated positioning code to use "decipoint" commands.
 *
 *   Revision 1.5  1997/07/30  20:33:05  mike
 *   Final changes for 1.1 release.
 *
 *   Revision 1.4  1997/07/30  18:47:39  mike
 *   Added scaling, orientation, and offset options.
 *
 *   Revision 1.3  1997/07/03  13:24:12  mike
 *   Updated documentation for 1.0 release.
 *
 *   Revision 1.2  1997/07/02  18:48:14  mike
 *   Added mode 2 compression code.
 *   Fixed bug in pcl_mode0 and pcl_mode2 - wasn't sending 'V' or 'W' at
 *   the right times.
 *
 *   Revision 1.2  1997/07/02  18:48:14  mike
 *   Added mode 2 compression code.
 *   Fixed bug in pcl_mode0 and pcl_mode2 - wasn't sending 'V' or 'W' at
 *   the right times.
 *
 *   Revision 1.1  1997/07/02  13:51:53  mike
 *   Initial revision
 *
 * End of "$Id: print-pcl.c,v 1.46 2000/04/20 02:49:25 rlk Exp $".
 */
