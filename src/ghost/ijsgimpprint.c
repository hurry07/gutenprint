/*
 *  $Id: ijsgimpprint.c,v 1.37 2003/02/22 17:20:20 rlk Exp $
 *
 *   ijs server for gimp-print.
 *
 *   Copyright 2001 Robert Krawitz (rlk@alum.mit.edu)
 *
 *   Originally written by Russell Lang, copyright assigned to Robert Krawitz.
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
 * Revision History:
 *
 *   See ChangeLog
 */

/* ijs server for gimp-print */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <gimp-print/gimp-print.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <locale.h>
#include <ijs.h>
#include <ijs_server.h>
#include <errno.h>
#include <gimp-print/gimp-print-intl-internal.h>

static int stp_debug = 0;

#define STP_DEBUG(x) do { if (stp_debug || getenv("STP_DEBUG")) x; } while (0)

typedef struct _GimpParamList GimpParamList;

struct _GimpParamList {
  GimpParamList *next;
  char *key;
  char *value;
  int value_size;
};

typedef struct _IMAGE
{
  IjsServerCtx *ctx;
  stp_vars_t v;
  char *filename;	/* OutputFile */
  int fd;		/* OutputFD + 1 (so that 0 is invalid) */
  int width;		/* pixels */
  int height;		/* pixels */
  int bps;		/* bytes per sample */
  int n_chan;		/* number of channels */
  int xres;		/* dpi */
  int yres;
  int output_type;
  int monochrome_flag;	/* for monochrome output */
  int row;		/* row number in buffer */
  int row_width;	/* length of a row */
  char *row_buf;	/* buffer for raster */
  double total_bytes;	/* total size of raster */
  double bytes_left;	/* bytes remaining to be read */
  GimpParamList *params;
} IMAGE;

static const char DeviceGray[] = "DeviceGray";
static const char DeviceRGB[] = "DeviceRGB";
static const char DeviceCMYK[] = "DeviceCMYK";

static char *
c_strdup(const char *s)
{
  char *ret = malloc(strlen(s) + 1);
  strcpy(ret, s);
  return ret;
}

static int
image_init(IMAGE *img, IjsPageHeader *ph)
{
  img->width = ph->width;
  img->height = ph->height;
  img->bps = ph->bps;
  img->n_chan = ph->n_chan;
  img->xres = ph->xres;
  img->yres = ph->yres;

  img->row = -1;
  img->row_width = (ph->n_chan * ph->bps * ph->width + 7) >> 3;
  img->row_buf = (char *)malloc(img->row_width);
  STP_DEBUG(fprintf(stderr, "image_init\n"));
  STP_DEBUG(fprintf(stderr,
		    "ph width %d height %d bps %d n_chan %d xres %f yres %f\n",
		    ph->width, ph->height, ph->bps, ph->n_chan, ph->xres,
		    ph->yres));

  if ((img->bps == 1) && (img->n_chan == 1) &&
      (strncmp(ph->cs, DeviceGray, strlen(DeviceGray)) == 0))
    {
      stp_parameter_t desc;
      STP_DEBUG(fprintf(stderr, "output monochrome\n"));
      img->output_type = OUTPUT_GRAY;
      stp_describe_parameter(img->v, "Contrast", &desc);
      if (desc.p_type == STP_PARAMETER_TYPE_DOUBLE)
	stp_set_float_parameter(img->v, "Contrast", desc.bounds.dbl.upper);
      stp_parameter_description_free(&desc);
      img->monochrome_flag = 1;
      /* 8-bit greyscale */
    }
  else if ((img->bps == 8) && (img->n_chan == 1) &&
      (strncmp(ph->cs, DeviceGray, strlen(DeviceGray)) == 0))
    {
      STP_DEBUG(fprintf(stderr, "output gray\n"));
      img->output_type = OUTPUT_GRAY;
      img->monochrome_flag = 0;
      /* 8-bit greyscale */
    }
  else if ((img->bps == 8) && (img->n_chan == 3) &&
	   (strncmp(ph->cs, DeviceRGB, strlen(DeviceRGB)) == 0))
    {
      STP_DEBUG(fprintf(stderr, "output color\n"));
      img->output_type = OUTPUT_COLOR;
      img->monochrome_flag = 0;
      /* 24-bit colour */
    }
  else if ((img->bps == 8) && (img->n_chan == 4) &&
	   (strncmp(ph->cs, DeviceCMYK, strlen(DeviceCMYK)) == 0))
    {
      STP_DEBUG(fprintf(stderr, "output CMYK\n"));
      img->output_type = OUTPUT_RAW_CMYK;
      img->monochrome_flag = 0;
      /* 32-bit CMYK colour */
    }
  else
    {
      fprintf(stderr, _("Bad color space: bps %d channels %d space %s\n"),
		img->bps, img->n_chan, ph->cs);
      /* unsupported */
      return -1;
    }

  if (img->row_buf == NULL)
    {
      STP_DEBUG(fprintf(stderr, _("No row buffer\n")));
      return -1;
    }

  return 0;
}

