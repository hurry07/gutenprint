/*
 * "$Id: print-escp2.c,v 1.147.2.7 2002/05/03 01:30:28 rlk Exp $"
 *
 *   Print plug-in EPSON ESC/P2 driver for the GIMP.
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
#include <string.h>
#include "print-escp2.h"

#ifdef __GNUC__
#define inline __inline__
#endif

#ifdef TEST_UNCOMPRESSED
#define COMPRESSION (0)
#define FILLFUNC stp_fill_uncompressed
#define COMPUTEFUNC stp_compute_uncompressed_linewidth
#define PACKFUNC stp_pack_uncompressed
#else
#define COMPRESSION (1)
#define FILLFUNC stp_fill_tiff
#define COMPUTEFUNC stp_compute_tiff_linewidth
#define PACKFUNC stp_pack_tiff
#endif

static void flush_pass(stp_softweave_t *sw, int passno, int model, int width,
		       int hoffset, int ydpi, int xdpi, int physical_xdpi,
		       int vertical_subpass);

static const int dotidmap[] =
{ 0, 1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 10, 11, 12, 12 };

static int
resid2dotid(int resid)
{
  if (resid < 0 || resid >= RES_N)
    return -1;
  return dotidmap[resid];
}

static const int densidmap[] =
{ 0, 1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 10, 11, 12, 12 };

static int
resid2densid(int resid)
{
  if (resid < 0 || resid >= RES_N)
    return -1;
  return densidmap[resid];
}

static int
bits2inktype(int bits)
{
  if (bits == 1)
    return INKTYPE_SINGLE;
  else
    return INKTYPE_VARIABLE;
}

static int
colors2inkset(int colors)
{
  switch (colors)
    {
    case 1:
    case 2:
    case 3:
    case 4:
      return INKSET_4;
    case 5:
    case 6:
      return INKSET_6;
    case 7:
      return INKSET_7;
    default:
      return -1;
    }
}

/*
 * Mapping between color and linear index.  The colors are
 * black, magenta, cyan, yellow, light magenta, light cyan
 */

static const int color_indices[16] = { 0, 1, 2, -1,
				       3, -1, -1, -1,
				       -1, 4, 5, -1,
				       6, -1, -1, -1 };
static const int colors[7] = { 0, 1, 2, 4, 1, 2, 4};
static const int densities[7] = { 0, 0, 0, 0, 1, 1, 1 };

static inline int
get_color_by_params(int plane, int density)
{
  if (plane > 4 || plane < 0 || density > 1 || density < 0)
    return -1;
  return color_indices[density * 8 + plane];
}

static const escp2_printer_attr_t escp2_printer_attrs[] =
{
  { "init_sequence",	 	0, 4 },
  { "has_black",	 	4, 1 },
  { "color",		 	5, 2 },
  { "graymode",		 	7, 1 },
  { "720dpi_mode",	 	8, 2 },
  { "variable_mode",		10, 2 },
  { "command_mode",		12, 4 },
  { "ink_types",		16, 1 },
  { "rollfeed",			17, 1 },
  { "horizontal_zero_margin",	18, 1 },
  { "vertical_zero_margin",	19, 1 },
  { "microweave",		20, 3 },
  { "vacuum",			23, 1 },
  { "microweave_exception",	24, 2 },
  { "deinitialize-je",          26, 1 },
};

#define INCH(x)		(72 * x)

static const res_t *escp2_find_resolution(int model, const stp_vars_t v,
					  const char *resolution);

typedef struct
{
  int undersample;
  int initial_vertical_offset;
  int min_nozzles;
  int printed_something;
} escp2_privdata_t;

typedef struct escp2_init
{
  int model;
  int output_type;
  int ydpi;
  int xdpi;
  int use_softweave;
  int use_microweave;
  int page_height;
  int page_width;
  int page_top;
  int page_bottom;
  int nozzles;
  int nozzle_separation;
  int horizontal_passes;
  int vertical_passes;
  int vertical_oversample;
  int bits;
  int unidirectional;
  int resid;
  int initial_vertical_offset;
  int ncolors;
  const char *paper_type;
  const char *media_source;
  stp_vars_t v;
} escp2_init_t;


static int
escp2_has_cap(int model, int feature,
	      model_featureset_t class, const stp_vars_t v)
{
  if (feature < 0 || feature >= MODEL_LIMIT)
    return -1;
  else
    {
      model_featureset_t featureset =
	(((1ul << escp2_printer_attrs[feature].bits) - 1ul) <<
	 escp2_printer_attrs[feature].shift);
      return ((stp_escp2_model_capabilities[model].flags & featureset)==class);
    }
}

#define DEF_SIMPLE_ACCESSOR(f, t)			\
static t						\
escp2_##f(int model, const stp_vars_t v)		\
{							\
  return (stp_escp2_model_capabilities[model].f);	\
}

