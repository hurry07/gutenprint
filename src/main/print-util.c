/*
 * "$Id: print-util.c,v 1.8.4.8 2001/11/18 15:40:38 sharkey Exp $"
 *
 *   Print plug-in driver utility functions for the GIMP.
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
 */

/*
 * This file must include only standard C header files.  The core code must
 * compile on generic platforms that don't support glib, gimp, gtk, etc.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <gimp-print/gimp-print.h>
#include "gimp-print-internal.h"
#include <gimp-print/gimp-print-intl-internal.h>
#include <math.h>
#include <limits.h>
#if defined(HAVE_VARARGS_H) && !defined(HAVE_STDARG_H)
#include <varargs.h>
#else
#include <stdarg.h>
#endif
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define FMIN(a, b) ((a) < (b) ? (a) : (b))

typedef struct stp_internal_option
{
  char *name;
  size_t length;
  char *data;
  struct stp_internal_option *next;
  struct stp_internal_option *prev;
} stp_internal_option_t;

typedef struct					/* Plug-in variables */
{
  const char	*output_to,	/* Name of file or command to print to */
	*driver,		/* Name of printer "driver" */
	*ppd_file,		/* PPD file */
        *resolution,		/* Resolution */
	*media_size,		/* Media size */
	*media_type,		/* Media type */
	*media_source,		/* Media source */
	*ink_type,		/* Ink or cartridge */
	*dither_algorithm;	/* Dithering algorithm */
  int	output_type;		/* Color or grayscale output */
  float	brightness;		/* Output brightness */
  float	scaling;		/* Scaling, percent of printable area */
  int	orientation,		/* Orientation - 0 = port., 1 = land.,
				   -1 = auto */
	left,			/* Offset from lower-lefthand corner, points */
	top;			/* ... */
  float gamma;                  /* Gamma */
  float contrast,		/* Output Contrast */
	cyan,			/* Output red level */
	magenta,		/* Output green level */
	yellow;			/* Output blue level */
  float	saturation;		/* Output saturation */
  float	density;		/* Maximum output density */
  int	image_type;		/* Image type (line art etc.) */
  int	unit;			/* Units for preview area 0=Inch 1=Metric */
  float app_gamma;		/* Application gamma */
  int	page_width;		/* Width of page in points */
  int	page_height;		/* Height of page in points */
  int	input_color_model;	/* Color model for this device */
  int	output_color_model;	/* Color model for this device */
  void  *lut;			/* Look-up table */
  void  *driver_data;		/* Private data of the driver */
  unsigned char *cmap;		/* Color map */
  void (*outfunc)(void *data, const char *buffer, size_t bytes);
  void *outdata;
  void (*errfunc)(void *data, const char *buffer, size_t bytes);
  void *errdata;
  stp_internal_option_t *options;
  int verified;			/* Ensure that params are OK! */
} stp_internal_vars_t;

typedef struct stp_internal_printer
{
  const char	*long_name,	/* Long name for UI */
		*driver;	/* Short name for printrc file */
  int	model;			/* Model number */
  const stp_printfuncs_t *printfuncs;
  stp_internal_vars_t printvars;
} stp_internal_printer_t;

typedef struct
{
  const char *name;
  const char *text;
  unsigned width;
  unsigned height;
  unsigned top;
  unsigned left;
  unsigned bottom;
  unsigned right;
  stp_papersize_unit_t paper_unit;
} stp_internal_papersize_t;

static const stp_internal_vars_t default_vars =
{
	"",			/* Name of file or command to print to */
	N_ ("ps2"),	       	/* Name of printer "driver" */
	"",			/* Name of PPD file */
	"",			/* Output resolution */
	"",			/* Size of output media */
	"",			/* Type of output media */
	"",			/* Source of output media */
	"",			/* Ink type */
	"",			/* Dither algorithm */
	OUTPUT_COLOR,		/* Color or grayscale output */
	1.0,			/* Output brightness */
	100.0,			/* Scaling (100% means entire printable area, */
				/*          -XXX means scale by PPI) */
	-1,			/* Orientation (-1 = automatic) */
	-1,			/* X offset (-1 = center) */
	-1,			/* Y offset (-1 = center) */
	1.0,			/* Screen gamma */
	1.0,			/* Contrast */
	1.0,			/* Cyan */
	1.0,			/* Magenta */
	1.0,			/* Yellow */
	1.0,			/* Output saturation */
	1.0,			/* Density */
	IMAGE_CONTINUOUS,	/* Image type */
	0,			/* Unit 0=Inch */
	1.0,			/* Application gamma placeholder */
	0,			/* Page width */
	0,			/* Page height */
	COLOR_MODEL_RGB,	/* Input color model */
	COLOR_MODEL_RGB		/* Output color model */
};

static const stp_internal_vars_t min_vars =
{
	"",			/* Name of file or command to print to */
	N_ ("ps2"),			/* Name of printer "driver" */
	"",			/* Name of PPD file */
	"",			/* Output resolution */
	"",			/* Size of output media */
	"",			/* Type of output media */
	"",			/* Source of output media */
	"",			/* Ink type */
	"",			/* Dither algorithm */
	0,			/* Color or grayscale output */
	0,			/* Output brightness */
	5.0,			/* Scaling (100% means entire printable area, */
				/*          -XXX means scale by PPI) */
	-1,			/* Orientation (-1 = automatic) */
	-1,			/* X offset (-1 = center) */
	-1,			/* Y offset (-1 = center) */
	0.1,			/* Screen gamma */
	0,			/* Contrast */
	0,			/* Cyan */
	0,			/* Magenta */
	0,			/* Yellow */
	0,			/* Output saturation */
	.1,			/* Density */
	0,			/* Image type */
	0,			/* Unit 0=Inch */
	1.0,			/* Application gamma placeholder */
	0,			/* Page width */
	0,			/* Page height */
	0,			/* Input color model */
	0			/* Output color model */
};

static const stp_internal_vars_t max_vars =
{
	"",			/* Name of file or command to print to */
	N_ ("ps2"),			/* Name of printer "driver" */
	"",			/* Name of PPD file */
	"",			/* Output resolution */
	"",			/* Size of output media */
	"",			/* Type of output media */
	"",			/* Source of output media */
	"",			/* Ink type */
	"",			/* Dither algorithm */
	OUTPUT_RAW_CMYK,	/* Color or grayscale output */
	2.0,			/* Output brightness */
	100.0,			/* Scaling (100% means entire printable area, */
				/*          -XXX means scale by PPI) */
	-1,			/* Orientation (-1 = automatic) */
	-1,			/* X offset (-1 = center) */
	-1,			/* Y offset (-1 = center) */
	4.0,			/* Screen gamma */
	4.0,			/* Contrast */
	4.0,			/* Cyan */
	4.0,			/* Magenta */
	4.0,			/* Yellow */
	9.0,			/* Output saturation */
	2.0,			/* Density */
	NIMAGE_TYPES - 1,	/* Image type */
	1,			/* Unit 0=Inch */
	1.0,			/* Application gamma placeholder */
	0,			/* Page width */
	0,			/* Page height */
	NCOLOR_MODELS - 1,	/* Input color model */
	NCOLOR_MODELS - 1	/* Output color model */
};

stp_vars_t
stp_allocate_vars(void)
{
  void *retval = stp_malloc(sizeof(stp_internal_vars_t));
  memset(retval, 0, sizeof(stp_internal_vars_t));
  stp_copy_vars(retval, (stp_vars_t)&default_vars);
  return (retval);
}

#define SAFE_FREE(x)				\
do						\
{						\
  if ((x))					\
    stp_free((char *)(x));			\
  ((x)) = NULL;					\
} while (0)