static void
image_finish(IMAGE *img)
{
  if (img->row_buf)
    free(img->row_buf);
  img->row_buf = NULL;
}

static double
get_float(const char *str, const char *name, double *pval)
{
  float new_value;
  /* Force locale to "C", because decimal numbers coming from the IJS
     client are always with a decimal point, nver with a decimal comma */
  setlocale(LC_ALL, "C");
  if (sscanf(str, "%f", &new_value) == 1)
    {
      setlocale(LC_ALL, "");
      *pval = new_value;
      return 0;
    }
  else
    {
      setlocale(LC_ALL, "");
      fprintf(stderr, _("Unable to parse parameter %s=%s (expect a number)\n"),
	      name, str);
      return -1;
    }
}

static int
get_int(const char *str, const char *name, int *pval)
{
  int new_value;
  /* Force locale to "C", because decimal numbers sent to the IJS
     client must have a decimal point, nver a decimal comma */
  setlocale(LC_ALL, "C");
  if (sscanf(str, "%d", &new_value) == 1)
    {
      setlocale(LC_ALL, "");
      *pval = new_value;
      return 0;
    }
  else
    {
      setlocale(LC_ALL, "");
      fprintf(stderr, _("Unable to parse parameter %s=%s (expect a number)\n"),
	      name, str);
      return -1;
    }
}

/* A C implementation of /^(\d\.+\-eE)+x(\d\.+\-eE)+$/ */
static int
gimp_parse_wxh (const char *val, int size,
		   double *pw, double *ph)
{
  char buf[256];
  char *tail;
  int i;

  for (i = 0; i < size; i++)
    if (val[i] == 'x')
      break;

  if (i + 1 >= size)
    return IJS_ESYNTAX;

  if (i >= sizeof(buf))
    return IJS_EBUF;

  memcpy (buf, val, i);
  buf[i] = 0;
  /* Force locale to "C", because decimal numbers coming from the IJS
     client are always with a decimal point, nver with a decimal comma */
  setlocale(LC_ALL, "C");
  *pw = strtod (buf, &tail);
  setlocale(LC_ALL, "");
  if (tail == buf)
    return IJS_ESYNTAX;

  if (size - i > sizeof(buf))
    return IJS_EBUF;

  memcpy (buf, val + i + 1, size - i - 1);
  buf[size - i - 1] = 0;
  /* Force locale to "C", because decimal numbers coming from the IJS
     client are always with a decimal point, nver with a decimal comma */
  setlocale(LC_ALL, "C");
  *ph = strtod (buf, &tail);
  setlocale(LC_ALL, "");
  if (tail == buf)
    return IJS_ESYNTAX;

  return 0;
}

/**
 * gimp_find_key: Search parameter list for key.
 *
 * @key: key to look up
 *
 * Return value: GimpParamList entry matching @key, or NULL.
 **/
static GimpParamList *
gimp_find_key (GimpParamList *pl, const char *key)
{
  GimpParamList *curs;

  for (curs = pl; curs != NULL; curs = curs->next)
    {
      if (!strcmp (curs->key, key))
	return curs;
    }
  return NULL;
}

static int
gimp_status_cb (void *status_cb_data,
		IjsServerCtx *ctx,
		IjsJobId job_id)
{
  return 0;
}

