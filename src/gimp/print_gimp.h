/*
 * "$Id: print_gimp.h,v 1.33 2002/11/28 05:39:13 rlk Exp $"
 *
 *   Print plug-in for the GIMP.
 *
 *   Copyright 1997-2000 Michael Sweet (mike@easysw.com),
 *	Robert Krawitz (rlk@alum.mit.edu). and Steve Miller (smiller@rni.net
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
 *
 * Revision History:
 *
 *   See ChangeLog
 */

#ifndef __PRINT_GIMP_H__
#define __PRINT_GIMP_H__

#ifdef __GNUC__
#define inline __inline__
#endif

#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#ifdef INCLUDE_GIMP_PRINT_H
#include INCLUDE_GIMP_PRINT_H
#else
#include <gimp-print/gimp-print.h>
#endif

/*
 * All Gimp-specific code is in this file.
 */

#define PLUG_IN_VERSION		VERSION " - " RELEASE_DATE
#define PLUG_IN_NAME		"Print"

#define ORIENT_AUTO             -1      /* Best orientation */
#define ORIENT_PORTRAIT         0       /* Portrait orientation */
#define ORIENT_LANDSCAPE        1       /* Landscape orientation */
#define ORIENT_UPSIDEDOWN       2       /* Reverse portrait orientation */
#define ORIENT_SEASCAPE         3       /* Reverse landscape orientation */

#define INVALID_TOP 1
#define INVALID_LEFT 2

typedef struct		/**** Printer List ****/
{
  int	active;			/* Do we know about this printer? */
  char	*name;			/* Name of printer */
  char  *output_to;
  float	scaling;		/* Scaling, percent of printable area */
  int   orientation;
  int	unit;			/* Units for preview area 0=Inch 1=Metric */
  int	invalid_mask;
  stp_vars_t v;
} gp_plist_t;

#define THUMBNAIL_MAXW	(128)
#define THUMBNAIL_MAXH	(128)

extern gint    thumbnail_w, thumbnail_h, thumbnail_bpp;
extern guchar *thumbnail_data;
extern gint    adjusted_thumbnail_bpp;
extern guchar *adjusted_thumbnail_data;
extern guchar *preview_thumbnail_data;

extern gint             plist_count;	   /* Number of system printers */
extern gint             plist_current;     /* Current system printer */
extern gp_plist_t         *plist;		  /* System printers */
extern const gchar     *image_filename;
extern stp_printer_t current_printer;
extern gint             runme;
extern gint             saveme;

extern GtkWidget *color_adjust_dialog;
extern GtkWidget *dither_algo_combo;
extern gp_plist_t *pv;

/*
 * Function prototypes
 */
extern void plist_set_output_to(gp_plist_t *p, const char *val);
extern void plist_set_output_to_n(gp_plist_t *p, const char *val, int n);
extern const char *plist_get_output_to(const gp_plist_t *p);
extern void plist_set_name(gp_plist_t *p, const char *val);
extern void plist_set_name_n(gp_plist_t *p, const char *val, int n);
extern const char *plist_get_name(const gp_plist_t *p);
extern void copy_printer(gp_plist_t *vd, const gp_plist_t *vs);

extern void  printrc_save (void);

extern int add_printer(const gp_plist_t *key, int add_only);
extern void initialize_printer(gp_plist_t *printer);
extern void update_adjusted_thumbnail (void);
extern void plist_build_combo         (GtkWidget     *combo,
				       stp_string_list_t items,
				       const gchar   *cur_item,
				       const gchar	  *def_value,
				       GtkSignalFunc callback,
				       gint          *callback_id,
				       gpointer	     data);

extern void invalidate_frame(void);
extern void invalidate_preview_thumbnail(void);
extern void do_color_updates    (void);
extern void redraw_color_swatch (void);
extern void build_dither_combo  (void);
extern void create_color_adjust_window  (void);
extern void update_adjusted_thumbnail   (void);
extern void create_main_window (void);
extern void set_color_sliders_active(int active);
extern void writefunc (void *file, const char *buf, size_t bytes);
extern void set_adjustment_tooltip(GtkObject *adjustment, const gchar *tip);
extern void set_help_data(GtkWidget *widget, const gchar *tooltip);
extern void table_attach_aligned(GtkTable *table, gint column, gint row,
				 const gchar *label_text, gfloat xalign,
				 gfloat yalign, GtkWidget *widget,
				 gint colspan, gboolean left_align);
extern gint compute_orientation(void);
extern void set_image_dimensions(gint width, gint height);
extern void set_image_resolution(gdouble xres, gdouble yres);
extern guchar *get_thumbnail_data(gint *width, gint *height, gint *bpp);

/* How to create an Image wrapping a Gimp drawable */
extern stp_image_t *Image_GimpDrawable_new(GimpDrawable *drawable);
extern void Image_transpose(stp_image_t *image);
extern void Image_hflip(stp_image_t *image);
extern void Image_vflip(stp_image_t *image);
extern void Image_crop(stp_image_t *image, int left, int top,
		       int right, int bottom);
extern void Image_rotate_ccw(stp_image_t *image);
extern void Image_rotate_cw(stp_image_t *image);
extern void Image_rotate_180(stp_image_t *image);

#endif  /* __PRINT_GIMP_H__ */