static char *
c_strdup(const char *s)
{
  char *ret;
  if (!s)
    {
      ret = stp_malloc(1);
      ret[0] = 0;
      return ret;
    }
  else
    {
      ret = stp_malloc(strlen(s) + 1);
      strcpy(ret, s);
      return ret;
    }
}

static char *
c_strndup(const char *s, int n)
{
  char *ret;
  if (!s || n < 0)
    {
      ret = stp_malloc(1);
      ret[0] = 0;
      return ret;
    }
  else
    {
      ret = stp_malloc(n + 1);
      strncpy(ret, s, n);
      ret[n] = 0;
      return ret;
    }
}

void
stp_free_vars(stp_vars_t vv)
{
  stp_internal_vars_t *v = (stp_internal_vars_t *) vv;
  SAFE_FREE(v->output_to);
  SAFE_FREE(v->driver);
  SAFE_FREE(v->ppd_file);
  SAFE_FREE(v->resolution);
  SAFE_FREE(v->media_size);
  SAFE_FREE(v->media_type);
  SAFE_FREE(v->media_source);
  SAFE_FREE(v->ink_type);
  SAFE_FREE(v->dither_algorithm);
}

#define DEF_STRING_FUNCS(s)				\
void							\
stp_set_##s(stp_vars_t vv, const char *val)		\
{							\
  stp_internal_vars_t *v = (stp_internal_vars_t *) vv;	\
  if (v->s == val)					\
    return;						\
  SAFE_FREE(v->s);					\
  v->s = c_strdup(val);					\
  v->verified = 0;					\
}							\
							\
void							\
stp_set_##s##_n(stp_vars_t vv, const char *val, int n)	\
{							\
  stp_internal_vars_t *v = (stp_internal_vars_t *) vv;	\
  if (v->s == val)					\
    return;						\
  SAFE_FREE(v->s);					\
  v->s = c_strndup(val, n);				\
  v->verified = 0;					\
}							\
							\
const char *						\
stp_get_##s(const stp_vars_t vv)			\
{							\
  stp_internal_vars_t *v = (stp_internal_vars_t *) vv;	\
  return v->s;						\
}

#define DEF_FUNCS(s, t)					\
void							\
stp_set_##s(stp_vars_t vv, t val)			\
{							\
  stp_internal_vars_t *v = (stp_internal_vars_t *) vv;	\
  v->verified = 0;					\
  v->s = val;						\
}							\
							\
t							\
stp_get_##s(const stp_vars_t vv)			\
{							\
  stp_internal_vars_t *v = (stp_internal_vars_t *) vv;	\
  return v->s;						\
}

DEF_STRING_FUNCS(output_to)
DEF_STRING_FUNCS(driver)
DEF_STRING_FUNCS(ppd_file)
DEF_STRING_FUNCS(resolution)
DEF_STRING_FUNCS(media_size)
DEF_STRING_FUNCS(media_type)
DEF_STRING_FUNCS(media_source)
DEF_STRING_FUNCS(ink_type)
DEF_STRING_FUNCS(dither_algorithm)
DEF_FUNCS(output_type, int)
DEF_FUNCS(orientation, int)
DEF_FUNCS(left, int)
DEF_FUNCS(top, int)
DEF_FUNCS(image_type, int)
DEF_FUNCS(unit, int)
DEF_FUNCS(page_width, int)
DEF_FUNCS(page_height, int)
DEF_FUNCS(input_color_model, int)
DEF_FUNCS(output_color_model, int)
DEF_FUNCS(brightness, float)
DEF_FUNCS(scaling, float)
DEF_FUNCS(gamma, float)
DEF_FUNCS(contrast, float)
DEF_FUNCS(cyan, float)
DEF_FUNCS(magenta, float)
DEF_FUNCS(yellow, float)
DEF_FUNCS(saturation, float)
DEF_FUNCS(density, float)
DEF_FUNCS(app_gamma, float)
DEF_FUNCS(lut, void *)
DEF_FUNCS(outdata, void *)
DEF_FUNCS(errdata, void *)
DEF_FUNCS(driver_data, void *)
DEF_FUNCS(cmap, unsigned char *)
DEF_FUNCS(outfunc, stp_outfunc_t)
DEF_FUNCS(errfunc, stp_outfunc_t)

void
stp_set_verified(stp_vars_t vv, int val)
{
  stp_internal_vars_t *v = (stp_internal_vars_t *) vv;
  v->verified = val;
}

int
stp_get_verified(const stp_vars_t vv)
{
  stp_internal_vars_t *v = (stp_internal_vars_t *) vv;
  return v->verified;
}

void
stp_copy_options(stp_vars_t vd, const stp_vars_t vs)
{
  const stp_internal_vars_t *src = (const stp_internal_vars_t *)vs;
  stp_internal_vars_t *dest = (stp_internal_vars_t *)vd;
  stp_internal_option_t *opt = (stp_internal_option_t *) src->options;
  stp_internal_option_t *popt = NULL;
  if (opt)
    {
      stp_internal_option_t *nopt = stp_malloc(sizeof(stp_internal_option_t));
      stp_set_verified(vd, 0);
      dest->options = nopt;
      memcpy(nopt, opt, sizeof(stp_internal_option_t));
      nopt->name = stp_malloc(strlen(opt->name) + 1);
      strcpy(nopt->name, opt->name);
      nopt->data = stp_malloc(opt->length);
      memcpy(nopt->data, opt->data, opt->length);
      opt = opt->next;
      popt = nopt;
      while (opt)
        {
          nopt = stp_malloc(sizeof(stp_internal_option_t));
          memcpy(nopt, opt, sizeof(stp_internal_option_t));
          nopt->prev = popt;
          popt->next = nopt;
          nopt->name = stp_malloc(strlen(opt->name) + 1);
          strcpy(nopt->name, opt->name);
          nopt->data = stp_malloc(opt->length);
          memcpy(nopt->data, opt->data, opt->length);
          opt = opt->next;
          popt = nopt;
        }
    }
}

void
stp_copy_vars(stp_vars_t vd, const stp_vars_t vs)
{
  if (vs == vd)
    return;
  stp_set_output_to(vd, stp_get_output_to(vs));
  stp_set_driver(vd, stp_get_driver(vs));
  stp_set_driver_data(vd, stp_get_driver_data(vs));
  stp_set_ppd_file(vd, stp_get_ppd_file(vs));
  stp_set_resolution(vd, stp_get_resolution(vs));
  stp_set_media_size(vd, stp_get_media_size(vs));
  stp_set_media_type(vd, stp_get_media_type(vs));
  stp_set_media_source(vd, stp_get_media_source(vs));
  stp_set_ink_type(vd, stp_get_ink_type(vs));
  stp_set_dither_algorithm(vd, stp_get_dither_algorithm(vs));
  stp_set_output_type(vd, stp_get_output_type(vs));
  stp_set_orientation(vd, stp_get_orientation(vs));
  stp_set_left(vd, stp_get_left(vs));
  stp_set_top(vd, stp_get_top(vs));
  stp_set_image_type(vd, stp_get_image_type(vs));
  stp_set_unit(vd, stp_get_unit(vs));
  stp_set_page_width(vd, stp_get_page_width(vs));
  stp_set_page_height(vd, stp_get_page_height(vs));
  stp_set_brightness(vd, stp_get_brightness(vs));
  stp_set_scaling(vd, stp_get_scaling(vs));
  stp_set_gamma(vd, stp_get_gamma(vs));
  stp_set_contrast(vd, stp_get_contrast(vs));
  stp_set_cyan(vd, stp_get_cyan(vs));
  stp_set_magenta(vd, stp_get_magenta(vs));
  stp_set_yellow(vd, stp_get_yellow(vs));
  stp_set_saturation(vd, stp_get_saturation(vs));
  stp_set_density(vd, stp_get_density(vs));
  stp_set_app_gamma(vd, stp_get_app_gamma(vs));
  stp_set_input_color_model(vd, stp_get_input_color_model(vd));
  stp_set_output_color_model(vd, stp_get_output_color_model(vd));
  stp_set_lut(vd, stp_get_lut(vs));
  stp_set_outdata(vd, stp_get_outdata(vs));
  stp_set_errdata(vd, stp_get_errdata(vs));
  stp_set_cmap(vd, stp_get_cmap(vs));
  stp_set_outfunc(vd, stp_get_outfunc(vs));
  stp_set_errfunc(vd, stp_get_errfunc(vs));
  stp_copy_options(vd, vs);
  stp_set_verified(vd, stp_get_verified(vs));
}