static int
gimp_list_cb (void *list_cb_data,
	      IjsServerCtx *ctx,
	      IjsJobId job_id,
	      char *val_buf,
	      int val_size)
{
  const char *param_list = "OutputFile,OutputFD,DeviceManufacturer,DeviceModel,Quality,MediaName,MediaType,MediaSource,InkType,DitherAlgorithm,ImageOptimization,Brightness,Gamma,Contrast,Cyan,Magenta,Yellow,Saturation,Density,PrintableArea,PrintableTopLeft,TopLeft,Dpi";
  int size = strlen (param_list);

  if (size > val_size)
    return IJS_EBUF;

  memcpy (val_buf, param_list, size);
  return size;
}

static int
gimp_enum_cb (void *enum_cb_data,
	      IjsServerCtx *ctx,
	      IjsJobId job_id,
	      const char *key,
	      char *val_buf,
	      int val_size)
{
  const char *val = NULL;
  if (!strcmp (key, "ColorSpace"))
    val = "DeviceRGB,DeviceGray,DeviceCMYK";
  else if (!strcmp (key, "DeviceManufacturer"))
    val = "Gimp-Print";
  else if (!strcmp (key, "DeviceModel"))
    val = "gimp-print";
  else if (!strcmp (key, "PageImageFormat"))
    val = "Raster";

  if (val == NULL)
    return IJS_EUNKPARAM;
  else
    {
      int size = strlen (val);

      if (size > val_size)
	return IJS_EBUF;
      memcpy (val_buf, val, size);
      return size;
    }
}

static int
gimp_get_cb (void *get_cb_data,
	     IjsServerCtx *ctx,
	     IjsJobId job_id,
	     const char *key,
	     char *val_buf,
	     int val_size)
{
  IMAGE *img = (IMAGE *)get_cb_data;
  stp_vars_t v = img->v;
  stp_printer_t printer = stp_get_printer(v);
  GimpParamList *pl = img->params;
  GimpParamList *curs;
  const char *val = NULL;
  char buf[256];

  STP_DEBUG(fprintf(stderr, "gimp_get_cb: %s\n", key));
  if (!printer)
    {
      if (strlen(stp_get_driver(v)) == 0)
	fprintf(stderr, _("Printer must be specified with -sModel\n"));
      else
	fprintf(stderr, _("Printer %s is not a known model\n"),
		stp_get_driver(v));
      return IJS_EUNKPARAM;
    }
  curs = gimp_find_key (pl, key);
  if (curs != NULL)
    {
      if (curs->value_size > val_size)
	return IJS_EBUF;
      memcpy (val_buf, curs->value, curs->value_size);
      return curs->value_size;
    }

  if (!strcmp(key, "PrintableArea"))
    {
      int l, r, b, t;
      int h, w;
      stp_get_imageable_area(v, &l, &r, &b, &t);
      h = b - t;
      w = r - l;
      /* Force locale to "C", because decimal numbers sent to the IJS
	 client must have a decimal point, nver a decimal comma */
      setlocale(LC_ALL, "C");
      sprintf(buf, "%gx%g", (double) w / 72.0, (double) h / 72.0);
      setlocale(LC_ALL, "");
      STP_DEBUG(fprintf(stderr, "PrintableArea %d %d %s\n", h, w, buf));
      val = buf;
    }
  else if (!strcmp(key, "Dpi"))
    {
      int x, y;
      stp_describe_resolution(v, &x, &y);
      /* Force locale to "C", because decimal numbers sent to the IJS
	 client must have a decimal point, nver a decimal comma */
      setlocale(LC_ALL, "C");
      sprintf(buf, "%d", x);
      setlocale(LC_ALL, "");
      STP_DEBUG(fprintf(stderr, "Dpi %d %d (%d) %s\n", x, y, x, buf));
      val = buf;
    }
  else if (!strcmp(key, "PrintableTopLeft"))
    {
      int l, r, b, t;
      int h, w;
      stp_get_media_size(v, &w, &h);
      stp_get_imageable_area(v, &l, &r, &b, &t);
      /* Force locale to "C", because decimal numbers sent to the IJS
	 client must have a decimal point, nver a decimal comma */
      setlocale(LC_ALL, "C");
      sprintf(buf, "%gx%g", (double) l / 72.0, (double) t / 72.0);
      setlocale(LC_ALL, "");
      STP_DEBUG(fprintf(stderr, "PrintableTopLeft %d %d %s\n", t, l, buf));
      val = buf;
    }
  else if (!strcmp (key, "DeviceManufacturer"))
    val = "Gimp-Print";
  else if (!strcmp (key, "DeviceModel"))
    val = stp_get_driver(img->v);
  else if (!strcmp (key, "PageImageFormat"))
    val = "Raster";

  if (val == NULL)
    return IJS_EUNKPARAM;
  else
    {
      int size = strlen (val);

      if (size > val_size)
	return IJS_EBUF;
      memcpy (val_buf, val, size);
      return size;
    }
}