#define DEF_MICROWEAVE_ACCESSOR(f, t)					     \
static t								     \
escp2_##f(int model, const stp_vars_t v)				     \
{									     \
  const res_t *res = escp2_find_resolution(model, v, stp_get_resolution(v)); \
  if (res && !(res->softweave))						     \
    return (stp_escp2_model_capabilities[model].m_##f);			     \
  else									     \
    return (stp_escp2_model_capabilities[model].f);			     \
}

DEF_SIMPLE_ACCESSOR(max_hres, int)
DEF_SIMPLE_ACCESSOR(max_vres, int)
DEF_SIMPLE_ACCESSOR(min_hres, int)
DEF_SIMPLE_ACCESSOR(min_vres, int)
DEF_SIMPLE_ACCESSOR(nozzles, unsigned)
DEF_SIMPLE_ACCESSOR(black_nozzles, unsigned)
DEF_SIMPLE_ACCESSOR(min_nozzles, unsigned)
DEF_SIMPLE_ACCESSOR(min_black_nozzles, unsigned)
DEF_SIMPLE_ACCESSOR(nozzle_separation, unsigned)
DEF_SIMPLE_ACCESSOR(black_nozzle_separation, unsigned)
DEF_SIMPLE_ACCESSOR(separation_rows, unsigned)
DEF_SIMPLE_ACCESSOR(xres, unsigned)
DEF_SIMPLE_ACCESSOR(enhanced_xres, unsigned)
DEF_SIMPLE_ACCESSOR(max_paper_width, unsigned)
DEF_SIMPLE_ACCESSOR(max_paper_height, unsigned)
DEF_SIMPLE_ACCESSOR(min_paper_width, unsigned)
DEF_SIMPLE_ACCESSOR(min_paper_height, unsigned)
DEF_SIMPLE_ACCESSOR(extra_feed, unsigned)
DEF_SIMPLE_ACCESSOR(pseudo_separation_rows, int)
DEF_SIMPLE_ACCESSOR(base_separation, int)
DEF_SIMPLE_ACCESSOR(base_resolution, int)
DEF_SIMPLE_ACCESSOR(enhanced_resolution, int)
DEF_SIMPLE_ACCESSOR(resolution_scale, int)
DEF_SIMPLE_ACCESSOR(lum_adjustment, const double *)
DEF_SIMPLE_ACCESSOR(hue_adjustment, const double *)
DEF_SIMPLE_ACCESSOR(sat_adjustment, const double *)
DEF_SIMPLE_ACCESSOR(head_offset, const int *)
DEF_SIMPLE_ACCESSOR(initial_vertical_offset, int)
DEF_SIMPLE_ACCESSOR(black_initial_vertical_offset, int)
DEF_SIMPLE_ACCESSOR(max_black_resolution, int)
DEF_SIMPLE_ACCESSOR(zero_margin_offset, int)
DEF_SIMPLE_ACCESSOR(paperlist, const paperlist_t *)
DEF_SIMPLE_ACCESSOR(reslist, const res_t *)
DEF_SIMPLE_ACCESSOR(inklist, const inklist_t *)

DEF_MICROWEAVE_ACCESSOR(left_margin, unsigned)
DEF_MICROWEAVE_ACCESSOR(right_margin, unsigned)
DEF_MICROWEAVE_ACCESSOR(top_margin, unsigned)
DEF_MICROWEAVE_ACCESSOR(bottom_margin, unsigned)
DEF_MICROWEAVE_ACCESSOR(roll_left_margin, unsigned)
DEF_MICROWEAVE_ACCESSOR(roll_right_margin, unsigned)
DEF_MICROWEAVE_ACCESSOR(roll_top_margin, unsigned)
DEF_MICROWEAVE_ACCESSOR(roll_bottom_margin, unsigned)

static int
reslist_count(const res_t *rt)
{
  int i = 0;
  while (rt->hres)
    {
      i++;
      rt++;
    }
  return i;
}

static int
escp2_ink_type(int model, int resid, const stp_vars_t v)
{
  int dotid = resid2dotid(resid);
  return stp_escp2_model_capabilities[model].dot_sizes[dotid];
}

static double
escp2_density(int model, int resid, const stp_vars_t v)
{
  int densid = resid2densid(resid);
  return stp_escp2_model_capabilities[model].densities[densid];
}

static const escp2_variable_inkset_t *
escp2_inks(int model, int resid, int colors, int bits, const stp_vars_t v)
{
  const escp2_variable_inklist_t *inks =
    stp_escp2_model_capabilities[model].inks;
  int inktype = bits2inktype(bits);
  int inkset = colors2inkset(colors);
  resid /= 2;
  return (*inks)[inktype][inkset][resid];
}

static const paper_t *
get_media_type(int model, const char *name, const stp_vars_t v)
{
  int i;
  const paperlist_t *p = escp2_paperlist(model, v);
  int paper_type_count = p->paper_count;
  for (i = 0; i < paper_type_count; i++)
    {
      if (!strcmp(name, p->papers[i].name))
	return &(p->papers[i]);
    }
  return NULL;
}

static int
escp2_has_advanced_command_set(int model, const stp_vars_t v)
{
  return (escp2_has_cap(model, MODEL_COMMAND, MODEL_COMMAND_PRO,v) ||
	  escp2_has_cap(model, MODEL_COMMAND, MODEL_COMMAND_1999,v) ||
	  escp2_has_cap(model, MODEL_COMMAND, MODEL_COMMAND_2000,v));
}

static char *
c_strdup(const char *s)
{
  char *ret = stp_malloc(strlen(s) + 1);
  strcpy(ret, s);
  return ret;
}

static int
verify_resolution(const res_t *res,
		  int model,
		  const stp_vars_t v)
{
  int nozzle_width =
    (escp2_base_separation(model, v) / escp2_nozzle_separation(model, v));
  int nozzles = escp2_nozzles(model, v);
  if (escp2_ink_type(model, res->resid, v) != -1 &&
      res->vres <= escp2_max_vres(model, v) &&
      res->hres <= escp2_max_hres(model, v) &&
      res->vres >= escp2_min_vres(model, v) &&
      res->hres >= escp2_min_hres(model, v) &&
      (res->microweave == 0 ||
       !escp2_has_cap(model, MODEL_MICROWEAVE,
		      MODEL_MICROWEAVE_NO, v)) &&
      (res->microweave <= 1 ||
       escp2_has_cap(model, MODEL_MICROWEAVE,
		     MODEL_MICROWEAVE_ENHANCED, v)) &&
      (nozzles == 1 ||
       ((res->vres / nozzle_width) * nozzle_width) == res->vres))
    {
      int xdpi = res->hres;
      int physical_xdpi =
	xdpi > escp2_enhanced_resolution(model, v) ?
	escp2_enhanced_xres(model, v) :
	escp2_xres(model, v);
      int horizontal_passes = xdpi / physical_xdpi;
      int oversample = horizontal_passes * res->vertical_passes
	* res->vertical_oversample;
      if (horizontal_passes < 1)
	horizontal_passes = 1;
      if (oversample < 1)
	oversample = 1;
      if (((horizontal_passes * res->vertical_passes) <= 8) &&
	  (! res->softweave || (nozzles > 1 && nozzles > oversample)))
	return 1;
    }
  return 0;
}


/*
 * 'escp2_parameters()' - Return the parameter values for the given parameter.
 */

static stp_param_t *				/* O - Parameter values */
escp2_parameters(const stp_printer_t printer,	/* I - Printer model */
		 const char *ppd_file,	/* I - PPD file (not used) */
		 const char *name,	/* I - Name of parameter */
		 int  *count)		/* O - Number of values */
{
  int		i;
  stp_param_t	*valptrs;
  int		model = stp_printer_get_model(printer);
  const stp_vars_t v = stp_printer_get_printvars(printer);

  if (count == NULL)
    return (NULL);

  *count = 0;

  if (name == NULL)
    return (NULL);

  if (strcmp(name, "PageSize") == 0)
  {
    unsigned int height_limit, width_limit;
    unsigned int min_height_limit, min_width_limit;
    int papersizes = stp_known_papersizes();
    valptrs = stp_malloc(sizeof(stp_param_t) * papersizes);
    *count = 0;
    width_limit = escp2_max_paper_width(model, v);
    height_limit = escp2_max_paper_height(model, v);
    min_width_limit = escp2_min_paper_width(model, v);
    min_height_limit = escp2_min_paper_height(model, v);

    for (i = 0; i < papersizes; i++)
    {
      const stp_papersize_t pt = stp_get_papersize_by_index(i);
      unsigned int pwidth = stp_papersize_get_width(pt);
      unsigned int pheight = stp_papersize_get_height(pt);
      if (strlen(stp_papersize_get_name(pt)) > 0 &&
	  pwidth <= width_limit && pheight <= height_limit &&
	  (pheight >= min_height_limit || pheight == 0) &&
	  (pwidth >= min_width_limit || pwidth == 0) &&
	  (pwidth == 0 || pheight > 0 ||
	   escp2_has_cap(model, MODEL_ROLLFEED, MODEL_ROLLFEED_YES, v)))
	{
	  valptrs[*count].name = c_strdup(stp_papersize_get_name(pt));
	  valptrs[*count].text = c_strdup(stp_papersize_get_text(pt));
	  (*count)++;
	}
    }

    return (valptrs);
  }
  else if (strcmp(name, "Resolution") == 0)
  {
    const res_t *res = escp2_reslist(model, v);
    valptrs = stp_malloc(sizeof(stp_param_t) * reslist_count(res));
    *count = 0;
    while (res->hres)
      {
	if (verify_resolution(res, model, v))
	  {
	    valptrs[*count].name = c_strdup(res->name);
	    valptrs[*count].text = c_strdup(_(res->text));
	    (*count)++;
	  }
	res++;
      }
    return (valptrs);
  }
  else if (strcmp(name, "InkType") == 0)
  {
    const inklist_t *inks = escp2_inklist(model, v);
    valptrs = stp_malloc(sizeof(stp_param_t) * inks->n_inks);
    *count = 0;
    for (i = 0; i < inks->n_inks; i++)
    {
      valptrs[*count].name = c_strdup(inks->inknames[i].name);
      valptrs[*count].text = c_strdup(_(inks->inknames[i].text));
      (*count)++;
    }
    return valptrs;
  }
  else if (strcmp(name, "MediaType") == 0)
  {
    const paperlist_t *p = escp2_paperlist(model, v);
    int nmediatypes = p->paper_count;
    valptrs = stp_malloc(sizeof(stp_param_t) * nmediatypes);
    for (i = 0; i < nmediatypes; i++)
    {
      valptrs[i].name = c_strdup(p->papers[i].name);
      valptrs[i].text = c_strdup(_(p->papers[i].text));
    }
    *count = nmediatypes;
    return valptrs;
  }
  else if (strcmp(name, "InputSlot") == 0)
  {
    if (escp2_has_cap(model, MODEL_ROLLFEED, MODEL_ROLLFEED_NO, v))
      return NULL;
    else
    {      /* Roll Feed capable printers */
      valptrs = stp_malloc(sizeof(stp_param_t) * 2);
      valptrs[0].name = c_strdup("Standard");
      valptrs[0].text = c_strdup(_("Standard"));
      valptrs[1].name = c_strdup("Roll");
      valptrs[1].text = c_strdup(_("Roll Feed"));
      *count = 2;
      return valptrs;
    }
  }
  else
    return (NULL);
}

static const res_t *
escp2_find_resolution(int model, const stp_vars_t v, const char *resolution)
{
  const res_t *res;
  if (!resolution || !strcmp(resolution, ""))
    return NULL;
  for (res = escp2_reslist(model, v);;res++)
    {
      if (!strcmp(resolution, res->name))
	return res;
      else if (!strcmp(res->name, ""))
	return NULL;
    }
}

/*
 * 'escp2_imageable_area()' - Return the imageable area of the page.
 */

static void
escp2_imageable_area(const stp_printer_t printer,	/* I - Printer model */
		     const stp_vars_t v,   /* I */
		     int  *left,	/* O - Left position in points */
		     int  *right,	/* O - Right position in points */
		     int  *bottom,	/* O - Bottom position in points */
		     int  *top)		/* O - Top position in points */
{
  int	width, height;			/* Size of page */
  int	rollfeed;			/* Roll feed selected */
  int model = stp_printer_get_model(printer);

  rollfeed = (strcmp(stp_get_media_source(v), "Roll") == 0);

  stp_default_media_size(printer, v, &width, &height);

  if (rollfeed)
    {
      *left =	escp2_roll_left_margin(model, v);
      *right =	width - escp2_roll_right_margin(model, v);
      *top =	height - escp2_roll_top_margin(model, v);
      *bottom =	escp2_roll_bottom_margin(model, v);
    }
  else
    {
      *left =	escp2_left_margin(model, v);
      *right =	width - escp2_right_margin(model, v);
      *top =	height - escp2_top_margin(model, v);
      *bottom =	escp2_bottom_margin(model, v);
    }
}

static void
escp2_limit(const stp_printer_t printer,	/* I - Printer model */
	    const stp_vars_t v,			/* I */
	    int *width,
	    int *height,
	    int *min_width,
	    int *min_height)
{
  int model = stp_printer_get_model(printer);
  *width =	escp2_max_paper_width(model, v);
  *height =	escp2_max_paper_height(model, v);
  *min_width =	escp2_min_paper_width(model, v);
  *min_height =	escp2_min_paper_height(model, v);
}

static const char *
escp2_default_parameters(const stp_printer_t printer,
			 const char *ppd_file,
			 const char *name)
{
  int i;
  int model = stp_printer_get_model(printer);
  const stp_vars_t v = stp_printer_get_printvars(printer);
  if (name == NULL)
    return NULL;
  if (strcmp(name, "PageSize") == 0)
    {
      unsigned int height_limit, width_limit;
      unsigned int min_height_limit, min_width_limit;
      int papersizes = stp_known_papersizes();
      width_limit = escp2_max_paper_width(model, v);
      height_limit = escp2_max_paper_height(model, v);
      min_width_limit = escp2_min_paper_width(model, v);
      min_height_limit = escp2_min_paper_height(model, v);
      for (i = 0; i < papersizes; i++)
	{
	  const stp_papersize_t pt = stp_get_papersize_by_index(i);
	  unsigned int pwidth = stp_papersize_get_width(pt);
	  unsigned int pheight = stp_papersize_get_height(pt);
	  if (strlen(stp_papersize_get_name(pt)) > 0 &&
	      pwidth >= min_width_limit && pheight >= min_height_limit &&
	      pwidth <= width_limit && pheight <= height_limit)
	    return (stp_papersize_get_name(pt));
	}
      return NULL;
    }
  else if (strcmp(name, "Resolution") == 0)
    {
      int model = stp_printer_get_model(printer);
      stp_vars_t v = stp_printer_get_printvars(printer);
      const res_t *res = escp2_reslist(model, v);
      while (res->hres)
	{
	  if (res->vres >= 360 && res->hres >= 360 &&
	      verify_resolution(res, model, v))
	    {
	      return (res->name);
	    }
	  res++;
	}
      return NULL;
    }
  else if (strcmp(name, "InkType") == 0)
    {
      const inklist_t *inks = escp2_inklist(model, v);
      return inks->inknames[0].name;
    }
  else if (strcmp(name, "MediaType") == 0)
    {
      const paperlist_t *p = escp2_paperlist(model, v);
      return (p->papers[0].name);
    }
  else if (strcmp(name, "InputSlot") == 0)
    {
      if (escp2_has_cap(model, MODEL_ROLLFEED, MODEL_ROLLFEED_NO, v))
	return NULL;
      else
	return "Standard";
    }
  else
    return (NULL);

}

static void
escp2_describe_resolution(const stp_printer_t printer,
			  const char *resolution, int *x, int *y)
{
  int model = stp_printer_get_model(printer);
  stp_vars_t v = stp_printer_get_printvars(printer);
  const res_t *res = escp2_reslist(model, v);
  int nozzle_width =
    escp2_base_separation(model, v) / escp2_nozzle_separation(model, v);
  while (res->hres)
    {
      if (escp2_ink_type(model, res->resid, v) != -1 &&
	  res->vres <= escp2_max_vres(model, v) &&
	  res->hres <= escp2_max_hres(model, v) &&
	  (res->microweave == 0 ||
	   !escp2_has_cap(model, MODEL_MICROWEAVE,
			  MODEL_MICROWEAVE_NO, v)) &&
	  (res->microweave <= 1 ||
	   escp2_has_cap(model, MODEL_MICROWEAVE,
			 MODEL_MICROWEAVE_ENHANCED, v)) &&
	  ((res->vres / nozzle_width) * nozzle_width) == res->vres &&
	  !strcmp(resolution, res->name))
	{
	  *x = res->hres;
	  *y = res->vres;
	  return;
	}
      res++;
    }
  *x = -1;
  *y = -1;
}

static void
escp2_reset_printer(const escp2_init_t *init)
{
  /*
   * Magic initialization string that's needed to take printer out of
   * packet mode.
   */
  if (escp2_has_cap(init->model, MODEL_INIT, MODEL_INIT_NEW, init->v))
    stp_zprintf(init->v, "%c%c%c\033\001@EJL 1284.4\n@EJL     \n\033@", 0, 0, 0);

  stp_puts("\033@", init->v);					/* ESC/P2 reset */
}

static void
escp2_set_remote_sequence(const escp2_init_t *init)
{
  /* Magic remote mode commands, whatever they do */

#if 0
  stp_zprintf(init->v, "\033(R%c%c%c%s", 1 + strlen(PACKAGE), 0, 0, PACKAGE);
  stp_zprintf(init->v, "\033%c%c%c", 0, 0, 0);
  stp_zprintf(init->v, "\033(R%c%c%c%s", 1 + strlen(VERSION), 0, 0, VERSION);
  stp_zprintf(init->v, "\033%c%c%c", 0, 0, 0);
  stp_zprintf(init->v, "\033(R%c%c%c%s", 1 + strlen(stp_get_driver(init->v)),
	      0, 0, stp_get_driver(init->v));
  stp_zprintf(init->v, "\033%c%c%c", 0, 0, 0);
  stp_puts("\033@", init->v);
#endif
  if (escp2_has_advanced_command_set(init->model, init->v))
    {
      int feed_sequence = 0;
      const paper_t *p =
	get_media_type(init->model, init->paper_type, init->v);
      /* Enter remote mode */
      stp_zprintf(init->v, "\033(R%c%c%cREMOTE1", 8, 0, 0);
      if (escp2_has_cap(init->model, MODEL_COMMAND,
			MODEL_COMMAND_PRO, init->v))
	{
	  /* Set Roll Feed mode */
	  if (strcmp(init->media_source,"Roll") == 0)
	    stp_zprintf(init->v, "PP%c%c%c%c%c", 3, 0, 0, 3, 0);
	  else
	    stp_zprintf(init->v, "PP%c%c%c%c%c", 3, 0, 0, 2, 0);
	  if (p)
	    {
	      stp_zprintf(init->v, "PH%c%c%c%c", 2, 0, 0, p->paper_thickness);
	      if (escp2_has_cap(init->model, MODEL_VACUUM, MODEL_VACUUM_YES,
				init->v))
		stp_zprintf(init->v, "SN%c%c%c%c%c", 3, 0, 0, 5,p->vacuum_intensity);
	      stp_zprintf(init->v, "SN%c%c%c%c%c", 3, 0, 0, 4, p->feed_adjustment);
	    }
	}
      else
	{
	  if (p)
	    feed_sequence = p->paper_feed_sequence;
	  /* Function unknown */
	  stp_zprintf(init->v, "PM%c%c%c%c", 2, 0, 0, 0);
	  /* Set mechanism sequence */
	  stp_zprintf(init->v, "SN%c%c%c%c%c", 3, 0, 0, 0, feed_sequence);
	  if (escp2_has_cap(init->model, MODEL_XZEROMARGIN,
			    MODEL_XZEROMARGIN_YES, init->v))
	    stp_zprintf(init->v, "FP%c%c%c\260\377", 3, 0, 0);

	  /* set up Roll-Feed options on appropriate printers
	     (tested for STP 870, which has no cutter) */
	  if (escp2_has_cap(init->model, MODEL_ROLLFEED,
			    MODEL_ROLLFEED_YES, init->v))
	    {
	      if (strcmp(init->media_source,"Roll") == 0)
		stp_zprintf(init->v, /* Set Roll Feed mode */
			    "IR%c%c%c%c"
			    "EX%c%c%c%c%c%c%c%c",
			    2, 0, 0, 1,
			    6, 0, 0, 0, 0, 0, 5, 1);
	      else
		stp_zprintf(init->v, /* Set non-Roll Feed mode */
			    "IR%c%c%c%c"
			    "EX%c%c%c%c%c%c%c%c",
			    2, 0, 0, 3,
			    6, 0, 0, 0, 0, 0, 5, 0);
	    }
	}
      /* Exit remote mode */
      stp_zprintf(init->v, "\033%c%c%c", 0, 0, 0);
    }
}

static void
escp2_set_graphics_mode(const escp2_init_t *init)
{
  stp_zfwrite("\033(G\001\000\001", 6, 1, init->v);	/* Enter graphics mode */
}

static void
escp2_set_resolution(const escp2_init_t *init)
{
  if (escp2_has_cap(init->model, MODEL_COMMAND, MODEL_COMMAND_PRO, init->v) ||
      (!(escp2_has_cap(init->model, MODEL_VARIABLE_DOT,
		       MODEL_VARIABLE_NORMAL, init->v)) &&
       init->use_softweave))
    {
      int hres = escp2_max_hres(init->model, init->v);
      stp_zprintf(init->v, "\033(U\005%c%c%c%c%c%c", 0, hres / init->ydpi,
		  hres / init->ydpi, hres / init->xdpi,
		  hres % 256, hres / 256);
    }
  else
    stp_zprintf(init->v, "\033(U\001%c%c", 0, 3600 / init->ydpi);
}

static void
escp2_set_color(const escp2_init_t *init)
{
  if (escp2_has_cap(init->model, MODEL_GRAYMODE, MODEL_GRAYMODE_YES,
		    init->v))
    stp_zprintf(init->v, "\033(K\002%c%c%c", 0, 0,
		(init->output_type == OUTPUT_GRAY ? 1 : 2));
}

static void
escp2_set_microweave(const escp2_init_t *init)
{
  stp_zprintf(init->v, "\033(i\001%c%c", 0, init->use_microweave);
}

static void
escp2_set_printhead_speed(const escp2_init_t *init)
{
  if (init->unidirectional)
    {
      stp_zprintf(init->v, "\033U%c", 1);
      if (init->xdpi > escp2_enhanced_resolution(init->model, init->v))
	stp_zprintf(init->v, "\033(s%c%c%c", 1, 0, 2);
    }
  else
    stp_zprintf(init->v, "\033U%c", 0);
}

static void
escp2_set_dot_size(const escp2_init_t *init)
{
  /* Dot size */
  int drop_size = escp2_ink_type(init->model, init->resid, init->v);
  if (drop_size >= 0)
    stp_zprintf(init->v, "\033(e\002%c%c%c", 0, 0, drop_size);
}

static void
escp2_set_page_height(const escp2_init_t *init)
{
  int l = init->ydpi * init->page_height / 72;
  if (escp2_has_cap(init->model, MODEL_COMMAND, MODEL_COMMAND_PRO, init->v) ||
      (!(escp2_has_cap(init->model, MODEL_VARIABLE_DOT,
		       MODEL_VARIABLE_NORMAL, init->v)) &&
       init->use_softweave))
    stp_zprintf(init->v, "\033(C\004%c%c%c%c%c", 0,
		l & 0xff, (l >> 8) & 0xff, (l >> 16) & 0xff, (l >> 24) & 0xff);
  else
    stp_zprintf(init->v, "\033(C\002%c%c%c", 0, l & 255, l >> 8);
}

static void
escp2_set_margins(const escp2_init_t *init)
{
  int l = init->ydpi * (init->page_height - init->page_bottom) / 72;
  int t = init->ydpi * (init->page_height - init->page_top) / 72;

  t += init->initial_vertical_offset;
  if (escp2_has_cap(init->model, MODEL_COMMAND, MODEL_COMMAND_PRO, init->v) ||
      (!(escp2_has_cap(init->model, MODEL_VARIABLE_DOT,
		       MODEL_VARIABLE_NORMAL, init->v)) &&
       init->use_softweave))
    {
      if (escp2_has_cap(init->model, MODEL_COMMAND,MODEL_COMMAND_2000,init->v))
	stp_zprintf(init->v, "\033(c\010%c%c%c%c%c%c%c%c%c", 0,
		    t & 0xff, t >> 8, (t >> 16) & 0xff, (t >> 24) & 0xff,
		    l & 0xff, l >> 8, (l >> 16) & 0xff, (l >> 24) & 0xff);
      else
	stp_zprintf(init->v, "\033(c\004%c%c%c%c%c", 0,
		    t & 0xff, t >> 8, l & 0xff, l >> 8);
    }
  else
    stp_zprintf(init->v, "\033(c\004%c%c%c%c%c", 0,
		t & 0xff, t >> 8, l & 0xff, l >> 8);
}

static void
escp2_set_form_factor(const escp2_init_t *init)
{
  if (escp2_has_advanced_command_set(init->model, init->v))
    {
      int page_width = init->page_width * init->ydpi / 72;
      int page_height = init->page_height * init->ydpi / 72;

      if (escp2_has_cap(init->model, MODEL_XZEROMARGIN, MODEL_XZEROMARGIN_YES,
			init->v))
	/* Make the page 2/10" wider (probably ignored by the printer) */
	page_width += 144 * init->xdpi / 720;

      stp_zprintf(init->v, "\033(S\010%c%c%c%c%c%c%c%c%c", 0,
		  ((page_width >> 0) & 0xff), ((page_width >> 8) & 0xff),
		  ((page_width >> 16) & 0xff), ((page_width >> 24) & 0xff),
		  ((page_height >> 0) & 0xff), ((page_height >> 8) & 0xff),
		  ((page_height >> 16) & 0xff), ((page_height >> 24) & 0xff));
    }
}

static void
escp2_set_printhead_resolution(const escp2_init_t *init)
{
  if (escp2_has_cap(init->model, MODEL_COMMAND, MODEL_COMMAND_PRO, init->v) ||
      (!(escp2_has_cap(init->model, MODEL_VARIABLE_DOT,
		       MODEL_VARIABLE_NORMAL, init->v)) &&
       init->use_softweave))
    {
      int xres;
      int yres;
      int nozzle_separation;

      if (escp2_has_cap(init->model, MODEL_COMMAND, MODEL_COMMAND_PRO,init->v))
	xres = init->xdpi;
      else if (init->xdpi > escp2_enhanced_resolution(init->model, init->v))
	xres = escp2_enhanced_xres(init->model, init->v);
      else
	xres = escp2_xres(init->model, init->v);
      if (init->xdpi < xres)
	xres = init->xdpi;
      xres = escp2_resolution_scale(init->model, init->v) / xres;

      if (init->output_type == OUTPUT_GRAY &&
	  (escp2_max_black_resolution(init->model, init->v) < 0 ||
	   init->ydpi <= escp2_max_black_resolution(init->model, init->v)))
	nozzle_separation = escp2_black_nozzle_separation(init->model,
							  init->v);
      else
	nozzle_separation = escp2_nozzle_separation(init->model, init->v);

      if (escp2_has_cap(init->model, MODEL_COMMAND, MODEL_COMMAND_PRO,
			init->v) && !init->use_softweave)
	yres = escp2_resolution_scale(init->model, init->v) / init->ydpi;
      else
	yres = (nozzle_separation *
		escp2_resolution_scale(init->model, init->v) /
		escp2_base_separation(init->model, init->v));

      /* Magic resolution cookie */
      stp_zprintf(init->v, "\033(D%c%c%c%c%c%c", 4, 0,
		  escp2_resolution_scale(init->model, init->v) % 256,
		  escp2_resolution_scale(init->model, init->v) / 256,
		  yres, xres);
    }
}

static void
escp2_init_printer(const escp2_init_t *init)
{
  escp2_reset_printer(init);
  escp2_set_remote_sequence(init);
  escp2_set_graphics_mode(init);
  escp2_set_resolution(init);
  escp2_set_color(init);
  escp2_set_microweave(init);
  escp2_set_printhead_speed(init);
  escp2_set_dot_size(init);
  escp2_set_printhead_resolution(init);
  escp2_set_page_height(init);
  escp2_set_margins(init);
  escp2_set_form_factor(init);
}

static void
escp2_deinit_printer(const escp2_init_t *init, int printed_something)
{
  if (!printed_something)
    stp_putc('\n', init->v);
  stp_puts(/* Eject page */
	   "\014"
	   /* ESC/P2 reset */
	   "\033@", init->v);
  if (escp2_has_advanced_command_set(init->model, init->v))
    {
      stp_zprintf(init->v, /* Enter remote mode */
		  "\033(R\010%c%cREMOTE1", 0, 0);
      /* set up Roll-Feed options on appropriate printers
	 (tested for STP 870, which has no cutter) */
      if (escp2_has_cap(init->model, MODEL_ROLLFEED,
			MODEL_ROLLFEED_YES, init->v))
	{
	  /* End Roll Feed mode */
	  if (strcmp(init->media_source, "Roll") == 0)
	    stp_zprintf(init->v, "IR\002%c%c%c", 0, 0, 0);
	  else
	    stp_zprintf(init->v, "IR\002%c%c%c", 0, 0, 2);
	}
      /* Load settings from NVRAM */
      stp_zprintf(init->v, "LD%c%c", 0, 0);

      /* Magic deinit sequence reported by Simone Falsini */
      if (escp2_has_cap(init->model, MODEL_DEINITIALIZE_JE,
			MODEL_DEINITIALIZE_JE_YES, init->v))
	stp_zprintf(init->v, "JE%c%c%c", 1, 0, 0);
		  /* Exit remote mode */
      stp_zprintf(init->v, "\033%c%c%c", 0, 0, 0);

    }
}

static void
adjust_print_quality(const escp2_init_t *init, void *dither,
		     double *lum_adjustment, double *sat_adjustment,
		     double *hue_adjustment)
{
  const paper_t *pt;
  const stp_vars_t nv = init->v;
  int i;
  const escp2_variable_inkset_t *inks;
  double k_upper, k_lower;
  int		ink_spread;
  /*
   * Compute the LUT.  For now, it's 8 bit, but that may eventually
   * sometimes change.
   */
  if (init->ncolors > 4)
    k_lower = .5;
  else
    k_lower = .25;

  pt = get_media_type(init->model, stp_get_media_type(nv), nv);
  if (pt)
    {
      stp_set_density(nv, stp_get_density(nv) * pt->base_density);
      if (init->ncolors >= 5)
	{
	  stp_set_cyan(nv, stp_get_cyan(nv) * pt->p_cyan);
	  stp_set_magenta(nv, stp_get_magenta(nv) * pt->p_magenta);
	  stp_set_yellow(nv, stp_get_yellow(nv) * pt->p_yellow);
	}
      else
	{
	  stp_set_cyan(nv, stp_get_cyan(nv) * pt->cyan);
	  stp_set_magenta(nv, stp_get_magenta(nv) * pt->magenta);
	  stp_set_yellow(nv, stp_get_yellow(nv) * pt->yellow);
	}
      stp_set_saturation(nv, stp_get_saturation(nv) * pt->saturation);
      stp_set_gamma(nv, stp_get_gamma(nv) * pt->gamma);
      k_lower *= pt->k_lower_scale;
      k_upper = pt->k_upper;
    }
  else				/* Can't find paper type? Assume plain */
    {
      stp_set_density(nv, stp_get_density(nv) * .8);
      k_lower *= .1;
      k_upper = .5;
    }
  stp_set_density(nv, stp_get_density(nv) *
		  escp2_density(init->model, init->resid, nv));
  if (stp_get_density(nv) > 1.0)
    stp_set_density(nv, 1.0);
  if (init->ncolors == 1)
    stp_set_gamma(nv, stp_get_gamma(nv) / .8);
  stp_compute_lut(nv, 256);

  for (i = 0; i <= NCOLORS; i++)
    stp_dither_set_black_level(dither, i, 1.0);
  stp_dither_set_black_lower(dither, k_lower);
  stp_dither_set_black_upper(dither, k_upper);

  inks = escp2_inks(init->model, init->resid, init->ncolors, init->bits, nv);
  if (inks)
    for (i = 0; i < NCOLORS; i++)
      if ((*inks)[i])
	stp_dither_set_ranges(dither, i, (*inks)[i]->count, (*inks)[i]->range,
			      (*inks)[i]->density * k_upper *
			      stp_get_density(nv));

  switch (stp_get_image_type(nv))
    {
    case IMAGE_LINE_ART:
      stp_dither_set_ink_spread(dither, 19);
      break;
    case IMAGE_SOLID_TONE:
      stp_dither_set_ink_spread(dither, 15);
      break;
    case IMAGE_CONTINUOUS:
      ink_spread = 13;
      if (init->ydpi > escp2_max_vres(init->model, nv))
	ink_spread++;
      if (init->bits > 1)
	ink_spread++;
      stp_dither_set_ink_spread(dither, ink_spread);
      break;
    }
  stp_dither_set_density(dither, stp_get_density(nv));
  if (escp2_lum_adjustment(init->model, nv))
    {
      for (i = 0; i < 49; i++)
	{
	  lum_adjustment[i] = escp2_lum_adjustment(init->model, nv)[i];
	  if (pt && pt->lum_adjustment)
	    lum_adjustment[i] *= pt->lum_adjustment[i];
	}
    }
  if (escp2_sat_adjustment(init->model, nv))
    {
      for (i = 0; i < 49; i++)
	{
	  sat_adjustment[i] = escp2_sat_adjustment(init->model, nv)[i];
	  if (pt && pt->sat_adjustment)
	    sat_adjustment[i] *= pt->sat_adjustment[i];
	}
    }
  if (escp2_hue_adjustment(init->model, nv))
    {
      for (i = 0; i < 49; i++)
	{
	  hue_adjustment[i] = escp2_hue_adjustment(init->model, nv)[i];
	  if (pt && pt->hue_adjustment)
	    hue_adjustment[i] += pt->hue_adjustment[i];
	}
    } 
} 

static void
get_inktype(const stp_printer_t printer, const stp_vars_t v,
	    int model, int *hasblack, int *ncolors)
{
  int		output_type = stp_get_output_type(v);
  const char	*ink_type = stp_get_ink_type(v);
  const inklist_t *ink_names = escp2_inklist(model, v);
  int i;

  if (output_type == OUTPUT_GRAY || output_type == OUTPUT_MONOCHROME)
    *ncolors = 1;
  else
    for (i = 0; i < ink_names->n_inks; i++)
      {
	if (strcmp(ink_type, ink_names->inknames[i].name) == 0)
	  {
	    *hasblack = ink_names->inknames[i].hasblack;
	    *ncolors = ink_names->inknames[i].ncolors;
	    break;
	  }
      }
  if (ncolors == 0)
    {
      ink_type = escp2_default_parameters(printer, NULL, "InkType");
      for (i = 0; i < ink_names->n_inks; i++)
	{
	  if (strcmp(ink_type, ink_names->inknames[i].name) == 0)
	    {
	      *hasblack = ink_names->inknames[i].hasblack;
	      *ncolors = ink_names->inknames[i].ncolors;
	      break;
	    }
	}
    }
}  

/*
 * 'escp2_print()' - Print an image to an EPSON printer.
 */
static void
escp2_print(const stp_printer_t printer,		/* I - Model */
	    stp_image_t     *image,		/* I - Image to print */
	    const stp_vars_t    v)
{
  unsigned char *cmap = stp_get_cmap(v);
  int		model = stp_printer_get_model(printer);
  const char	*resolution = stp_get_resolution(v);
  const char	*media_type = stp_get_media_type(v);
  int		output_type = stp_get_output_type(v);
  int		orientation = stp_get_orientation(v);
  double	scaling = stp_get_scaling(v);
  const char	*media_source = stp_get_media_source(v);
  int		top = stp_get_top(v);
  int		left = stp_get_left(v);
  int		y;		/* Looping vars */
  int		xdpi, ydpi;	/* Resolution */
  int		undersample;
  int		undersample_denominator;
  int		resid;
  int		physical_ydpi;
  int		physical_xdpi;
  int		i;
  int		n;		/* Output number */
  unsigned short *out;	/* Output pixels (16-bit) */
  unsigned char	*in,		/* Input pixels */
		*black = NULL,		/* Black bitmap data */
		*cyan = NULL,		/* Cyan bitmap data */
		*magenta = NULL,	/* Magenta bitmap data */
		*yellow = NULL,		/* Yellow bitmap data */
		*lcyan = NULL,		/* Light cyan bitmap data */
		*lmagenta = NULL,	/* Light magenta bitmap data */
		*dyellow = NULL;	/* Dark yellow bitmap data */
  int		page_left,	/* Left margin of page */
		page_right,	/* Right margin of page */
		page_top,	/* Top of page */
		page_bottom,	/* Bottom of page */
		page_width,	/* Width of page */
		page_height,	/* Height of page */
		page_true_height,	/* True height of page */
		out_width,	/* Width of image on page */
		out_height,	/* Height of image on page */
		out_bpp,	/* Output bytes per pixel */
		length,		/* Length of raster data */
		errdiv,		/* Error dividend */
		errmod,		/* Error modulus */
		errval,		/* Current error value */
		errline,	/* Current raster line */
		errlast;	/* Last raster line loaded */
  stp_convert_t	colorfunc = 0;	/* Color conversion function... */
  int		zero_mask;
  int   	image_height,
		image_width,
		image_bpp;
  int		use_softweave = 0;
  int		use_microweave = 0;
  int		nozzles = 1;
  int		nozzle_separation = 1;
  int		horizontal_passes = 1;
  int		vertical_passes = 1;
  int		vertical_oversample = 1;
  int		unidirectional = 0;
  int		hasblack = 0;
  const res_t	*res;
  int		bits = 1;
  void *	weave = NULL;
  void *	dither;
  int		separation_rows;
  stp_vars_t	nv = stp_allocate_copy(v);
  escp2_init_t	init;
  int max_vres;
  const unsigned char *cols[7];
  int head_offset[8];
  const int *offset_ptr;
  int max_head_offset;
  double lum_adjustment[49], sat_adjustment[49], hue_adjustment[49];
  int ncolors = 0;
  escp2_privdata_t privdata;
  int drop_size;
  int min_nozzles;
  stp_dither_data_t *dt;

  if (!stp_get_verified(nv))
    {
      stp_eprintf(nv, "Print options not verified; cannot print.\n");
      return;
    }

  privdata.undersample = 1;
  privdata.initial_vertical_offset = 0;
  privdata.printed_something = 0;
  stp_set_driver_data(nv, &privdata);

  get_inktype(printer, nv, model, &hasblack, &ncolors);

  stp_set_output_color_model(nv, COLOR_MODEL_CMY);

 /*
  * Compute the output size...
  */
  image->init(image);
  image_height = image->height(image);
  image_width = image->width(image);
  image_bpp = image->bpp(image);

  escp2_imageable_area(printer, nv, &page_left, &page_right,
		       &page_bottom, &page_top);

  stp_compute_page_parameters(page_right, page_left, page_top, page_bottom,
			      scaling, image_width, image_height, image,
			      &orientation, &page_width, &page_height,
			      &out_width, &out_height, &left, &top);

  /*
   * Recompute the image height and width.  If the image has been
   * rotated, these will change from previously.
   */
  image_height = image->height(image);
  image_width = image->width(image);
  stp_default_media_size(printer, nv, &n, &page_true_height);

  colorfunc = stp_choose_colorfunc(output_type, image_bpp, cmap, &out_bpp, nv);

 /*
  * Figure out the output resolution...
  */
  res = escp2_find_resolution(model, nv, resolution);
  use_softweave = res->softweave;
  use_microweave = res->microweave;
  if (use_softweave)
    max_vres = escp2_max_vres(model, nv);
  else
    max_vres = escp2_base_resolution(model, nv);
  xdpi = res->hres;
  ydpi = res->vres;
  resid = res->resid;
  undersample = res->vertical_undersample;
  undersample_denominator = res->vertical_denominator;
  privdata.undersample = res->vertical_undersample;
  vertical_passes = res->vertical_passes;
  vertical_oversample = res->vertical_oversample;
  unidirectional = res->unidirectional;
  drop_size = escp2_ink_type(model, resid, nv);

  if (use_microweave &&
      (escp2_has_cap(model, MODEL_MICROWEAVE_EXCEPTION,
		     MODEL_MICROWEAVE_EXCEPTION_360, nv)))
    {
      if (ydpi == 360)
	use_microweave = 0;
    }
  if (use_softweave)
    {
      physical_xdpi = (xdpi > escp2_enhanced_resolution(model, nv)) ?
	escp2_enhanced_xres(model, nv) : escp2_xres(model, nv);
      horizontal_passes = xdpi / physical_xdpi;
      if ((output_type == OUTPUT_GRAY || output_type == OUTPUT_MONOCHROME) &&
	  (escp2_max_black_resolution(model, nv) < 0 ||
	   ydpi <= escp2_max_black_resolution(model, nv)) &&
	  escp2_black_nozzles(model, nv))
	{
	  nozzles = escp2_black_nozzles(model, nv);
	  nozzle_separation = escp2_black_nozzle_separation(model, nv);
	  min_nozzles = escp2_min_black_nozzles(model, nv);
	}
      else
	{
	  nozzles = escp2_nozzles(model, nv);
	  nozzle_separation = escp2_nozzle_separation(model, nv);
	  min_nozzles = escp2_min_nozzles(model, nv);
	}
      nozzle_separation =
	nozzle_separation * ydpi / escp2_base_separation(model, nv);
    }
  else
    {
      physical_xdpi = (xdpi <= escp2_base_resolution(model, nv)) ?
	xdpi : escp2_base_resolution(model, nv);
      horizontal_passes = xdpi / escp2_base_resolution(model, nv);
      nozzles = 1;
      min_nozzles = 1;
      nozzle_separation = 1;
    }

  separation_rows = escp2_separation_rows(model, nv);

  if (drop_size > 0 && drop_size & 0x10)
    bits = 2;
  else
    bits = 1;
  if (horizontal_passes == 0)
    horizontal_passes = 1;
  privdata.min_nozzles = min_nozzles;

  physical_ydpi = ydpi;
  if (ydpi > max_vres)
    physical_ydpi = max_vres;

  offset_ptr = escp2_head_offset(model, nv);
  max_head_offset = 0;
  if (ncolors > 1)
    for (i = 0; i < ncolors; i++)
      {
	head_offset[i] = offset_ptr[i] * ydpi /
	  escp2_base_separation(model, nv);
	if (head_offset[i] > max_head_offset)
	  max_head_offset = head_offset[i];
      }

 /*
  * Send ESC/P2 initialization commands...
  */
  init.model = model;
  init.output_type = output_type;
  if (init.output_type == OUTPUT_MONOCHROME)
    init.output_type = OUTPUT_GRAY;
  init.ydpi = ydpi * undersample;
  if (init.ydpi > escp2_max_vres(init.model, init.v))
    init.ydpi = escp2_max_vres(init.model, init.v);
  init.xdpi = xdpi;
  init.use_softweave = use_softweave;
  init.use_microweave = use_microweave;
  init.page_height = page_true_height;
  init.page_width = page_width;
  init.page_top = page_top;
  if (init.output_type == OUTPUT_GRAY)
    {
      if (escp2_max_black_resolution(model, nv) < 0 ||
	  ydpi <= escp2_max_black_resolution(init.model, init.v))
	init.initial_vertical_offset =
	  escp2_black_initial_vertical_offset(init.model, init.v) * init.ydpi /
	  escp2_base_separation(model, nv);
      else
    init.initial_vertical_offset =
      (escp2_initial_vertical_offset(init.model, init.v) + offset_ptr[0]) *
      init.ydpi / escp2_base_separation(model, nv);
    }
  else
    init.initial_vertical_offset =
      escp2_initial_vertical_offset(init.model, init.v) * init.ydpi /
      escp2_base_separation(model, nv);

   /* adjust bottom margin for a 480 like head configuration */
  init.page_bottom = page_bottom - max_head_offset * 72 / ydpi;
  if ((max_head_offset * 72 % ydpi) != 0)
    init.page_bottom -= 1;
  if (init.page_bottom < 0)
    init.page_bottom = 0;

  init.horizontal_passes = horizontal_passes;
  init.vertical_passes = vertical_passes;
  init.vertical_oversample = vertical_oversample;
  init.unidirectional = unidirectional;
  init.resid = resid;
  init.bits = bits;
  init.paper_type = media_type;
  init.media_source = media_source;
  init.v = nv;
  init.ncolors = ncolors;

  escp2_init_printer(&init);

 /*
  * Convert image size to printer resolution...
  */

  out_width  = xdpi * out_width / 72;
  out_height = ydpi * out_height / 72;

  left = physical_ydpi * undersample * left / 72 / undersample_denominator;

 /*
  * Adjust for zero-margin printing...
  */

  if (escp2_has_cap(model, MODEL_XZEROMARGIN, MODEL_XZEROMARGIN_YES, nv))
    {
     /*
      * In zero-margin mode, the origin is about 3/20" to the left of the
      * paper's left edge.
      */
      left += escp2_zero_margin_offset(model, nv) * physical_ydpi *
	undersample / max_vres / undersample_denominator;
    }

  weave = stp_initialize_weave(nozzles, nozzle_separation,
			       horizontal_passes, vertical_passes,
			       vertical_oversample, ncolors, bits,
			       out_width, out_height, separation_rows,
			       top * physical_ydpi / 72,
			       (page_height * physical_ydpi / 72 +
				escp2_extra_feed(model, nv) * physical_ydpi /
				escp2_base_resolution(model, nv)),
			       1, head_offset, nv, flush_pass,
			       FILLFUNC, PACKFUNC, COMPUTEFUNC);

 /*
  * Allocate memory for the raster data...
  */

  length = (out_width + 7) / 8;

  if (output_type == OUTPUT_GRAY || output_type == OUTPUT_MONOCHROME)
    black = stp_zalloc(length * bits);
  else
    {
      cyan = stp_zalloc(length * bits);
      magenta = stp_zalloc(length * bits);
      yellow = stp_zalloc(length * bits);

      if (ncolors == 7)
	dyellow = stp_zalloc(length * bits);
      if (ncolors >= 5)
	{
	  lcyan = stp_zalloc(length * bits);
	  lmagenta = stp_zalloc(length * bits);
	}
      if (hasblack)
	black = stp_zalloc(length * bits);
    }
  cols[0] = black;
  cols[1] = magenta;
  cols[2] = cyan;
  cols[3] = yellow;
  cols[4] = lmagenta;
  cols[5] = lcyan;
  cols[6] = dyellow;

  dt = stp_create_dither_data();
  stp_add_channel(dt, black, ECOLOR_K, 0);
  stp_add_channel(dt, cyan, ECOLOR_C, 0);
  stp_add_channel(dt, lcyan, ECOLOR_C, 1);
  stp_add_channel(dt, magenta, ECOLOR_M, 0);
  stp_add_channel(dt, lmagenta, ECOLOR_M, 1);
  stp_add_channel(dt, yellow, ECOLOR_Y, 0);
  stp_add_channel(dt, dyellow, ECOLOR_Y, 1);

  in  = stp_malloc(image_width * image_bpp);
  out = stp_malloc(image_width * out_bpp * 2);

  errdiv  = image_height / out_height;
  errmod  = image_height % out_height;
  errval  = 0;
  errlast = -1;
  errline  = 0;

  if (xdpi > ydpi)
    dither = stp_init_dither(image_width, out_width, 1, xdpi / ydpi, nv);
  else
    dither = stp_init_dither(image_width, out_width, ydpi / xdpi, 1, nv);

  adjust_print_quality(&init, dither,
		       lum_adjustment, sat_adjustment, hue_adjustment);

 /*
  * Let the user know what we're doing...
  */

  image->progress_init(image);

  QUANT(0);
  for (y = 0; y < out_height; y ++)
    {
      int duplicate_line = 1;
      if ((y & 63) == 0)
	image->note_progress(image, y, out_height);

      if (errline != errlast)
	{
	  errlast = errline;
	  duplicate_line = 0;
	  if (image->get_row(image, in, errline) != STP_IMAGE_OK)
	    break;
	  (*colorfunc)(nv, in, out, &zero_mask, image_width, image_bpp, cmap,
		       escp2_hue_adjustment(model, nv) ? hue_adjustment : NULL,
		       escp2_lum_adjustment(model, nv) ? lum_adjustment : NULL,
		       escp2_sat_adjustment(model, nv) ? sat_adjustment :NULL);
	}
      QUANT(1);

      stp_dither(out, y, dither, dt, duplicate_line, zero_mask);
      QUANT(2);

      stp_write_weave(weave, length, ydpi, model, out_width, left,
		      xdpi, physical_xdpi, cols);
      QUANT(3);
      errval += errmod;
      errline += errdiv;
      if (errval >= out_height)
	{
	  errval -= out_height;
	  errline ++;
	}
      QUANT(4);
    }
  image->progress_conclude(image);
  stp_flush_all(weave, model, out_width, left, ydpi, xdpi, physical_xdpi);
  QUANT(5);

  stp_free_dither_data(dt);
  stp_free_dither(dither);

 /*
  * Cleanup...
  */
  escp2_deinit_printer(&init, privdata.printed_something);

  stp_free_lut(nv);
  stp_free(in);
  stp_free(out);
  stp_destroy_weave(weave);

  for (i = 0; i < 7; i++)
    if (cols[i])
      stp_free((unsigned char *) cols[i]);

#ifdef QUANTIFY
  print_timers(nv);
#endif
  stp_free_vars(nv);
}

const stp_printfuncs_t stp_escp2_printfuncs =
{
  escp2_parameters,
  stp_default_media_size,
  escp2_imageable_area,
  escp2_limit,
  escp2_print,
  escp2_default_parameters,
  escp2_describe_resolution,
  stp_verify_printer_params
};

static void
set_vertical_position(stp_softweave_t *sw, stp_pass_t *pass, int model,
		      const stp_vars_t v)
{
  escp2_privdata_t *pd = (escp2_privdata_t *) stp_get_driver_data(v);
  int advance = pass->logicalpassstart - sw->last_pass_offset -
    (sw->separation_rows - 1);
  advance *= pd->undersample;
  if (pass->logicalpassstart > sw->last_pass_offset ||
      pd->initial_vertical_offset != 0)
    {
      int a0, a1, a2, a3;
      advance += pd->initial_vertical_offset;
      pd->initial_vertical_offset = 0;
      a0 = advance         & 0xff;
      a1 = (advance >> 8)  & 0xff;
      a2 = (advance >> 16) & 0xff;
      a3 = (advance >> 24) & 0xff;
      if (!escp2_has_cap(model, MODEL_COMMAND, MODEL_COMMAND_PRO, v) &&
	  (sw->jets == 1 || escp2_has_cap(model, MODEL_VARIABLE_DOT,
					  MODEL_VARIABLE_NORMAL, v)))
	stp_zprintf(v, "\033(v%c%c%c%c", 2, 0, a0, a1);
      else
	stp_zprintf(v, "\033(v%c%c%c%c%c%c", 4, 0, a0, a1, a2, a3);
      sw->last_pass_offset = pass->logicalpassstart;
    }
}

static void
set_color(stp_softweave_t *sw, stp_pass_t *pass, int model, const stp_vars_t v,
	  int color)
{
  if (sw->last_color != color &&
      !escp2_has_cap(model, MODEL_COMMAND, MODEL_COMMAND_PRO, v) &&
      (sw->jets == 1 || escp2_has_cap(model, MODEL_VARIABLE_DOT,
				      MODEL_VARIABLE_NORMAL, v)))
    {
      if (!escp2_has_cap(model, MODEL_COLOR, MODEL_COLOR_4, v))
	stp_zprintf(v, "\033(r%c%c%c%c", 2, 0, densities[color],
		    colors[color]);
      else
	stp_zprintf(v, "\033r%c", colors[color]);
      sw->last_color = color;
    }
}

static void
set_horizontal_position(stp_softweave_t *sw, stp_pass_t *pass, int model,
			const stp_vars_t v, int hoffset, int ydpi,
			int xdpi, int vertical_subpass)
{
  int microoffset = vertical_subpass & (sw->horizontal_weave - 1);
  int pos;
  if (!escp2_has_advanced_command_set(model, v) &&
      (xdpi <= escp2_base_resolution(model, v) ||
       escp2_max_hres(model, v) < 1440))
    {
      pos = (hoffset + microoffset);
      if (pos > 0)
	stp_zprintf(v, "\033\\%c%c", pos & 255, pos >> 8);
    }
  else if (escp2_has_cap(model, MODEL_COMMAND, MODEL_COMMAND_PRO,v) ||
	   (escp2_has_advanced_command_set(model, v) &&
	    !(escp2_has_cap(model, MODEL_VARIABLE_DOT,
			    MODEL_VARIABLE_NORMAL, v))))
    {
      pos = ((hoffset * xdpi / ydpi) + microoffset);
      if (pos > 0)
	stp_zprintf(v, "\033($%c%c%c%c%c%c", 4, 0,
		    pos & 255, (pos >> 8) & 255,
		    (pos >> 16) & 255, (pos >> 24) & 255);
    }
  else
    {
      pos = ((hoffset * escp2_max_hres(model, v) / ydpi) + microoffset);
      if (pos > 0)
	stp_zprintf(v, "\033(\\%c%c%c%c%c%c", 4, 0, 160, 5,
		    pos & 255, pos >> 8);
    }
}

static void
send_print_command(stp_softweave_t *sw, stp_pass_t *pass, int model, int color,
		   int lwidth, const stp_vars_t v, int hoffset, int ydpi,
		   int xdpi, int physical_xdpi, int nlines)
{
  if (!escp2_has_cap(model, MODEL_COMMAND, MODEL_COMMAND_PRO,v) &&
      sw->jets == 1 && sw->bitwidth == 1)
    {
      int ygap = 3600 / ydpi;
      int xgap = 3600 / xdpi;
      if (ydpi == 720 &&
	  escp2_has_cap(model, MODEL_720DPI_MODE, MODEL_720DPI_600, v))
	ygap *= 8;
      stp_zprintf(v, "\033.%c%c%c%c%c%c", COMPRESSION, ygap, xgap,
		  1, lwidth & 255, (lwidth >> 8) & 255);
    }
  else if (!escp2_has_cap(model, MODEL_COMMAND, MODEL_COMMAND_PRO,v) &&
	   escp2_has_cap(model, MODEL_VARIABLE_DOT, MODEL_VARIABLE_NORMAL, v))
    {
      int ygap = 3600 / ydpi;
      int xgap = 3600 / physical_xdpi;
      if (escp2_has_cap(model, MODEL_720DPI_MODE, MODEL_720DPI_600, v))
	ygap *= 8;
      else if (escp2_pseudo_separation_rows(model, v) > 0)
	ygap *= escp2_pseudo_separation_rows(model, v);
      else
	ygap *= sw->separation_rows;
      stp_zprintf(v, "\033.%c%c%c%c%c%c", COMPRESSION, ygap, xgap,
		  nlines, lwidth & 255, (lwidth >> 8) & 255);
    }
  else
    {
      int ncolor = (densities[color] << 4) | colors[color];
      int nwidth = sw->bitwidth * ((lwidth + 7) / 8);
      stp_zprintf(v, "\033i%c%c%c%c%c%c%c", ncolor, COMPRESSION,
		  sw->bitwidth, nwidth & 255, (nwidth >> 8) & 255,
		  nlines & 255, (nlines >> 8) & 255);
    }
}

static void
send_extra_data(stp_softweave_t *sw, stp_vars_t v, int extralines, int lwidth)
{
  int k = 0;
  for (k = 0; k < extralines; k++)
    {
      int bytes_to_fill = sw->bitwidth * ((lwidth + 7) / 8);
      int full_blocks = bytes_to_fill / 128;
      int leftover = bytes_to_fill % 128;
      int l = 0;
      while (l < full_blocks)
	{
	  stp_putc(129, v);
	  stp_putc(0, v);
	  l++;
	}
      if (leftover == 1)
	{
	  stp_putc(1, v);
	  stp_putc(0, v);
	}
      else if (leftover > 0)
	{
	  stp_putc(257 - leftover, v);
	  stp_putc(0, v);
	}
    }
}

static void
flush_pass(stp_softweave_t *sw, int passno, int model, int width,
	   int hoffset, int ydpi, int xdpi, int physical_xdpi,
	   int vertical_subpass)
{
  int j;
  const stp_vars_t v = (sw->v);
  escp2_privdata_t *pd = (escp2_privdata_t *) stp_get_driver_data(v);
  stp_lineoff_t *lineoffs = stp_get_lineoffsets_by_pass(sw, passno);
  stp_lineactive_t *lineactive = stp_get_lineactive_by_pass(sw, passno);
  const stp_linebufs_t *bufs = stp_get_linebases_by_pass(sw, passno);
  stp_pass_t *pass = stp_get_pass_by_pass(sw, passno);
  stp_linecount_t *linecount = stp_get_linecount_by_pass(sw, passno);
  int lwidth = (width + (sw->horizontal_weave - 1)) / sw->horizontal_weave;

  ydpi *= pd->undersample;

  if (ydpi > escp2_max_vres(model, v))
    ydpi = escp2_max_vres(model, v);
  for (j = 0; j < sw->ncolors; j++)
    {
      if (lineactive[0].v[j] > 0 ||
        escp2_has_cap(model, MODEL_MICROWEAVE_EXCEPTION,
                      MODEL_MICROWEAVE_EXCEPTION_BLACK, v))
	{
	  int nlines = linecount[0].v[j];
	  int minlines = pd->min_nozzles;
	  int extralines = 0;
	  if (nlines < minlines)
	    {
	      extralines = minlines - nlines;
	      nlines = minlines;
	    }
	  set_vertical_position(sw, pass, model, v);
	  set_color(sw, pass, model, v, j);
	  set_horizontal_position(sw, pass, model, v, hoffset, ydpi, xdpi,
				  vertical_subpass);
	  send_print_command(sw, pass, model, j, lwidth, v, hoffset, ydpi,
			     xdpi, physical_xdpi, nlines);

	  /*
	   * Send the data
	   */
	  stp_zfwrite((const char *)bufs[0].v[j], lineoffs[0].v[j], 1, v);
	  if (extralines)
	    send_extra_data(sw, v, extralines, lwidth);
	  stp_putc('\r', v);
	  pd->printed_something = 1;
	}
      lineoffs[0].v[j] = 0;
      linecount[0].v[j] = 0;
    }

  sw->last_pass = pass->pass;
  pass->pass = -1;
}