stp_vars_t
stp_allocate_copy(const stp_vars_t vs)
{
  stp_vars_t vd = stp_allocate_vars();
  stp_copy_vars(vd, vs);
  return (vd);
}

#define ICLAMP(value)						\
do								\
{								\
  if (stp_get_##value(user) < stp_get_##value(min))		\
    stp_set_##value(user, stp_get_##value(min));		\
  else if (stp_get_##value(user) > stp_get_##value(max))	\
    stp_set_##value(user, stp_get_##value(max));		\
} while (0)

void
stp_merge_printvars(stp_vars_t user, const stp_vars_t print)
{
  const stp_vars_t max = stp_maximum_settings();
  const stp_vars_t min = stp_minimum_settings();
  stp_set_cyan(user, stp_get_cyan(user) * stp_get_cyan(print));
  ICLAMP(cyan);
  stp_set_magenta(user, stp_get_magenta(user) * stp_get_magenta(print));
  ICLAMP(magenta);
  stp_set_yellow(user, stp_get_yellow(user) * stp_get_yellow(print));
  ICLAMP(yellow);
  stp_set_contrast(user, stp_get_contrast(user) * stp_get_contrast(print));
  ICLAMP(contrast);
  stp_set_brightness(user, stp_get_brightness(user)*stp_get_brightness(print));
  ICLAMP(brightness);
  stp_set_gamma(user, stp_get_gamma(user) / stp_get_gamma(print));
  ICLAMP(gamma);
  stp_set_saturation(user, stp_get_saturation(user)*stp_get_saturation(print));
  ICLAMP(saturation);
  stp_set_density(user, stp_get_density(user) * stp_get_density(print));
  ICLAMP(density);
  if (stp_get_output_type(print) == OUTPUT_GRAY &&
      stp_get_output_type(user) == OUTPUT_COLOR)
    stp_set_output_type(user, OUTPUT_GRAY);
}

/*
 * 'stp_default_media_size()' - Return the size of a default page size.
 */

/*
 * Sizes are converted to 1/72in, then rounded down so that we don't
 * print off the edge of the paper.
 */
static stp_internal_papersize_t paper_sizes[] =
{
  /* Common imperial page sizes */
  { "Letter",		N_ ("Letter"),
    612,  792, 0, 0, 0, 0, PAPERSIZE_ENGLISH },	/* 8.5in x 11in */
  { "Legal",		N_ ("Legal"),
    612, 1008, 0, 0, 0, 0, PAPERSIZE_ENGLISH },	/* 8.5in x 14in */
  { "Tabloid",		N_ ("Tabloid"),
    792, 1224, 0, 0, 0, 0, PAPERSIZE_ENGLISH },	/*  11in x 17in */
  { "Executive",	N_ ("Executive"),
    522, 756, 0, 0, 0, 0, PAPERSIZE_ENGLISH },	/* 7.25 * 10.5in */
  { "Postcard",		N_ ("Postcard"),
    283,  416, 0, 0, 0, 0, PAPERSIZE_ENGLISH },	/* 100mm x 147mm */
  { "w216h360",		N_ ("3x5"),
    216,  360, 0, 0, 0, 0, PAPERSIZE_ENGLISH },
  { "w288h432",		N_ ("4x6"),
    288,  432, 0, 0, 0, 0, PAPERSIZE_ENGLISH },
  { "w324h495",		N_ ("Epson 4x6 Photo Paper"),
    324, 495, 0, 0, 0, 0, PAPERSIZE_ENGLISH },
  { "w360h504",		N_ ("5x7"),
    360,  504, 0, 0, 0, 0, PAPERSIZE_ENGLISH },
  { "w360h576",		N_ ("5x8"),
    360,  576, 0, 0, 0, 0, PAPERSIZE_ENGLISH },
  { "w432h576",		N_ ("6x8"),
    432,  576, 0, 0, 0, 0, PAPERSIZE_ENGLISH },
  { "8x10",		N_ ("8x10"),
    576,  720, 0, 0, 0, 0, PAPERSIZE_ENGLISH },
  { "Statement",	N_ ("Manual"),
    396,  612, 0, 0, 0, 0, PAPERSIZE_ENGLISH },	/* 5.5in x 8.5in */
  { "TabloidExtra",	N_ ("12x18"),
    864, 1296, 0, 0, 0, 0, PAPERSIZE_ENGLISH },
  { "SuperB",		N_ ("Super B 13x19"),
    936, 1368, 0, 0, 0, 0, PAPERSIZE_ENGLISH },

  /* Other common photographic paper sizes */
  { "w576h864",		N_ ("8x12"),
    576,  864, 0, 0, 0, 0, PAPERSIZE_ENGLISH }, /* Sometimes used for 35 mm */
  { "w792h1008",	N_ ("11x14"),
    792, 1008, 0, 0, 0, 0, PAPERSIZE_ENGLISH },
  { "w1152h1440",	N_ ("16x20"),
    1152, 1440, 0, 0, 0, 0, PAPERSIZE_ENGLISH },
  { "w1152h1728",	N_ ("16x24"),
    1152, 1728, 0, 0, 0, 0, PAPERSIZE_ENGLISH }, /* 20x24 for 35 mm */
  { "w1440h1728",	N_ ("20x24"),
    1440, 1728, 0, 0, 0, 0, PAPERSIZE_ENGLISH },
  { "w1440h2160",	N_ ("20x30"),
    1440, 2160, 0, 0, 0, 0, PAPERSIZE_ENGLISH },	/* 24x30 for 35 mm */
  { "w1728h2160",	N_ ("24x30"),
    1728, 2160, 0, 0, 0, 0, PAPERSIZE_ENGLISH },
  { "w1728h2592",	N_ ("24x36"),
    1728, 2592, 0, 0, 0, 0, PAPERSIZE_ENGLISH }, /* Sometimes used for 35 mm */
  { "w2160h2880",	N_ ("30x40"),
    2160, 2880, 0, 0, 0, 0, PAPERSIZE_ENGLISH },

  /* International Paper Sizes (mostly taken from BS4000:1968) */

  /*
   * "A" series: Paper and boards, trimmed sizes
   *
   * "A" sizes are in the ratio 1 : sqrt(2).  A0 has a total area
   * of 1 square metre.  Everything is rounded to the nearest
   * millimetre.  Thus, A0 is 841mm x 1189mm.  Every other A
   * size is obtained by doubling or halving another A size.
   */
  { "w4768h6749",	N_ ("4A"),
    4768, 6749, 0, 0, 0, 0, PAPERSIZE_METRIC },	/* 1682mm x 2378mm */
  { "w3370h4768",	N_ ("2A"),
    3370, 4768, 0, 0, 0, 0, PAPERSIZE_METRIC },	/* 1189mm x 1682mm */
  { "A0",		N_ ("A0"),
    2384, 3370, 0, 0, 0, 0, PAPERSIZE_METRIC },	/*  841mm x 1189mm */
  { "A1",		N_ ("A1"),
    1684, 2384, 0, 0, 0, 0, PAPERSIZE_METRIC },	/*  594mm x  841mm */
  { "A2",		N_ ("A2"),
    1191, 1684, 0, 0, 0, 0, PAPERSIZE_METRIC },	/*  420mm x  594mm */
  { "A3",		N_ ("A3"),
    842, 1191, 0, 0, 0, 0, PAPERSIZE_METRIC },	/*  297mm x  420mm */
  { "A4",		N_ ("A4"),
    595,  842, 0, 0, 0, 0, PAPERSIZE_METRIC },	/*  210mm x  297mm */
  { "A5",		N_ ("A5"),
    420,  595, 0, 0, 0, 0, PAPERSIZE_METRIC },	/*  148mm x  210mm */
  { "A6",		N_ ("A6"),
    297,  420, 0, 0, 0, 0, PAPERSIZE_METRIC },	/*  105mm x  148mm */
  { "A7",		N_ ("A7"),
    210,  297, 0, 0, 0, 0, PAPERSIZE_METRIC },	/*   74mm x  105mm */
  { "A8",		N_ ("A8"),
    148,  210, 0, 0, 0, 0, PAPERSIZE_METRIC },	/*   52mm x   74mm */
  { "A9",		N_ ("A9"),
    105,  148, 0, 0, 0, 0, PAPERSIZE_METRIC },	/*   37mm x   52mm */
  { "A10",		N_ ("A10"),
    73,  105, 0, 0, 0, 0, PAPERSIZE_METRIC },	/*   26mm x   37mm */

  /*
   * Stock sizes for normal trims.
   * Allowance for trim is 3 millimetres.
   */
  { "w2437h3458",	N_ ("RA0"),
    2437, 3458, 0, 0, 0, 0, PAPERSIZE_METRIC },	/*  860mm x 1220mm */
  { "w1729h2437",	N_ ("RA1"),
    1729, 2437, 0, 0, 0, 0, PAPERSIZE_METRIC },	/*  610mm x  860mm */
  { "w1218h1729",	N_ ("RA2"),
    1218, 1729, 0, 0, 0, 0, PAPERSIZE_METRIC },	/*  430mm x  610mm */
  { "w864h1218",	N_ ("RA3"),
    864, 1218, 0, 0, 0, 0, PAPERSIZE_METRIC },	/*  305mm x  430mm */
  { "w609h864",		N_ ("RA4"),
    609,  864, 0, 0, 0, 0, PAPERSIZE_METRIC },	/*  215mm x  305mm */

  /*
   * Stock sizes for bled work or extra trims.
   */
  { "w2551h3628",	N_ ("SRA0"),
    2551, 3628, 0, 0, 0, 0, PAPERSIZE_METRIC },	/*  900mm x 1280mm */
  { "w1814h2551",	N_ ("SRA1"),
    1814, 2551, 0, 0, 0, 0, PAPERSIZE_METRIC },	/*  640mm x  900mm */
  { "w1275h1814",	N_ ("SRA2"),
    1275, 1814, 0, 0, 0, 0, PAPERSIZE_METRIC },	/*  450mm x  640mm */
  { "w907h1275",	N_ ("SRA3"),
    907, 1275, 0, 0, 0, 0, PAPERSIZE_METRIC },	/*  320mm x  450mm */
  { "w637h907",		N_ ("SRA4"),
    637,  907, 0, 0, 0, 0, PAPERSIZE_METRIC },	/*  225mm x  320mm */

  /*
   * "B" series: Posters, wall charts and similar items.
   */
  { "w5669h8016",	N_ ("4B ISO"),
    5669, 8016, 0, 0, 0, 0, PAPERSIZE_METRIC },	/* 2000mm x 2828mm */
  { "w4008h5669",	N_ ("2B ISO"),
    4008, 5669, 0, 0, 0, 0, PAPERSIZE_METRIC },	/* 1414mm x 2000mm */
  { "ISOB0",		N_ ("B0 ISO"),
    2834, 4008, 0, 0, 0, 0, PAPERSIZE_METRIC },	/* 1000mm x 1414mm */
  { "ISOB1",		N_ ("B1 ISO"),
    2004, 2834, 0, 0, 0, 0, PAPERSIZE_METRIC },	/*  707mm x 1000mm */
  { "ISOB2",		N_ ("B2 ISO"),
    1417, 2004, 0, 0, 0, 0, PAPERSIZE_METRIC },	/*  500mm x  707mm */
  { "ISOB3",		N_ ("B3 ISO"),
    1000, 1417, 0, 0, 0, 0, PAPERSIZE_METRIC },	/*  353mm x  500mm */
  { "ISOB4",		N_ ("B4 ISO"),
    708, 1000, 0, 0, 0, 0, PAPERSIZE_METRIC },	/*  250mm x  353mm */
  { "ISOB5",		N_ ("B5 ISO"),
    498,  708, 0, 0, 0, 0, PAPERSIZE_METRIC },	/*  176mm x  250mm */
  { "ISOB6",		N_ ("B6 ISO"),
    354,  498, 0, 0, 0, 0, PAPERSIZE_METRIC },	/*  125mm x  176mm */
  { "ISOB7",		N_ ("B7 ISO"),
    249,  354, 0, 0, 0, 0, PAPERSIZE_METRIC },	/*   88mm x  125mm */
  { "ISOB8",		N_ ("B8 ISO"),
    175,  249, 0, 0, 0, 0, PAPERSIZE_METRIC },	/*   62mm x   88mm */
  { "ISOB9",		N_ ("B9 ISO"),
    124,  175, 0, 0, 0, 0, PAPERSIZE_METRIC },	/*   44mm x   62mm */
  { "ISOB10",		N_ ("B10 ISO"),
    87,  124, 0, 0, 0, 0, PAPERSIZE_METRIC },	/*   31mm x   44mm */

  { "B0",		N_ ("B0 JIS"),
    2919, 4127, 0, 0, 0, 0, PAPERSIZE_METRIC },
  { "B1",		N_ ("B1 JIS"),
    2063, 2919, 0, 0, 0, 0, PAPERSIZE_METRIC },
  { "B2",		N_ ("B2 JIS"),
    1459, 2063, 0, 0, 0, 0, PAPERSIZE_METRIC },
  { "B3",		N_ ("B3 JIS"),
    1029, 1459, 0, 0, 0, 0, PAPERSIZE_METRIC },
  { "B4",		N_ ("B4 JIS"),
    727, 1029, 0, 0, 0, 0, PAPERSIZE_METRIC },
  { "B5",		N_ ("B5 JIS"),
    518,  727, 0, 0, 0, 0, PAPERSIZE_METRIC },
  { "B6",		N_ ("B6 JIS"),
    362,  518, 0, 0, 0, 0, PAPERSIZE_METRIC },
  { "B7",		N_ ("B7 JIS"),
    257,  362, 0, 0, 0, 0, PAPERSIZE_METRIC },
  { "B8",		N_ ("B8 JIS"),
    180,  257, 0, 0, 0, 0, PAPERSIZE_METRIC },
  { "B9",		N_ ("B9 JIS"),
    127,  180, 0, 0, 0, 0, PAPERSIZE_METRIC },
  { "B10",		N_ ("B10 JIS"),
    90,  127, 0, 0, 0, 0, PAPERSIZE_METRIC },

  /*
   * "C" series: Envelopes or folders suitable for A size stationery.
   */
  { "C0",		N_ ("C0"),
    2599, 3676, 0, 0, 0, 0, PAPERSIZE_METRIC },	/*  917mm x 1297mm */
  { "C1",		N_ ("C1"),
    1836, 2599, 0, 0, 0, 0, PAPERSIZE_METRIC },	/*  648mm x  917mm */
  { "C2",		N_ ("C2"),
    1298, 1836, 0, 0, 0, 0, PAPERSIZE_METRIC },	/*  458mm x  648mm */
  { "C3",		N_ ("C3"),
    918, 1298, 0, 0, 0, 0, PAPERSIZE_METRIC },	/*  324mm x  458mm */
  { "C4",		N_ ("C4"),
    649,  918, 0, 0, 0, 0, PAPERSIZE_METRIC },	/*  229mm x  324mm */
  { "C5",		N_ ("C5"),
    459,  649, 0, 0, 0, 0, PAPERSIZE_METRIC },	/*  162mm x  229mm */
  { "w354h918",		N_ ("B6-C4"),
    354,  918, 0, 0, 0, 0, PAPERSIZE_METRIC },	/*  125mm x  324mm */
  { "C6",		N_ ("C6"),
    323,  459, 0, 0, 0, 0, PAPERSIZE_METRIC },	/*  114mm x  162mm */
  { "DL",		N_ ("DL"),
    311,  623, 0, 0, 0, 0, PAPERSIZE_METRIC },	/*  110mm x  220mm */
  { "w229h459",		N_ ("C7-6"),
    229,  459, 0, 0, 0, 0, PAPERSIZE_METRIC },	/*   81mm x  162mm */
  { "C7",		N_ ("C7"),
    229,  323, 0, 0, 0, 0, PAPERSIZE_METRIC },	/*   81mm x  114mm */
  { "C8",		N_ ("C8"),
    161,  229, 0, 0, 0, 0, PAPERSIZE_METRIC },	/*   57mm x   81mm */
  { "C9",		N_ ("C9"),
    113,  161, 0, 0, 0, 0, PAPERSIZE_METRIC },	/*   40mm x   57mm */
  { "C10",		N_ ("C10"),
    79,  113, 0, 0, 0, 0, PAPERSIZE_METRIC },	/*   28mm x   40mm */

  /*
   * US CAD standard paper sizes
   */
  { "ARCHA",		N_ ("ArchA"),
    648,  864, 0, 0, 0, 0, PAPERSIZE_ENGLISH },
  { "ARCHB",		N_ ("ArchB"),
    864, 1296, 0, 0, 0, 0, PAPERSIZE_ENGLISH },
  { "ARCHC",		N_ ("ArchC"),
    1296, 1728, 0, 0, 0, 0, PAPERSIZE_ENGLISH },
  { "ARCHD",		N_ ("ArchD"),
    1728, 2592, 0, 0, 0, 0, PAPERSIZE_ENGLISH },
  { "ARCHE",		N_ ("ArchE"),
    2592, 3456, 0, 0, 0, 0, PAPERSIZE_ENGLISH },

  /*
   * Foolscap
   */
  { "w612h936",		N_ ("American foolscap"),
    612,  936, 0, 0, 0, 0, PAPERSIZE_ENGLISH }, /* American foolscap */
  { "w648h936",		N_ ("European foolscap"),
    648,  936, 0, 0, 0, 0, PAPERSIZE_ENGLISH }, /* European foolscap */

  /*
   * Sizes for book production
   * The BPIF and the Publishers Association jointly recommend ten
   * standard metric sizes for case-bound titles as follows:
   */
  { "w535h697",		N_ ("Crown Quarto"),
    535,  697, 0, 0, 0, 0, PAPERSIZE_METRIC }, /* 189mm x 246mm */
  { "w569h731",		N_ ("Large Crown Quarto"),
    569,  731, 0, 0, 0, 0, PAPERSIZE_METRIC }, /* 201mm x 258mm */
  { "w620h782",		N_ ("Demy Quarto"),
    620,  782, 0, 0, 0, 0, PAPERSIZE_METRIC }, /* 219mm x 276mm */
  { "w671h884",		N_ ("Royal Quarto"),
    671,  884, 0, 0, 0, 0, PAPERSIZE_METRIC }, /* 237mm x 312mm */
  /*{ "ISO A4",             595,
    841, PAPERSIZE_METRIC, 0, 0, 0, 0 },    210mm x 297mm */
  { "w348h527",		N_ ("Crown Octavo"),
    348,  527, 0, 0, 0, 0, PAPERSIZE_METRIC }, /* 123mm x 186mm */
  { "w365h561",		N_ ("Large Crown Octavo"),
    365,  561, 0, 0, 0, 0, PAPERSIZE_METRIC }, /* 129mm x 198mm */
  { "w391h612",		N_ ("Demy Octavo"),
    391,  612, 0, 0, 0, 0, PAPERSIZE_METRIC }, /* 138mm x 216mm */
  { "w442h663",		N_ ("Royal Octavo"),
    442,  663, 0, 0, 0, 0, PAPERSIZE_METRIC }, /* 156mm x 234mm */
  /*{ N_ ("ISO A5"),             419,
    595, 0, 0, 0, 0, PAPERSIZE_METRIC },    148mm x 210mm */

  /* Paperback sizes in common usage */
  { "w314h504",		N_ ("Small paperback"),
    314, 504, 0, 0, 0, 0, PAPERSIZE_METRIC }, /* 111mm x 178mm */
  { "w314h513",		N_ ("Penguin small paperback"),
    314, 513, 0, 0, 0, 0, PAPERSIZE_METRIC }, /* 111mm x 181mm */
  { "w365h561",		N_ ("Penguin large paperback"),
    365, 561, 0, 0, 0, 0, PAPERSIZE_METRIC }, /* 129mm x 198mm */

  /* Miscellaneous sizes */
  { "w283h420",		N_ ("Hagaki Card"),
    283, 420, 0, 0, 0, 0, PAPERSIZE_METRIC }, /* 100 x 148 mm */
  { "w420h567",		N_ ("Oufuku Card"),
    420, 567, 0, 0, 0, 0, PAPERSIZE_METRIC }, /* 148 x 200 mm */
  { "w340h666",		N_ ("Japanese long envelope #3"),
    340, 666, 0, 0, 0, 0, PAPERSIZE_METRIC }, /* Japanese long envelope #3 */
  { "w255h581",		N_ ("Japanese long envelope #4"),
    255, 581, 0, 0, 0, 0, PAPERSIZE_METRIC }, /* Japanese long envelope #4 */
  { "w680h941",		N_ ("Japanese Kaku envelope #4"),
    680, 941, 0, 0, 0, 0, PAPERSIZE_METRIC }, /* Japanese Kaku envelope #4 */
  { "COM10",		N_ ("Commercial 10"),
    297, 684, 0, 0, 0, 0, PAPERSIZE_ENGLISH }, /* US Commercial 10 env */
  { "w315h414",		N_ ("A2 Invitation"),
    315, 414, 0, 0, 0, 0, PAPERSIZE_ENGLISH }, /* US A2 invitation */
  { "Custom",		N_ ("Custom"),
    0, 0, 0, 0, 0, 0, PAPERSIZE_ENGLISH },

  { "w252",		N_ ("89 mm Roll Paper"),
    252, 0, 0, 0, 0, 0, PAPERSIZE_METRIC },
  { "w288",		N_ ("4 Inch Roll Paper"),
    288, 0, 0, 0, 0, 0, PAPERSIZE_ENGLISH },
  { "w360",		N_ ("5 Inch Roll Paper"),
    360, 0, 0, 0, 0, 0, PAPERSIZE_ENGLISH },
  { "w595",		N_ ("210 mm Roll Paper"),
    595, 0, 0, 0, 0, 0, PAPERSIZE_METRIC },
  { "w936",		N_ ("13 Inch Roll Paper"),
    936, 0, 0, 0, 0, 0, PAPERSIZE_ENGLISH },
  { "w1584",		N_ ("22 Inch Roll Paper"),
    1584, 0, 0, 0, 0, 0, PAPERSIZE_ENGLISH },
  { "w1728",		N_ ("24 Inch Roll Paper"),
    1728, 0, 0, 0, 0, 0, PAPERSIZE_ENGLISH },
  { "w2592",		N_ ("36 Inch Roll Paper"),
    2592, 0, 0, 0, 0, 0, PAPERSIZE_ENGLISH },
  { "w3168",		N_ ("44 Inch Roll Paper"),
    3168, 0, 0, 0, 0, 0, PAPERSIZE_ENGLISH },

  { "",           "",    0, 0, 0, 0, 0, PAPERSIZE_METRIC }
};

int
stp_known_papersizes(void)
{
  return sizeof(paper_sizes) / sizeof(stp_internal_papersize_t) - 1;
}

const char *
stp_papersize_get_name(const stp_papersize_t pt)
{
  const stp_internal_papersize_t *p = (const stp_internal_papersize_t *) pt;
  return p->name;
}

const char *
stp_papersize_get_text(const stp_papersize_t pt)
{
  const stp_internal_papersize_t *p = (const stp_internal_papersize_t *) pt;
  return _(p->text);
}

unsigned
stp_papersize_get_width(const stp_papersize_t pt)
{
  const stp_internal_papersize_t *p = (const stp_internal_papersize_t *) pt;
  return p->width;
}

unsigned
stp_papersize_get_height(const stp_papersize_t pt)
{
  const stp_internal_papersize_t *p = (const stp_internal_papersize_t *) pt;
  return p->height;
}

unsigned
stp_papersize_get_top(const stp_papersize_t pt)
{
  const stp_internal_papersize_t *p = (const stp_internal_papersize_t *) pt;
  return p->top;
}

unsigned
stp_papersize_get_left(const stp_papersize_t pt)
{
  const stp_internal_papersize_t *p = (const stp_internal_papersize_t *) pt;
  return p->left;
}

unsigned
stp_papersize_get_bottom(const stp_papersize_t pt)
{
  const stp_internal_papersize_t *p = (const stp_internal_papersize_t *) pt;
  return p->bottom;
}

unsigned
stp_papersize_get_right(const stp_papersize_t pt)
{
  const stp_internal_papersize_t *p = (const stp_internal_papersize_t *) pt;
  return p->right;
}

stp_papersize_unit_t
stp_papersize_get_unit(const stp_papersize_t pt)
{
  const stp_internal_papersize_t *p = (const stp_internal_papersize_t *) pt;
  return p->paper_unit;
}

#if 1
/*
 * This is, of course, blatantly thread-unsafe.  However, it certainly
 * speeds up genppd by a lot!
 */
const stp_papersize_t
stp_get_papersize_by_name(const char *name)
{
  static int last_used_papersize = 0;
  int base = last_used_papersize;
  int sizes = stp_known_papersizes();
  int i;
  for (i = 0; i < sizes; i++)
    {
      int size_to_try = (i + base) % sizes;
      const stp_internal_papersize_t *val = &(paper_sizes[size_to_try]);
      if (!strcmp(val->name, name))
	{
	  last_used_papersize = size_to_try;
	  return (const stp_papersize_t) val;
	}
    }
  return NULL;
}
#else
const stp_papersize_t
stp_get_papersize_by_name(const char *name)
{
  const stp_internal_papersize_t *val = &(paper_sizes[0]);
  while (strlen(val->name) > 0)
    {
      if (!strcmp(val->name, name))
	return (stp_papersize_t) val;
      val++;
    }
  return NULL;
}
#endif

const stp_papersize_t
stp_get_papersize_by_index(int index)
{
  if (index < 0 || index >= stp_known_papersizes())
    return NULL;
  else
    return (stp_papersize_t) &(paper_sizes[index]);
}

static int
paper_size_mismatch(int l, int w, const stp_internal_papersize_t *val)
{
  int hdiff = abs(l - (int) val->height);
  int vdiff = abs(w - (int) val->width);
  return hdiff + vdiff;
}

const stp_papersize_t
stp_get_papersize_by_size(int l, int w)
{
  int score = INT_MAX;
  const stp_internal_papersize_t *ref = NULL;
  const stp_internal_papersize_t *val = &(paper_sizes[0]);
  int sizes = stp_known_papersizes();
  int i;
  for (i = 0; i < sizes; i++)
    {
      if (val->width == w && val->height == l)
	return (stp_papersize_t) val;
      else
	{
	  int myscore = paper_size_mismatch(l, w, val);
	  if (myscore < score && myscore < 20)
	    {
	      ref = val;
	      score = myscore;
	    }
	}
      val++;
    }
  return (stp_papersize_t) ref;
}

void
stp_default_media_size(const stp_printer_t printer,
					/* I - Printer model (not used) */
		   const stp_vars_t v,	/* I */
        	   int  *width,		/* O - Width in points */
        	   int  *height)	/* O - Height in points */
{
  if (stp_get_page_width(v) > 0 && stp_get_page_height(v) > 0)
    {
      *width = stp_get_page_width(v);
      *height = stp_get_page_height(v);
    }
  else
    {
      const stp_papersize_t papersize =
	stp_get_papersize_by_name(stp_get_media_size(v));
      if (!papersize)
	{
	  *width = 1;
	  *height = 1;
	}
      else
	{
	  *width = stp_papersize_get_width(papersize);
	  *height = stp_papersize_get_height(papersize);
	}
      if (*width == 0)
	*width = 612;
      if (*height == 0)
	*height = 792;
    }
}

/*
 * The list of printers has been moved to printers.c
 */
#include "print-printers.c"

int
stp_known_printers(void)
{
  return printer_count;
}

const stp_printer_t
stp_get_printer_by_index(int idx)
{
  if (idx < 0 || idx >= printer_count)
    return NULL;
  return (stp_printer_t) &(printers[idx]);
}

const stp_printer_t
stp_get_printer_by_long_name(const char *long_name)
{
  const stp_internal_printer_t *val = &(printers[0]);
  int i;
  for (i = 0; i < stp_known_printers(); i++)
    {
      if (!strcmp(val->long_name, long_name))
	return (stp_printer_t) val;
      val++;
    }
  return NULL;
}

const stp_printer_t
stp_get_printer_by_driver(const char *driver)
{
  const stp_internal_printer_t *val = &(printers[0]);
  int i;
  for (i = 0; i < stp_known_printers(); i++)
    {
      if (!strcmp(val->driver, driver))
	return (stp_printer_t) val;
      val++;
    }
  return NULL;
}

int
stp_get_printer_index_by_driver(const char *driver)
{
  int idx = 0;
  const stp_internal_printer_t *val = &(printers[0]);
  for (idx = 0; idx < stp_known_printers(); idx++)
    {
      if (!strcmp(val->driver, driver))
	return idx;
      val++;
    }
  return -1;
}

const char *
stp_printer_get_long_name(const stp_printer_t p)
{
  const stp_internal_printer_t *val = (const stp_internal_printer_t *) p;
  return val->long_name;
}

const char *
stp_printer_get_driver(const stp_printer_t p)
{
  const stp_internal_printer_t *val = (const stp_internal_printer_t *) p;
  return val->driver;
}

int
stp_printer_get_model(const stp_printer_t p)
{
  const stp_internal_printer_t *val = (const stp_internal_printer_t *) p;
  return val->model;
}

const stp_printfuncs_t *
stp_printer_get_printfuncs(const stp_printer_t p)
{
  const stp_internal_printer_t *val = (const stp_internal_printer_t *) p;
  return val->printfuncs;
}

const stp_vars_t
stp_printer_get_printvars(const stp_printer_t p)
{
  const stp_internal_printer_t *val = (const stp_internal_printer_t *) p;
  return (stp_vars_t) &(val->printvars);
}

const char *
stp_default_dither_algorithm(void)
{
  return stp_dither_algorithm_name(0);
}

void
stp_compute_page_parameters(int page_right,	/* I */
			    int page_left, /* I */
			    int page_top, /* I */
			    int page_bottom, /* I */
			    double scaling, /* I */
			    int image_width, /* I */
			    int image_height, /* I */
			    stp_image_t *image, /* IO */
			    int *orientation, /* IO */
			    int *page_width, /* O */
			    int *page_height, /* O */
			    int *out_width,	/* O */
			    int *out_height, /* O */
			    int *left, /* O */
			    int *top) /* O */
{
  *page_width  = page_right - page_left;
  *page_height = page_top - page_bottom;

  /* In AUTO orientation, just orient the paper the same way as the image. */

  if (*orientation == ORIENT_AUTO)
    {
      if ((*page_width >= *page_height && image_width >= image_height)
         || (*page_height >= *page_width && image_height >= image_width))
        *orientation = ORIENT_PORTRAIT;
      else
        *orientation = ORIENT_LANDSCAPE;
    }

  if (*orientation == ORIENT_LANDSCAPE)
      image->rotate_ccw(image);
  else if (*orientation == ORIENT_UPSIDEDOWN)
      image->rotate_180(image);
  else if (*orientation == ORIENT_SEASCAPE)
      image->rotate_cw(image);

  image_width  = image->width(image);
  image_height = image->height(image);

  /*
   * Calculate width/height...
   */

  if (scaling == 0.0)
    {
      *out_width  = *page_width;
      *out_height = *page_height;
    }
  else if (scaling < 0.0)
    {
      /*
       * Scale to pixels per inch...
       */

      *out_width  = image_width * -72.0 / scaling;
      *out_height = image_height * -72.0 / scaling;
    }
  else
    {
      /*
       * Scale by percent...
       */

      /*
       * Decide which orientation gives the proper fit
       * If we ask for 50%, we do not want to exceed that
       * in either dimension!
       */

      int twidth0 = *page_width * scaling / 100.0;
      int theight0 = twidth0 * image_height / image_width;
      int theight1 = *page_height * scaling / 100.0;
      int twidth1 = theight1 * image_width / image_height;

      *out_width = FMIN(twidth0, twidth1);
      *out_height = FMIN(theight0, theight1);
    }

  if (*out_width == 0)
    *out_width = 1;
  if (*out_height == 0)
    *out_height = 1;

  /*
   * Adjust offsets depending on the page orientation...
   */

  if (*orientation == ORIENT_LANDSCAPE || *orientation == ORIENT_SEASCAPE)
    {
      int x;

      x     = *left;
      *left = *top;
      *top  = x;
    }

  if ((*orientation == ORIENT_UPSIDEDOWN || *orientation == ORIENT_SEASCAPE)
      && *left >= 0)
    {
      *left = *page_width - *left - *out_width;
      if (*left < 0)
	*left = 0;
    }

  if ((*orientation == ORIENT_UPSIDEDOWN || *orientation == ORIENT_LANDSCAPE)
      && *top >= 0)
    {
      *top = *page_height - *top - *out_height;
      if (*top < 0)
	*top = 0;
    }

  if (*left < 0)
    *left = (*page_width - *out_width) / 2;

  if (*top < 0)
    *top  = (*page_height - *out_height) / 2;
}

void
stp_set_printer_defaults(stp_vars_t v, const stp_printer_t p,
			 const char *ppd_file)
{
  const stp_printfuncs_t *printfuncs = stp_printer_get_printfuncs(p);
  stp_set_resolution(v, ((printfuncs->default_parameters)
			 (p, ppd_file, "Resolution")));
  stp_set_ink_type(v, ((printfuncs->default_parameters)
		       (p, ppd_file, "InkType")));
  stp_set_media_type(v, ((printfuncs->default_parameters)
			 (p, ppd_file, "MediaType")));
  stp_set_media_source(v, ((printfuncs->default_parameters)
			   (p, ppd_file, "InputSlot")));
  stp_set_media_size(v, ((printfuncs->default_parameters)
			 (p, ppd_file, "PageSize")));
  stp_set_dither_algorithm(v, stp_default_dither_algorithm());
  stp_set_driver(v, stp_printer_get_driver(p));
}

static int
verify_param(const char *checkval, stp_param_t *vptr,
	     int count, const char *what, const stp_vars_t v)
{
  int answer = 0;
  int i;
  if (count > 0)
    {
      for (i = 0; i < count; i++)
	if (!strcmp(checkval, vptr[i].name))
	  {
	    answer = 1;
	    break;
	  }
      if (!answer)
	stp_eprintf(v, "%s is not a valid %s\n", checkval, what);
      for (i = 0; i < count; i++)
	{
	  stp_free((void *)vptr[i].name);
	  stp_free((void *)vptr[i].text);
	}
    }
  else
    stp_eprintf(v, "%s is not a valid %s\n", checkval, what);
  if (vptr)
    free(vptr);
  return answer;
}

int
stp_verify_printer_params(const stp_printer_t p, const stp_vars_t v)
{
  stp_param_t *vptr;
  int count;
  int i;
  int answer = 1;
  const stp_printfuncs_t *printfuncs = stp_printer_get_printfuncs(p);
  const stp_vars_t printvars = stp_printer_get_printvars(p);
  const char *ppd_file = stp_get_ppd_file(v);

  /*
   * Note that in raw CMYK mode the user is responsible for not sending
   * color output to black & white printers!
   */
  if (stp_get_output_type(printvars) == OUTPUT_GRAY &&
      (stp_get_output_type(v) == OUTPUT_COLOR ||
       stp_get_output_type(v) == OUTPUT_RAW_CMYK))
    {
      answer = 0;
      stp_eprintf(v, "Printer does not support color output\n");
    }
  if (strlen(stp_get_media_size(v)) > 0)
    {
      const char *checkval = stp_get_media_size(v);
      vptr = (*printfuncs->parameters)(p, ppd_file, "PageSize", &count);
      answer &= verify_param(checkval, vptr, count, "page size", v);
    }
  else
    {
      int height, width;
      int min_height, min_width;
      (*printfuncs->limit)(p, v, &width, &height, &min_width, &min_height);
      if (stp_get_page_height(v) <= min_height ||
	  stp_get_page_height(v) > height ||
	  stp_get_page_width(v) <= min_width || stp_get_page_width(v) > width)
	{
	  answer = 0;
	  stp_eprintf(v, "Image size is not valid\n");
	}
    }

  if (strlen(stp_get_media_type(v)) > 0)
    {
      const char *checkval = stp_get_media_type(v);
      vptr = (*printfuncs->parameters)(p, ppd_file, "MediaType", &count);
      answer &= verify_param(checkval, vptr, count, "media type", v);
    }

  if (strlen(stp_get_media_source(v)) > 0)
    {
      const char *checkval = stp_get_media_source(v);
      vptr = (*printfuncs->parameters)(p, ppd_file, "InputSlot", &count);
      answer &= verify_param(checkval, vptr, count, "media source", v);
    }

  if (strlen(stp_get_resolution(v)) > 0)
    {
      const char *checkval = stp_get_resolution(v);
      vptr = (*printfuncs->parameters)(p, ppd_file, "Resolution", &count);
      answer &= verify_param(checkval, vptr, count, "resolution", v);
    }

  if (strlen(stp_get_ink_type(v)) > 0)
    {
      const char *checkval = stp_get_ink_type(v);
      vptr = (*printfuncs->parameters)(p, ppd_file, "InkType", &count);
      answer &= verify_param(checkval, vptr, count, "ink type", v);
    }

  for (i = 0; i < stp_dither_algorithm_count(); i++)
    if (!strcmp(stp_get_dither_algorithm(v), stp_dither_algorithm_name(i)))
      {
	stp_set_verified(v, answer);
	return answer;
      }

  stp_eprintf(v, "%s is not a valid dither algorithm\n",
	      stp_get_dither_algorithm(v));
  stp_set_verified(v, 0);
  return 0;
}

const stp_vars_t
stp_default_settings()
{
  return (stp_vars_t) &default_vars;
}

const stp_vars_t
stp_maximum_settings()
{
  return (stp_vars_t) &max_vars;
}

const stp_vars_t
stp_minimum_settings()
{
  return (stp_vars_t) &min_vars;
}

#if defined DISABLE_NLS || !defined HAVE_VASPRINTF
#include <stdarg.h>

static int vasprintf (char **result, const char *format, va_list args);
static int int_vasprintf (char **result, const char *format, va_list *args);

static int
int_vasprintf (char **result, const char *format, va_list *args)
{
  const char *p = format;
  /* Add one to make sure that it is never zero, which might cause malloc
     to return NULL.  */
  int total_width = strlen (format) + 1;
  va_list ap;

  memcpy (&ap, args, sizeof (va_list));

  while (*p != '\0')
    {
      if (*p++ == '%')
	{
	  while (strchr ("-+ #0", *p))
	    ++p;
	  if (*p == '*')
	    {
	      ++p;
	      total_width += abs (va_arg (ap, int));
	    }
	  else
	    total_width += strtoul (p, (char **) &p, 10);
	  if (*p == '.')
	    {
	      ++p;
	      if (*p == '*')
		{
		  ++p;
		  total_width += abs (va_arg (ap, int));
		}
	      else
		total_width += strtoul (p, (char **) &p, 10);
	    }
	  while (strchr ("hlL", *p))
	    ++p;
	  /* Should be big enough for any format specifier except %s.  */
	  total_width += 30;
	  switch (*p)
	    {
	    case 'd':
	    case 'i':
	    case 'o':
	    case 'u':
	    case 'x':
	    case 'X':
	    case 'c':
	      (void) va_arg (ap, int);
	      break;
	    case 'f':
	    case 'e':
	    case 'E':
	    case 'g':
	    case 'G':
	      (void) va_arg (ap, double);
	      break;
	    case 's':
	      total_width += strlen (va_arg (ap, char *));
	      break;
	    case 'p':
	    case 'n':
	      (void) va_arg (ap, char *);
	      break;
	    }
	}
    }
#ifdef TEST
  global_total_width = total_width;
#endif
  *result = malloc (total_width);
  if (*result != NULL)
    return vsprintf (*result, format, *args);
  else
    return 0;
}

static int
vasprintf (char **result, const char *format, va_list args)
{
  return int_vasprintf (result, format, &args);
}
#else
extern int vasprintf (char **result, const char *format, va_list args);
#endif

void
stp_zprintf(const stp_vars_t v, const char *format, ...)
{
  va_list args;
  int bytes;
  char *result;
  va_start(args, format);
  bytes = vasprintf(&result, format, args);
  va_end(args);
  (stp_get_outfunc(v))((void *)(stp_get_outdata(v)), result, bytes);
  free(result);
}

void
stp_zfwrite(const char *buf, size_t bytes, size_t nitems, const stp_vars_t v)
{
  (stp_get_outfunc(v))((void *)(stp_get_outdata(v)), buf, bytes * nitems);
}

void
stp_putc(int ch, const stp_vars_t v)
{
  char a = (char) ch;
  (stp_get_outfunc(v))((void *)(stp_get_outdata(v)), &a, 1);
}

void
stp_puts(const char *s, const stp_vars_t v)
{
  (stp_get_outfunc(v))((void *)(stp_get_outdata(v)), s, strlen(s));
}

void
stp_eprintf(const stp_vars_t v, const char *format, ...)
{
  va_list args;
  int bytes;
  char *result;
  if (stp_get_errfunc(v))
    {
      va_start(args, format);
      bytes = vasprintf(&result, format, args);
      va_end(args);
      (stp_get_errfunc(v))((void *)(stp_get_errdata(v)), result, bytes);
      free(result);
    }
}

void
stp_erputc(int ch)
{
  putc(ch, stderr);
}

void
stp_erprintf(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
}

static unsigned long stp_debug_level = 0;

static void
init_stp_debug(void)
{
  static int debug_initialized = 0;
  if (!debug_initialized)
    {
      const char *dval = getenv("STP_DEBUG");
      debug_initialized = 1;
      if (dval)
	{
	  stp_debug_level = strtoul(dval, 0, 0);
	  stp_erprintf("Gimp-Print %s %s\n", VERSION, RELEASE_DATE);
	}
    }
}

void
stp_dprintf(unsigned long level, const stp_vars_t v, const char *format, ...)
{
  va_list args;
  int bytes;
  char *result;
  init_stp_debug();
  if ((level & stp_debug_level) && stp_get_errfunc(v))
    {
      va_start(args, format);
      bytes = vasprintf(&result, format, args);
      va_end(args);
      (stp_get_errfunc(v))((void *)(stp_get_errdata(v)), result, bytes);
      free(result);
    }
}

void
stp_deprintf(unsigned long level, const char *format, ...)
{
  va_list args;
  int bytes;
  char *result;
  init_stp_debug();
  if (level & stp_debug_level)
    {
      va_start(args, format);
      bytes = vasprintf(&result, format, args);
      va_end(args);
      stp_erprintf("%s", result);
      free(result);
    }
}

void *
stp_malloc (size_t size)
{
  register void *memptr = NULL;

  if ((memptr = malloc (size)) == NULL)
    {
      fputs("Virtual memory exhausted.\n", stderr);
      exit (EXIT_FAILURE);
    }
  return (memptr);
}

void
stp_free(void *ptr)
{
  free(ptr);
}

int
stp_init(void)
{
  static int stp_is_initialised = 0;
  if (!stp_is_initialised)
    {
      /* Things that are only initialised once */
      /* Set up gettext */
#ifdef ENABLE_NLS
      setlocale (LC_ALL, "");
      bindtextdomain (PACKAGE, PACKAGE_LOCALE_DIR);
#endif
      init_stp_debug();
    }
  stp_is_initialised = 1;
  return (0);
}

#ifdef QUANTIFY
unsigned quantify_counts[NUM_QUANTIFY_BUCKETS] = {0};
struct timeval quantify_buckets[NUM_QUANTIFY_BUCKETS] = {{0,0}};
int quantify_high_index = 0;
int quantify_first_time = 1;
struct timeval quantify_cur_time;
struct timeval quantify_prev_time;

void print_timers(const stp_vars_t v)
{
  int i;

  stp_eprintf(v, "%s", "Quantify timers:\n");
  for (i = 0; i <= quantify_high_index; i++)
    {
      if (quantify_counts[i] > 0)
	{
	  stp_eprintf(v,
		      "Bucket %d:\t%ld.%ld s\thit %u times\n", i,
		      quantify_buckets[i].tv_sec, quantify_buckets[i].tv_usec,
		      quantify_counts[i]);
	  quantify_buckets[i].tv_sec = 0;
	  quantify_buckets[i].tv_usec = 0;
	  quantify_counts[i] = 0;
	}
    }
}
#endif