static int
gimp_set_cb (void *set_cb_data, IjsServerCtx *ctx, IjsJobId jobid,
	     const char *key, const char *value, int value_size)
{
  int code = 0;
  char vbuf[256];
  int i;
  double z;
  IMAGE *img = (IMAGE *)set_cb_data;
  STP_DEBUG(fprintf (stderr, "gimp_set_cb: %s=", key));
  STP_DEBUG(fwrite (value, 1, value_size, stderr));
  STP_DEBUG(fputs ("\n", stderr));
  if (value_size > sizeof(vbuf)-1)
    return -1;
  memset(vbuf, 0, sizeof(vbuf));
  memcpy(vbuf, value, value_size);

  if (strcmp(key, "OutputFile") == 0)
    {
      if (img->filename)
	free(img->filename);
      img->filename = c_strdup(vbuf);
    }
  else if (strcmp(key, "OutputFD") == 0) {
    /* Force locale to "C", because decimal numbers sent to the IJS
       client must have a decimal point, nver a decimal comma */
    setlocale(LC_ALL, "C");
    img->fd = atoi(vbuf) + 1;
    setlocale(LC_ALL, "");
  } else if (strcmp(key, "DeviceManufacturer") == 0)
    ;				/* We don't care who makes it */
  else if (strcmp(key, "DeviceModel") == 0)
    {
      stp_printer_t printer = stp_get_printer_by_driver(vbuf);
      stp_set_driver(img->v, vbuf);
      if (printer &&
	  strcmp(stp_printer_get_family(printer), "ps") != 0 &&
	  strcmp(stp_printer_get_family(printer), "raw") != 0)
	stp_set_printer_defaults(img->v, printer);
      else
	code = IJS_ERANGE;
    }
  else if (strcmp(key, "Quality") == 0)
    stp_set_string_parameter(img->v, "Resolution", vbuf);
  else if (strcmp(key, "TopLeft") == 0)
    {
      int l, r, b, t, pw, ph;
      double w, h;
      stp_get_imageable_area(img->v, &l, &r, &b, &t);
      stp_get_media_size(img->v, &pw, &ph);
      STP_DEBUG(fprintf(stderr, "l %d r %d t %d b %d pw %d ph %d\n",
			l, r, t, b, pw, ph));
      code = gimp_parse_wxh(vbuf, strlen(vbuf), &w, &h);
      if (code == 0)
	{
	  int al = (w * 72);
	  int ah = (h * 72);
	  STP_DEBUG(fprintf(stderr, "left top %f %f %d %d %s\n",
			    w * 72, h * 72, al, ah, vbuf));
	  if (al >= 0)
	    stp_set_left(img->v, al);
	  if (ah >= 0)
	    stp_set_top(img->v, ah);
	}
    }
  else if (strcmp(key, "PaperSize") == 0)
    {
      double w, h;
      code = gimp_parse_wxh(vbuf, strlen(vbuf), &w, &h);
      if (code == 0)
	{
	  const stp_papersize_t *p;
	  w *= 72;
	  h *= 72;
	  STP_DEBUG(fprintf(stderr, "paper size %f %f %s\n", w, h, vbuf));
	  stp_set_page_width(img->v, w);
	  stp_set_page_height(img->v, h);
	  if ((p = stp_get_papersize_by_size(h, w)) != NULL)
	    {
	      STP_DEBUG(fprintf(stderr, "Found page size %s\n", p->name));
	      stp_set_string_parameter(img->v, "PageSize", p->name);
	    }
	  else
	    STP_DEBUG(fprintf(stderr, "No matching paper size found\n"));
	}
    }
  else if (strcmp (key, "Duplex") == 0)
    {
    }
  else if (strcmp (key, "PS:Duplex") == 0)
    {
    }
  else if (strcmp (key, "Tumble") == 0)
    {
    }
  else if (strcmp (key, "PS:Tumble") == 0)
    {
    }
  else
    {
      stp_curve_t curve;
      stp_parameter_t desc;
      stp_describe_parameter(img->v, key, &desc);
      switch (desc.p_type)
	{
	case STP_PARAMETER_TYPE_STRING_LIST:
	  stp_set_string_parameter(img->v, key, vbuf);
	  break;
	case STP_PARAMETER_TYPE_FILE:
	  stp_set_file_parameter(img->v, key, vbuf);
	  break;
	case STP_PARAMETER_TYPE_CURVE:
	  curve = stp_curve_create_read_string(vbuf);
	  if (curve)
	    stp_set_curve_parameter(img->v, key, curve);
	  stp_curve_free(curve);
	  break;
	case STP_PARAMETER_TYPE_DOUBLE:
	  if (! img->monochrome_flag || strcmp (key, "Contrast") != 0)
	    {
	      code = get_float(vbuf, key, &z);
	      if (code == 0)
		stp_set_float_parameter(img->v, key, z);
	    }
	case STP_PARAMETER_TYPE_INT:
	  code = get_int(vbuf, key, &i);
	  if (code == 0)
	    stp_set_int_parameter(img->v, key, i);
	case STP_PARAMETER_TYPE_BOOLEAN:
	  code = get_int(vbuf, key, &i);
	  if (code == 0)
	    stp_set_boolean_parameter(img->v, key, i);
	default:
	  STP_DEBUG(fprintf(stderr, "Bad parameter %s %d\n", key, desc.p_type));
	}
      stp_parameter_description_free(&desc);
    }

  if (code == 0)
    {
      GimpParamList *pl = gimp_find_key (img->params, key);

      if (pl == NULL)
	{
	  pl = (GimpParamList *)malloc (sizeof (GimpParamList));
	  pl->next = img->params;
	  pl->key = malloc (strlen(key) + 1);
	  memcpy (pl->key, key, strlen(key) + 1);
	  img->params = pl;
	}
      else
	{
	  free (pl->value);
	}
      pl->value = malloc (value_size);
      memcpy (pl->value, value, value_size);
      pl->value_size = value_size;
    }

  return code;
}

/**********************************************************/

static void
gimp_outfunc(void *data, const char *buffer, size_t bytes)
{
  if ((data != NULL) && (buffer != NULL) && (bytes != 0))
    fwrite(buffer, 1, bytes, (FILE *)data);
}

/**********************************************************/
/* stp_image_t functions */

/* bytes per pixel (NOT bits per pixel) */
static int
gimp_image_bpp(stp_image_t *image)
{
  IMAGE *img = (IMAGE *)(image->rep);
  STP_DEBUG(fprintf(stderr, "gimp_image_bpp: bps=%d n_chan=%d returning %d\n",
		    img->bps, img->n_chan, (img->bps * img->n_chan + 7) / 8));
  return (img->bps * img->n_chan + 7) / 8;
}

static int
gimp_image_width(stp_image_t *image)
{
  IMAGE *img = (IMAGE *)(image->rep);
  return img->width;
}

static int
gimp_image_height(stp_image_t *image)
{
  IMAGE *img = (IMAGE *)(image->rep);
  return img->height * img->xres / img->yres;
}

static int
image_next_row(IMAGE *img)
{
  int status = 0;
  double n_bytes = img->bytes_left;
  if (img->bytes_left)
    {

      if (n_bytes > img->row_width)
	n_bytes = img->row_width;
#ifdef VERBOSE
      STP_DEBUG(fprintf(stderr, "%.0f bytes left, reading %.d, on row %d\n",
			img->bytes_left, (int) n_bytes, img->row));
#endif
      status = ijs_server_get_data(img->ctx, img->row_buf, (int) n_bytes);
      if (status)
	{
	  STP_DEBUG(fprintf(stderr, "page aborted!\n"));
	}
      else
	{
	  img->row++;
	  img->bytes_left -= n_bytes;
	}
    }
  else
    return 1;	/* Done */
  return status;
}

static stp_image_status_t
gimp_image_get_row(stp_image_t *image, unsigned char *data, size_t byte_limit,
		   int row)
{
  IMAGE *img = (IMAGE *)(image->rep);
  int physical_row = row * img->yres / img->xres;

  if ((physical_row < 0) || (physical_row >= img->height))
    return STP_IMAGE_STATUS_ABORT;

  /* Read until we reach the requested row. */
  while (physical_row > img->row)
    {
      if (image_next_row(img))
	return STP_IMAGE_STATUS_ABORT;
    }

  if (physical_row == img->row)
    {
      unsigned i, j, length;
      switch (img->bps)
	{
	case 8:
	  memcpy(data, img->row_buf, img->row_width);
	  break;
	case 1:
	  length = img->width / 8;
	  for (i = 0; i < length; i++)
	    for (j = 128; j > 0; j >>= 1)
	      {
		if (img->row_buf[i] & j)
		  data[0] = 255;
		else
		  data[0] = 0;
		data++;
	      }
	  length = img->width % 8;
	  for (j = 128; j > 1 << (7 - length); j >>= 1)
	    {
	      if (img->row_buf[i] & j)
		data[0] = 255;
	      else
		data[0] = 0;
	      data++;
	    }
	  break;
	default:
	  return STP_IMAGE_STATUS_ABORT;
	}
    }
  else
    return STP_IMAGE_STATUS_ABORT;
  return STP_IMAGE_STATUS_OK;
}


static const char *
gimp_image_get_appname(stp_image_t *image)
{
  return "ijsgimp";
}

static void
gimp_image_note_progress(stp_image_t *image, double current, double total)
{
  char buf[256];
  sprintf(buf, _("%.0f of %.0f\n"), current, total);
  STP_DEBUG(gimp_outfunc(stderr, buf, strlen(buf)));
}

/**********************************************************/

static const char *
safe_get_string_parameter(const stp_vars_t v, const char *param)
{
  const char *val = stp_get_string_parameter(v, param);
  if (val)
    return val;
  else
    return "NULL";
}

static void
stp_dbg(const char *msg, const stp_vars_t v)
{
  fprintf(stderr, "Settings: Model %s\n", stp_get_driver(v));
  fprintf(stderr,"%s Settings: c: %f  m: %f  y: %f\n", msg,
	  stp_get_float_parameter(v, "Cyan"),
	  stp_get_float_parameter(v, "Magenta"),
	  stp_get_float_parameter(v, "Yellow"));
  fprintf(stderr,"Settings: bright: %f  contrast: %f\n",
	  stp_get_float_parameter(v, "Brightness"),
	  stp_get_float_parameter(v, "Contrast"));
  fprintf(stderr,"Settings: Gamma: %f  Saturation: %f  Density: %f\n",
	  stp_get_float_parameter(v, "Gamma"),
	  stp_get_float_parameter(v, "Saturation"),
	  stp_get_float_parameter(v, "Density"));
  fprintf(stderr, "Settings: width %d, height %d\n",
	  stp_get_page_width(v), stp_get_page_height(v));
  fprintf(stderr, "Settings: output type %d\n", stp_get_output_type(v));
  fprintf(stderr, "Settings: Image Type %s\n",
	  safe_get_string_parameter(v, "ImageOptimization"));
  fprintf(stderr, "Settings: Quality %s\n",
	  safe_get_string_parameter(v, "Resolution"));
  fprintf(stderr, "Settings: Dither %s\n",
	  safe_get_string_parameter(v, "DitherAlgorithm"));
  fprintf(stderr, "Settings: MediaSource %s\n",
	  safe_get_string_parameter(v, "InputSlot"));
  fprintf(stderr, "Settings: MediaType %s\n",
	  safe_get_string_parameter(v, "MediaType"));
  fprintf(stderr, "Settings: MediaSize %s\n",
	  safe_get_string_parameter(v, "PageSize"));
  fprintf(stderr, "Settings: InkType %s\n",
	  safe_get_string_parameter(v, "InkType"));
}

int
main (int argc, char **argv)
{
  IjsPageHeader ph;
  int status;
  int page = 0;
  IMAGE img;
  stp_image_t si;
  stp_printer_t printer = NULL;
  FILE *f = NULL;
  int l, t, r, b, w, h;
  int width, height;

  memset(&img, 0, sizeof(img));

  img.ctx = ijs_server_init();
  if (img.ctx == NULL)
    return 1;

  stp_init();
  img.v = stp_vars_create();
  if (img.v == NULL)
    {
      ijs_server_done(img.ctx);
      return 1;
    }
  stp_set_top(img.v, 0);
  stp_set_left(img.v, 0);

  /* Error messages to stderr. */
  stp_set_errfunc(img.v, gimp_outfunc);
  stp_set_errdata(img.v, stderr);

  /* Printer data goes to file f, but we haven't opened it yet. */
  stp_set_outfunc(img.v, gimp_outfunc);
  stp_set_outdata(img.v, NULL);

  memset(&si, 0, sizeof(si));
  si.bpp = gimp_image_bpp;
  si.width = gimp_image_width;
  si.height = gimp_image_height;
  si.get_row = gimp_image_get_row;
  si.get_appname = gimp_image_get_appname;
  si.note_progress = gimp_image_note_progress;
  si.rep = &img;

  ijs_server_install_status_cb (img.ctx, gimp_status_cb, &img);
  ijs_server_install_list_cb (img.ctx, gimp_list_cb, &img);
  ijs_server_install_enum_cb (img.ctx, gimp_enum_cb, &img);
  ijs_server_install_get_cb (img.ctx, gimp_get_cb, &img);
  ijs_server_install_set_cb(img.ctx, gimp_set_cb, &img);

  STP_DEBUG(stp_dbg("about to start", img.v));

  do
    {

      status = ijs_server_get_page_header(img.ctx, &ph);
      if (status)
	{
	  if (status < 0)
	    fprintf(stderr, _("ijs_server_get_page_header failed %d\n"),
		    status);
	  break;
	}
      STP_DEBUG(fprintf(stderr, "got page header, %d x %d\n",
			ph.width, ph.height));
      STP_DEBUG(stp_dbg("have page header", img.v));

      status = image_init(&img, &ph);
      if (status)
	{
	  fprintf(stderr, _("image_init failed %d\n"), status);
	  break;
	}

      if (page == 0)
	{
	  if (img.fd)
	    {
	      f = fdopen(img.fd - 1, "wb");
	      if (!f)
		{
		  fprintf(stderr, _("Unable to open file descriptor: %s\n"),
			  strerror(errno));
		  status = -1;
		  break;
		}
	    }
	  else if (img.filename && strlen(img.filename) > 0)
	    {
	      f = fopen(img.filename, "wb");
	      if (!f)
		{
		  status = -1;
		  fprintf(stderr, _("Unable to open %s: %s\n"), img.filename,
			  strerror(errno));
		  break;
		}
	    }

	  /* Printer data to file */
	  stp_set_outdata(img.v, f);

	  printer = stp_get_printer(img.v);
	  if (printer == NULL)
	    {
	      fprintf(stderr, _("Unknown printer %s\n"),
		      stp_get_driver(img.v));
	      status = -1;
	      break;
	    }
	  stp_merge_printvars(img.v, stp_printer_get_defaults(printer));
	}


      img.total_bytes = (double) ((ph.n_chan * ph.bps * ph.width + 7) >> 3)
	* (double) ph.height;
      img.bytes_left = img.total_bytes;

      stp_set_float_parameter(img.v, "AppGamma", 1.7);
      stp_set_output_type(img.v, img.output_type);
      stp_get_media_size(img.v, &w, &h);
      stp_get_imageable_area(img.v, &l, &r, &b, &t);
      width = r - l;
      stp_set_width(img.v, width);
      height = b - t;
      stp_set_height(img.v, height);
      stp_set_page_number(img.v, page);
      stp_set_job_mode(img.v, STP_JOB_MODE_JOB);
      STP_DEBUG(stp_dbg("about to print", img.v));
      if (stp_verify(img.v))
	{
	  if (page == 0)
	    stp_start_job(img.v, &si);
	  stp_print(img.v, &si);
	}
      else
	{
	  fprintf(stderr, _("Bad parameters; cannot continue!\n"));
	  status = -1;
	  break;
	}

      while (img.bytes_left)
	{
	  status = image_next_row(&img);
	  if (status)
	    {
	      fprintf(stderr, _("Get next row failed at %.0f\n"),
		      img.bytes_left);
	      break;
	    }
	}

      image_finish(&img);
      page++;
    }
  while (status == 0);
  stp_end_job(img.v, &si);

  if (f)
    {
      fclose(f);
    }

  if (status > 0)
    status = 0; /* normal exit */

  ijs_server_done(img.ctx);

  STP_DEBUG(fprintf (stderr, "server exiting with status %d\n", status));
  return status;
}
