/*
 * "$Id: gtk_color_window.c,v 1.2 2001/02/02 01:25:33 rleigh Exp $"
 *
 *   Main window code for Print plug-in for the GIMP.
 *
 *   Copyright 1997-2000 Michael Sweet (mike@easysw.com),
 *	Robert Krawitz (rlk@alum.mit.edu), and Steve Miller (smiller@rni.net)
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "../../lib/libprintut.h"

#include "print_gimp.h"

#ifndef NEW_UI_ONLY

#include "print-intl.h"

extern stp_vars_t vars;
extern int plist_count;	     /* Number of system printers */
extern int plist_current;    /* Current system printer */
extern stp_plist_t  *plist;       /* System printers */

GtkWidget* gtk_color_adjust_dialog;

static GtkWidget* brightness_scale;	/* Scale for brightness */
static GtkWidget* brightness_entry;	/* Text entry widget for brightness */
static GtkWidget* saturation_scale;	/* Scale for saturation */
static GtkWidget* saturation_entry;	/* Text entry widget for saturation */
static GtkWidget* density_scale;	/* Scale for density */
static GtkWidget* density_entry;	/* Text entry widget for density */
static GtkWidget* contrast_scale;	/* Scale for contrast */
static GtkWidget* contrast_entry;	/* Text entry widget for contrast */
static GtkWidget* cyan_scale;		/* Scale for cyan */
static GtkWidget* cyan_entry;		/* Text entry widget for cyan */
static GtkWidget* magenta_scale;		/* Scale for magenta */
static GtkWidget* magenta_entry;		/* Text entry widget for magenta */
static GtkWidget* yellow_scale;		/* Scale for yellow */
static GtkWidget* yellow_entry;		/* Text entry widget for yellow */
static GtkWidget* gamma_scale;		/* Scale for gamma */
static GtkWidget* gamma_entry;         /* Text entry widget for gamma */
static GtkWidget* dismiss_button;      /* Action area dismiss button */
static GtkWidget* dither_algo_button;  /* Button for dither type menu */
GtkWidget* dither_algo_menu = NULL;  /* dither menu */

static GtkObject* brightness_adjustment;  /* Adjustment object for brightness */
static GtkObject* saturation_adjustment;  /* Adjustment object for saturation */
static GtkObject* density_adjustment;	   /* Adjustment object for density */
static GtkObject* contrast_adjustment;	   /* Adjustment object for contrast */
static GtkObject* cyan_adjustment;	   /* Adjustment object for cyan */
static GtkObject* magenta_adjustment;	   /* Adjustment object for magenta */
static GtkObject* yellow_adjustment;	   /* Adjustment object for yellow */
static GtkObject* gamma_adjustment;	   /* Adjustment object for gamma */


static void gtk_brightness_update(GtkAdjustment *);
static void gtk_brightness_callback(GtkWidget *);
static void gtk_saturation_update(GtkAdjustment *);
static void gtk_saturation_callback(GtkWidget *);
static void gtk_density_update(GtkAdjustment *);
static void gtk_density_callback(GtkWidget *);
static void gtk_contrast_update(GtkAdjustment *);
static void gtk_contrast_callback(GtkWidget *);
static void gtk_cyan_update(GtkAdjustment *);
static void gtk_cyan_callback(GtkWidget *);
static void gtk_magenta_update(GtkAdjustment *);
static void gtk_magenta_callback(GtkWidget *);
static void gtk_yellow_update(GtkAdjustment *);
static void gtk_yellow_callback(GtkWidget *);
static void gtk_gamma_update(GtkAdjustment *);
static void gtk_gamma_callback(GtkWidget *);
static void gtk_close_adjust_callback(void);
static void gtk_dither_algo_callback(GtkWidget *, gint);
void gtk_build_dither_menu(void);


/*****************************************************************************
 *
 * gtk_create_color_adjust_window(void)
 *
 * NOTES:
 *   creates the color adjuster popup, allowing the user to adjust brightness,
 *   contrast, saturation, etc.
 *****************************************************************************/
void gtk_create_color_adjust_window(void)
{
    GtkWidget*  dialog;
    GtkWidget*  table;
    GtkWidget*  label;
    GtkWidget*  hbbox;       /* Button box for Dismiss button */
    GtkWidget*  box;            /* Box container */
    GtkWidget*  scale;  /* Scale widget */
    GtkWidget*  entry;  /* Text entry widget */
    const stp_vars_t *lower = stp_minimum_settings();
    const stp_vars_t *upper = stp_maximum_settings();
    const stp_vars_t *defvars = stp_default_settings();

    GtkObject*  scale_data;  /* Scale data (limits) */
    char s[100]; /* Text string */

    /***
     * Create dialog widget
     ***/
    gtk_color_adjust_dialog = dialog = gtk_dialog_new();

    gtk_window_set_title(GTK_WINDOW(dialog), _("Print Color Adjust"));

    gtk_window_set_wmclass(GTK_WINDOW(dialog), "print", "Gimp");
    gtk_window_position(GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE);
    gtk_container_border_width(GTK_CONTAINER(dialog), 0);
    gtk_signal_connect(GTK_OBJECT(dialog),
		       "destroy",
		       (GtkSignalFunc)gtk_close_adjust_callback,
		       NULL);

    /*
     * Top-level table for dialog...
     */
    table = gtk_table_new(8, 4, FALSE);
    gtk_container_border_width(GTK_CONTAINER(table), 6);
    gtk_table_set_col_spacings(GTK_TABLE(table), 4);
    gtk_table_set_row_spacings(GTK_TABLE(table), 8);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
		       table,
		       FALSE,
		       FALSE,
		       0);
    gtk_widget_show(table);

    /***
     * Brightness slider...
     ***/
    label = gtk_label_new(_("Brightness:"));
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table),
		     label,
		     0, 1,  /* horiz */
		     0, 1,  /* vert */
		     GTK_FILL,
		     GTK_FILL,
		     0, 0);
    gtk_widget_show(label);

    box = gtk_hbox_new(FALSE, 8);
    gtk_table_attach(GTK_TABLE(table),
		     box,
		     1, 4,
		     0, 1,
		     GTK_FILL, GTK_FILL,
		     0, 0);
    gtk_widget_show(box);

    brightness_adjustment = scale_data =
	gtk_adjustment_new(vars.brightness, lower->brightness,
			   upper->brightness, defvars->brightness / 100,
			   defvars->brightness / 100,
			   defvars->brightness / 100);

    gtk_signal_connect(GTK_OBJECT(scale_data),
		       "value_changed",
		       (GtkSignalFunc)gtk_brightness_update,
		       NULL);
    /* Text part. */
    scale = brightness_scale = gtk_hscale_new(GTK_ADJUSTMENT(scale_data));
    gtk_box_pack_start(GTK_BOX(box), scale, FALSE, FALSE, 0);
    gtk_widget_set_usize(scale, 200, 0);
    gtk_scale_set_draw_value(GTK_SCALE(scale), FALSE);
    gtk_range_set_update_policy(GTK_RANGE(scale), GTK_UPDATE_CONTINUOUS);
    gtk_widget_show(scale);

    brightness_entry = entry = gtk_entry_new();
    sprintf(s, "%5.3f", vars.brightness);
    gtk_entry_set_text(GTK_ENTRY(entry), s);
    gtk_signal_connect(GTK_OBJECT(entry),
		       "changed",
		       (GtkSignalFunc)gtk_brightness_callback,
		       NULL);
    gtk_box_pack_start(GTK_BOX(box), entry, FALSE, FALSE, 0);
    gtk_widget_set_usize(entry, 40, 0);
    gtk_widget_show(entry);

    /***
     * Contrast slider...
     ***/
    gtk_container_set_border_width(GTK_CONTAINER(GTK_DIALOG(dialog)->
						 action_area),
				   2);
    gtk_box_set_homogeneous (GTK_BOX (GTK_DIALOG (dialog)->action_area),
			     FALSE);
    hbbox = gtk_hbutton_box_new ();
    gtk_button_box_set_spacing (GTK_BUTTON_BOX (hbbox), 4);
    gtk_box_pack_end (GTK_BOX (GTK_DIALOG (dialog)->action_area),
		      hbbox,
		      FALSE,
		      FALSE,
		      0);
    gtk_widget_show (hbbox);

    label = gtk_label_new(_("Contrast:"));
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table),
		     label,
		     0, 1,
		     1, 2,
		     GTK_FILL,
		     GTK_FILL,
		     0,
		     0);
    gtk_widget_show(label);
    box = gtk_hbox_new(FALSE, 8);
    gtk_table_attach(GTK_TABLE(table),
		     box,
		     1, 4,
		     1, 2,
		     GTK_FILL,
		     GTK_FILL,
		     0,
		     0);
    gtk_widget_show(box);

    contrast_adjustment = scale_data =
	gtk_adjustment_new(vars.contrast, lower->contrast,
			   upper->contrast, defvars->contrast / 100,
			   defvars->contrast / 100,
			   defvars->contrast / 100);

    gtk_signal_connect(GTK_OBJECT(contrast_adjustment),
		       "value_changed",
		       (GtkSignalFunc)gtk_contrast_update,
		       NULL);

    contrast_scale = scale = gtk_hscale_new(GTK_ADJUSTMENT(scale_data));
    gtk_box_pack_start(GTK_BOX(box), scale, FALSE, FALSE, 0);
    gtk_widget_set_usize(scale, 200, 0);
    gtk_scale_set_draw_value(GTK_SCALE(scale), FALSE);
    gtk_range_set_update_policy(GTK_RANGE(scale), GTK_UPDATE_CONTINUOUS);
    gtk_widget_show(scale);

    contrast_entry = entry = gtk_entry_new();
    sprintf(s, "%5.3f", vars.contrast);
    gtk_entry_set_text(GTK_ENTRY(entry), s);
    gtk_signal_connect(GTK_OBJECT(entry),
		       "changed",
		       (GtkSignalFunc)gtk_contrast_callback,
		       NULL);
    gtk_box_pack_start(GTK_BOX(box), entry, FALSE, FALSE, 0);
    gtk_widget_set_usize(entry, 40, 0);
    gtk_widget_show(entry);

    /***
     * Cyan slider...
     ***/
    label = gtk_label_new(_("Cyan:"));
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table),
		     label,
		     0, 1,
		     2, 3,
		     GTK_FILL,
		     GTK_FILL,
		     0,
		     0);
    gtk_widget_show(label);

    box = gtk_hbox_new(FALSE, 8);
    gtk_table_attach(GTK_TABLE(table),
		     box,
		     1, 4,
		     2, 3,
		     GTK_FILL, GTK_FILL,
		     0, 0);
    gtk_widget_show(box);

    cyan_adjustment = scale_data =
	gtk_adjustment_new(vars.cyan, lower->cyan,
			   upper->cyan, defvars->cyan / 100,
			   defvars->cyan / 100,
			   defvars->cyan / 100);

    gtk_signal_connect(GTK_OBJECT(scale_data),
		       "value_changed",
		       (GtkSignalFunc)gtk_cyan_update,
		       NULL);

    cyan_scale = scale = gtk_hscale_new(GTK_ADJUSTMENT(scale_data));
    gtk_box_pack_start(GTK_BOX(box), scale, FALSE, FALSE, 0);
    gtk_widget_set_usize(scale, 200, 0);
    gtk_scale_set_draw_value(GTK_SCALE(scale), FALSE);
    gtk_range_set_update_policy(GTK_RANGE(scale), GTK_UPDATE_CONTINUOUS);
    gtk_widget_show(scale);

    cyan_entry = entry = gtk_entry_new();
    sprintf(s, "%5.3f", vars.cyan);
    gtk_entry_set_text(GTK_ENTRY(entry), s);
    gtk_signal_connect(GTK_OBJECT(entry),
		       "changed",
		       (GtkSignalFunc)gtk_cyan_callback,
		       NULL);
    gtk_box_pack_start(GTK_BOX(box), entry, FALSE, FALSE, 0);
    gtk_widget_set_usize(entry, 40, 0);
    gtk_widget_show(entry);

    /***
     * Magenta slider...
     ***/
    label = gtk_label_new(_("Magenta:"));
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table),
		     label,
		     0, 1,
		     3, 4,
		     GTK_FILL,
		     GTK_FILL,
		     0, 0);
    gtk_widget_show(label);

    box = gtk_hbox_new(FALSE, 8);
    gtk_table_attach(GTK_TABLE(table),
		     box,
		     1, 4,
		     3, 4,
		     GTK_FILL,
		     GTK_FILL,
		     0,
		     0);
    gtk_widget_show(box);

    magenta_adjustment = scale_data =
	gtk_adjustment_new(vars.magenta, lower->magenta,
			   upper->magenta, defvars->magenta / 100,
			   defvars->magenta / 100,
			   defvars->magenta / 100);

    gtk_signal_connect(GTK_OBJECT(scale_data),
		       "value_changed",
		       (GtkSignalFunc)gtk_magenta_update,
		       NULL);

    magenta_scale = scale = gtk_hscale_new(GTK_ADJUSTMENT(scale_data));
    gtk_box_pack_start(GTK_BOX(box), scale, FALSE, FALSE, 0);
    gtk_widget_set_usize(scale, 200, 0);
    gtk_scale_set_draw_value(GTK_SCALE(scale), FALSE);
    gtk_range_set_update_policy(GTK_RANGE(scale), GTK_UPDATE_CONTINUOUS);
    gtk_widget_show(scale);

    magenta_entry = entry = gtk_entry_new();
    sprintf(s, "%5.3f", vars.magenta);
    gtk_entry_set_text(GTK_ENTRY(entry), s);
    gtk_signal_connect(GTK_OBJECT(entry),
		       "changed",
		       (GtkSignalFunc)gtk_magenta_callback,
		       NULL);
    gtk_box_pack_start(GTK_BOX(box), entry, FALSE, FALSE, 0);
    gtk_widget_set_usize(entry, 40, 0);
    gtk_widget_show(entry);

    /***
     * Yellow slider...
     ***/
    label = gtk_label_new(_("Yellow:"));
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table),
		     label,
		     0, 1,
		     4, 5,
		     GTK_FILL,
		     GTK_FILL,
		     0,
		     0);
    gtk_widget_show(label);

    box = gtk_hbox_new(FALSE, 8);
    gtk_table_attach(GTK_TABLE(table),
		     box,
		     1, 4,
		     4, 5,
		     GTK_FILL,
		     GTK_FILL,
		     0,
		     0);
    gtk_widget_show(box);

    yellow_adjustment = scale_data =
	gtk_adjustment_new(vars.yellow, lower->yellow,
			   upper->yellow, defvars->yellow / 100,
			   defvars->yellow / 100,
			   defvars->yellow / 100);

    gtk_signal_connect(GTK_OBJECT(scale_data),
		       "value_changed",
		       (GtkSignalFunc)gtk_yellow_update,
		       NULL);

    yellow_scale = scale = gtk_hscale_new(GTK_ADJUSTMENT(scale_data));
    gtk_box_pack_start(GTK_BOX(box), scale, FALSE, FALSE, 0);
    gtk_widget_set_usize(scale, 200, 0);
    gtk_scale_set_draw_value(GTK_SCALE(scale), FALSE);
    gtk_range_set_update_policy(GTK_RANGE(scale), GTK_UPDATE_CONTINUOUS);
    gtk_widget_show(scale);

    yellow_entry = entry = gtk_entry_new();
    sprintf(s, "%5.3f", vars.yellow);
    gtk_entry_set_text(GTK_ENTRY(entry), s);
    gtk_signal_connect(GTK_OBJECT(entry),
		       "changed",
		       (GtkSignalFunc)gtk_yellow_callback,
		       NULL);
    gtk_box_pack_start(GTK_BOX(box), entry, FALSE, FALSE, 0);
    gtk_widget_set_usize(entry, 40, 0);
    gtk_widget_show(entry);

    /***
     * Saturation slider...
     ***/
    label = gtk_label_new(_("Saturation:"));
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table),
		     label,
		     0,
		     1,
		     5,
		     6,
		     GTK_FILL,
		     GTK_FILL,
		     0,
		     0);
    gtk_widget_show(label);

    box = gtk_hbox_new(FALSE, 8);
    gtk_table_attach(GTK_TABLE(table),
		     box,
		     1,
		     4,
		     5,
		     6,
		     GTK_FILL,
		     GTK_FILL,
		     0,
		     0);
    gtk_widget_show(box);

    saturation_adjustment = scale_data =
	gtk_adjustment_new(vars.saturation, lower->saturation,
			   upper->saturation, defvars->saturation / 100,
			   defvars->saturation / 100,
			   defvars->saturation / 100);

    gtk_signal_connect(GTK_OBJECT(scale_data),
		       "value_changed",
		       (GtkSignalFunc)gtk_saturation_update, NULL);

    saturation_scale = scale = gtk_hscale_new(GTK_ADJUSTMENT(scale_data));
    gtk_box_pack_start(GTK_BOX(box), scale, FALSE, FALSE, 0);
    gtk_widget_set_usize(scale, 200, 0);
    gtk_scale_set_draw_value(GTK_SCALE(scale), FALSE);
    gtk_range_set_update_policy(GTK_RANGE(scale), GTK_UPDATE_CONTINUOUS);
    gtk_widget_show(scale);

    saturation_entry = entry = gtk_entry_new();
    sprintf(s, "%5.3f", vars.saturation);
    gtk_entry_set_text(GTK_ENTRY(entry), s);
    gtk_signal_connect(GTK_OBJECT(entry),
		       "changed",
		       (GtkSignalFunc)gtk_saturation_callback,
		       NULL);
    gtk_box_pack_start(GTK_BOX(box), entry, FALSE, FALSE, 0);
    gtk_widget_set_usize(entry, 40, 0);
    gtk_widget_show(entry);

    /***
     * Density slider...
     ***/
    label = gtk_label_new(_("Density:"));
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table),
		     label,
		     0,
		     1,
		     6,
		     7,
		     GTK_FILL,
		     GTK_FILL,
		     0,
		     0);
    gtk_widget_show(label);

    box = gtk_hbox_new(FALSE, 8);
    gtk_table_attach(GTK_TABLE(table),
		     box,
		     1,
		     4,
		     6,
		     7,
		     GTK_FILL,
		     GTK_FILL,
		     0,
		     0);
    gtk_widget_show(box);

    density_adjustment = scale_data =
	gtk_adjustment_new(vars.density, lower->density,
			   upper->density, defvars->density / 100,
			   defvars->density / 100,
			   defvars->density / 100);

    gtk_signal_connect(GTK_OBJECT(scale_data),
		       "value_changed",
		       (GtkSignalFunc)gtk_density_update,
		       NULL);

    density_scale = scale = gtk_hscale_new(GTK_ADJUSTMENT(scale_data));
    gtk_box_pack_start(GTK_BOX(box), scale, FALSE, FALSE, 0);
    gtk_widget_set_usize(scale, 200, 0);
    gtk_scale_set_draw_value(GTK_SCALE(scale), FALSE);
    gtk_range_set_update_policy(GTK_RANGE(scale), GTK_UPDATE_CONTINUOUS);
    gtk_widget_show(scale);

    density_entry = entry = gtk_entry_new();
    sprintf(s, "%5.3f", vars.density);
    gtk_entry_set_text(GTK_ENTRY(entry), s);
    gtk_signal_connect(GTK_OBJECT(entry),
		       "changed",
		       (GtkSignalFunc)gtk_density_callback,
		       NULL);
    gtk_box_pack_start(GTK_BOX(box), entry, FALSE, FALSE, 0);
    gtk_widget_set_usize(entry, 40, 0);
    gtk_widget_show(entry);

    /***
     * Gamma slider...
     ***/
    label = gtk_label_new(_("Gamma:"));
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table),
		     label,
		     0,
		     1,
		     7,
		     8,
		     GTK_FILL, GTK_FILL,
		     0, 0);
    gtk_widget_show(label);

    box = gtk_hbox_new(FALSE, 8);
    gtk_table_attach(GTK_TABLE(table),
		     box,
		     1,
		     4,
		     7,
		     8,
		     GTK_FILL,
		     GTK_FILL,
		     0,
		     0);
    gtk_widget_show(box);

    gamma_adjustment = scale_data =
	gtk_adjustment_new(vars.gamma, lower->gamma,
			   upper->gamma, defvars->gamma / 100,
			   defvars->gamma / 100,
			   defvars->gamma / 100);

    gtk_signal_connect(GTK_OBJECT(gamma_adjustment),
		       "value_changed",
		       (GtkSignalFunc)gtk_gamma_update,
		       NULL);

    gamma_scale = scale = gtk_hscale_new(GTK_ADJUSTMENT(scale_data));
    gtk_box_pack_start(GTK_BOX(box), scale, FALSE, FALSE, 0);
    gtk_widget_set_usize(scale, 200, 0);
    gtk_scale_set_draw_value(GTK_SCALE(scale), FALSE);
    gtk_range_set_update_policy(GTK_RANGE(scale), GTK_UPDATE_CONTINUOUS);
    gtk_widget_show(scale);

    gamma_entry = entry = gtk_entry_new();
    sprintf(s, "%5.3f", vars.gamma);
    gtk_entry_set_text(GTK_ENTRY(entry), s);
    gtk_signal_connect(GTK_OBJECT(entry),
		       "changed",
		       (GtkSignalFunc)gtk_gamma_callback,
		       NULL);
    gtk_box_pack_start(GTK_BOX(box), entry, FALSE, FALSE, 0);
    gtk_widget_set_usize(entry, 40, 0);
    gtk_widget_show(entry);

    /***
     * Dither algorithm  option menu...
     ***/
    label = gtk_label_new(_("Dither Algorithm:"));
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table),
		     label,
		     1, 2,
		     8, 9,
		     GTK_FILL, GTK_FILL,
		     0, 0);
    gtk_widget_show(label);

    box = gtk_hbox_new(FALSE, 0);
    gtk_table_attach(GTK_TABLE(table),
		     box,
		     2, 3,
		     8, 9,
		     GTK_FILL, GTK_FILL,
		     0, 0);
    gtk_widget_show(box);

    dither_algo_button = gtk_option_menu_new();
    gtk_box_pack_start(GTK_BOX(box), dither_algo_button, FALSE, FALSE, 0);
    gtk_build_dither_menu();


    /***
     * Add dismiss button
     ***/
    gtk_container_set_border_width(GTK_CONTAINER(GTK_DIALOG(dialog)->
						 action_area), 2);
    gtk_box_set_homogeneous (GTK_BOX (GTK_DIALOG (dialog)->action_area),
			     FALSE);
    hbbox = gtk_hbutton_box_new ();
    gtk_button_box_set_spacing (GTK_BUTTON_BOX (hbbox), 4);
    gtk_box_pack_end (GTK_BOX (GTK_DIALOG (dialog)->action_area),
		      hbbox,
		      TRUE, TRUE, 0);
    gtk_widget_show (hbbox);

    dismiss_button = gtk_button_new_with_label (_("Dismiss"));
    GTK_WIDGET_SET_FLAGS (dismiss_button, GTK_CAN_DEFAULT);
    gtk_signal_connect (GTK_OBJECT (dismiss_button),
			"clicked",
			(GtkSignalFunc)gtk_close_adjust_callback,
			NULL);
    gtk_box_pack_start (GTK_BOX (hbbox), dismiss_button, FALSE, FALSE, 0);
    gtk_widget_grab_default (dismiss_button);
    gtk_widget_show (dismiss_button);

    /***
     * Don't realize this dialog just yet, just return
     ***/
}

void
gtk_do_color_updates(void)
{
  GTK_ADJUSTMENT(brightness_adjustment)->value = plist[plist_current].v.brightness;
  gtk_signal_emit_by_name(brightness_adjustment, "value_changed");

  GTK_ADJUSTMENT(gamma_adjustment)->value = plist[plist_current].v.gamma;
  gtk_signal_emit_by_name(gamma_adjustment, "value_changed");

  GTK_ADJUSTMENT(contrast_adjustment)->value = plist[plist_current].v.contrast;
  gtk_signal_emit_by_name(contrast_adjustment, "value_changed");

  GTK_ADJUSTMENT(cyan_adjustment)->value = plist[plist_current].v.cyan;
  gtk_signal_emit_by_name(cyan_adjustment, "value_changed");

  GTK_ADJUSTMENT(magenta_adjustment)->value = plist[plist_current].v.magenta;
  gtk_signal_emit_by_name(magenta_adjustment, "value_changed");

  GTK_ADJUSTMENT(yellow_adjustment)->value = plist[plist_current].v.yellow;
  gtk_signal_emit_by_name(yellow_adjustment, "value_changed");

  GTK_ADJUSTMENT(saturation_adjustment)->value = plist[plist_current].v.saturation;
  gtk_signal_emit_by_name(saturation_adjustment, "value_changed");

  GTK_ADJUSTMENT(density_adjustment)->value = plist[plist_current].v.density;
  gtk_signal_emit_by_name(density_adjustment, "value_changed");
}

/***************************************************************************
 *
 * gtk_brightness_callback()
 *
 * Update the brightness scale using the text entry.
 ***************************************************************************/
static void gtk_brightness_callback(GtkWidget *widget) /* I - Entry widget */
{
    gint new_value;  /* New scaling value */

    new_value = atoi(gtk_entry_get_text(GTK_ENTRY(widget)));

    if (vars.brightness != new_value)
    {
	if ((new_value >= GTK_ADJUSTMENT(brightness_adjustment)->lower) &&
	    (new_value < GTK_ADJUSTMENT(brightness_adjustment)->upper))
	{
	    GTK_ADJUSTMENT(brightness_adjustment)->value = new_value;

	    gtk_signal_emit_by_name(brightness_adjustment, "value_changed");
	}
    }
}


/****************************************************************************
 *
 * gtk_brightness_update()
 *
 *  Update the brightness field using the scale.
 ***************************************************************************/
static void gtk_brightness_update(GtkAdjustment *adjustment) /* I- New value */
{
    char s[255];  /* Text buffer */

    if (vars.brightness != adjustment->value)
    {
	vars.brightness = adjustment->value;
	plist[plist_current].v.brightness = adjustment->value;

	sprintf(s, "%5.3f", vars.brightness);

	gtk_signal_handler_block_by_data(GTK_OBJECT(brightness_entry), NULL);
	gtk_entry_set_text(GTK_ENTRY(brightness_entry), s);
	gtk_signal_handler_unblock_by_data(GTK_OBJECT(brightness_entry), NULL);
    }
}

/****************************************************************************
 *
 * gtk_contrast_callback(()
 *
 *  Update the brightness field using the scale.
 ***************************************************************************/
static void gtk_contrast_callback(GtkWidget *widget)  /* I - Entry widget */
{
    gint new_value;  /* New scaling value */

    new_value = atoi(gtk_entry_get_text(GTK_ENTRY(widget)));

    if (vars.contrast != new_value)
    {
	if ((new_value >= GTK_ADJUSTMENT(contrast_adjustment)->lower) &&
	    (new_value < GTK_ADJUSTMENT(contrast_adjustment)->upper))
	{
	    GTK_ADJUSTMENT(contrast_adjustment)->value = new_value;

	    gtk_signal_emit_by_name(contrast_adjustment, "value_changed");
	}
    }
}

/****************************************************************************
 *
 * gtk_contrast_update()
 *
 *  Update the contrast field using the scale.
 ***************************************************************************/
static void gtk_contrast_update(GtkAdjustment *adjustment) /* I - New value */
{
    char s[255];  /* Text buffer */

    if (vars.contrast != adjustment->value)
    {
	vars.contrast = adjustment->value;
	plist[plist_current].v.contrast = adjustment->value;

	sprintf(s, "%5.3f", vars.contrast);

	gtk_signal_handler_block_by_data(GTK_OBJECT(contrast_entry), NULL);
	gtk_entry_set_text(GTK_ENTRY(contrast_entry), s);
	gtk_signal_handler_unblock_by_data(GTK_OBJECT(contrast_entry), NULL);
    }
}


/****************************************************************************
 *
 * gtk_cyan_callback() - Update the cyan scale using the text entry.
 *
 ***************************************************************************/
static void gtk_cyan_callback(GtkWidget *widget)	/* I - Entry widget */
{
  gint new_value;  /* New scaling value */

  new_value = atoi(gtk_entry_get_text(GTK_ENTRY(widget)));

  if (vars.cyan != new_value)
  {
    if ((new_value >= GTK_ADJUSTMENT(cyan_adjustment)->lower) &&
	(new_value < GTK_ADJUSTMENT(cyan_adjustment)->upper))
    {
      GTK_ADJUSTMENT(cyan_adjustment)->value = new_value;

      gtk_signal_emit_by_name(cyan_adjustment, "value_changed");
    }
  }
}

/****************************************************************************
 *
 * gtk_cyan_update() - Update the cyan field using the scale.
 *
 ***************************************************************************/
static void gtk_cyan_update(GtkAdjustment *adjustment)	/* I - New value */
{
  char s[255];  /* Text buffer */

  if (vars.cyan != adjustment->value)
  {
    vars.cyan = adjustment->value;
    plist[plist_current].v.cyan = adjustment->value;

    sprintf(s, "%5.3f", vars.cyan);

    gtk_signal_handler_block_by_data(GTK_OBJECT(cyan_entry), NULL);
    gtk_entry_set_text(GTK_ENTRY(cyan_entry), s);
    gtk_signal_handler_unblock_by_data(GTK_OBJECT(cyan_entry), NULL);
  }
}

/****************************************************************************
 *
 * gtk_magenta_callback() - Update the magenta scale using the text entry.
 *
 ***************************************************************************/
static void gtk_magenta_callback(GtkWidget *widget)	/* I - Entry widget */
{
  gint new_value;   /* New scaling value */

  new_value = atoi(gtk_entry_get_text(GTK_ENTRY(widget)));

  if (vars.magenta != new_value)
  {
    if ((new_value >= GTK_ADJUSTMENT(magenta_adjustment)->lower) &&
	(new_value < GTK_ADJUSTMENT(magenta_adjustment)->upper))
    {
      GTK_ADJUSTMENT(magenta_adjustment)->value = new_value;

      gtk_signal_emit_by_name(magenta_adjustment, "value_changed");
    }
  }
}

/****************************************************************************
 *
 * gtk_magenta_update() - Update the magenta field using the scale.
 *
 ***************************************************************************/
static void gtk_magenta_update(GtkAdjustment *adjustment)	/* I - New value */
{
  char s[255];  /* Text buffer */

  if (vars.magenta != adjustment->value)
  {
    vars.magenta = adjustment->value;
    plist[plist_current].v.magenta = adjustment->value;

    sprintf(s, "%5.3f", vars.magenta);

    gtk_signal_handler_block_by_data(GTK_OBJECT(magenta_entry), NULL);
    gtk_entry_set_text(GTK_ENTRY(magenta_entry), s);
    gtk_signal_handler_unblock_by_data(GTK_OBJECT(magenta_entry), NULL);
  }
}

/****************************************************************************
 *
 * gtk_yellow_callback() - Update the yellow scale using the text entry.
 *
 ***************************************************************************/
static void gtk_yellow_callback(GtkWidget *widget)	/* I - Entry widget */
{
  gint new_value;  /* New scaling value */

  new_value = atoi(gtk_entry_get_text(GTK_ENTRY(widget)));

  if (vars.yellow != new_value)
  {
    if ((new_value >= GTK_ADJUSTMENT(yellow_adjustment)->lower) &&
	(new_value < GTK_ADJUSTMENT(yellow_adjustment)->upper))
    {
      GTK_ADJUSTMENT(yellow_adjustment)->value = new_value;

      gtk_signal_emit_by_name(yellow_adjustment, "value_changed");
    }
  }
}

/****************************************************************************
 *
 * gtk_yellow_update() - Update the yellow field using the scale.
 *
 ***************************************************************************/
static void gtk_yellow_update(GtkAdjustment *adjustment)	/* I - New value */
{
  char s[255];  /* Text buffer */

  if (vars.yellow != adjustment->value)
  {
    vars.yellow = adjustment->value;
    plist[plist_current].v.yellow = adjustment->value;

    sprintf(s, "%5.3f", vars.yellow);

    gtk_signal_handler_block_by_data(GTK_OBJECT(yellow_entry), NULL);
    gtk_entry_set_text(GTK_ENTRY(yellow_entry), s);
    gtk_signal_handler_unblock_by_data(GTK_OBJECT(yellow_entry), NULL);
  }
}

/****************************************************************************
 *
 * gtk_gamma_callback() - Update the gamma scale using the text entry.
 *
 ***************************************************************************/
static void gtk_gamma_callback(GtkWidget *widget)	/* I - Entry widget */
{
  gint new_value;  /* New scaling value */

  new_value = atoi(gtk_entry_get_text(GTK_ENTRY(widget)));

  if (vars.gamma != new_value)
  {
    if ((new_value >= GTK_ADJUSTMENT(gamma_adjustment)->lower) &&
	(new_value < GTK_ADJUSTMENT(gamma_adjustment)->upper))
    {
      GTK_ADJUSTMENT(gamma_adjustment)->value = new_value;

      gtk_signal_emit_by_name(gamma_adjustment, "value_changed");
    }
  }
}

/****************************************************************************
 *
 * gtk_gamma_update() - Update the gamma field using the scale.
 *
 ***************************************************************************/
static void gtk_gamma_update(GtkAdjustment *adjustment)	/* I - New value */
{
  char s[255];  /* Text buffer */

  if (vars.gamma != adjustment->value)
  {
    vars.gamma = adjustment->value;

    plist[plist_current].v.gamma = adjustment->value;

    sprintf(s, "%5.3f", vars.gamma);

    gtk_signal_handler_block_by_data(GTK_OBJECT(gamma_entry), NULL);
    gtk_entry_set_text(GTK_ENTRY(gamma_entry), s);
    gtk_signal_handler_unblock_by_data(GTK_OBJECT(gamma_entry), NULL);

  }
}

/****************************************************************************
 *
 * gtk_saturation_callback() - Update the saturation scale using the text
 * entry.
 *
 ***************************************************************************/
static void gtk_saturation_callback(GtkWidget *widget)	/* I - Entry widget */
{
  gint new_value;  /* New scaling value */

  new_value = atoi(gtk_entry_get_text(GTK_ENTRY(widget)));

  if (vars.saturation != new_value)
  {
    if ((new_value >= GTK_ADJUSTMENT(saturation_adjustment)->lower) &&
	(new_value < GTK_ADJUSTMENT(saturation_adjustment)->upper))
    {
      GTK_ADJUSTMENT(saturation_adjustment)->value = new_value;

      gtk_signal_emit_by_name(saturation_adjustment, "value_changed");
    }
  }
}

/****************************************************************************
 *
 * gtk_saturation_update() - Update the saturation field using the scale.
 *
 ***************************************************************************/
static void gtk_saturation_update(GtkAdjustment *adjustment)
{
  char s[255];  /* Text buffer */

  if (vars.saturation != adjustment->value)
  {
    vars.saturation = adjustment->value;
    plist[plist_current].v.saturation = adjustment->value;

    sprintf(s, "%5.3f", vars.saturation);

    gtk_signal_handler_block_by_data(GTK_OBJECT(saturation_entry), NULL);
    gtk_entry_set_text(GTK_ENTRY(saturation_entry), s);
    gtk_signal_handler_unblock_by_data(GTK_OBJECT(saturation_entry), NULL);
  }
}


/****************************************************************************
 *
 * gtk_density_callback() - Update the density scale using the text entry.
 *
 ***************************************************************************/
static void gtk_density_callback(GtkWidget *widget)	/* I - Entry widget */
{
  gint  new_value;  /* New scaling value */

  new_value = atoi(gtk_entry_get_text(GTK_ENTRY(widget)));

  if (vars.density != new_value)
  {
    if ((new_value >= GTK_ADJUSTMENT(density_adjustment)->lower) &&
	(new_value < GTK_ADJUSTMENT(density_adjustment)->upper))
    {
      GTK_ADJUSTMENT(density_adjustment)->value = new_value;

      gtk_signal_emit_by_name(density_adjustment, "value_changed");
    }
  }
}

/****************************************************************************
 *
 * gtk_density_update() - Update the density field using the scale.
 *
 ***************************************************************************/
static void gtk_density_update(GtkAdjustment *adjustment)  /* I - New value */
{
  char s[255];  /* Text buffer */

  if (vars.density != adjustment->value)
  {
    vars.density = adjustment->value;
    plist[plist_current].v.density = adjustment->value;

    sprintf(s, "%4.3f", vars.density);

    gtk_signal_handler_block_by_data(GTK_OBJECT(density_entry), NULL);
    gtk_entry_set_text(GTK_ENTRY(density_entry), s);
    gtk_signal_handler_unblock_by_data(GTK_OBJECT(density_entry), NULL);
  }
}

/****************************************************************************
 *
 * gtk_close_adjust_callback() - callback for destruction of popup.  Don't
 * actually do anything, we just want to hide/show the dialog.
 *
 ***************************************************************************/
static void gtk_close_adjust_callback(void)
{
    gtk_widget_hide(gtk_color_adjust_dialog);
}

/****************************************************************************
 *
 * gtk_build_dither_menu() - builds the dither algorithm option menu.
 *
 ***************************************************************************/
void gtk_build_dither_menu()
{
    int		i;	/* Looping var */
    GtkWidget	*item;	/* Menu item */
    GtkWidget	*item0 = 0;	/* First menu item */

    if (dither_algo_menu != NULL)
    {
	gtk_widget_destroy(dither_algo_menu);
	dither_algo_menu = NULL;
    }

    dither_algo_menu = gtk_menu_new();

    if (stp_dither_algorithm_count() == 0)
    {
	item = gtk_menu_item_new_with_label (_("Standard"));
	gtk_menu_append (GTK_MENU (dither_algo_menu), item);
	gtk_widget_show (item);
	gtk_option_menu_set_menu (GTK_OPTION_MENU (dither_algo_button),
				  dither_algo_menu);
	gtk_widget_set_sensitive (dither_algo_button, FALSE);
	gtk_widget_show(dither_algo_button);
	return;
    }
    else
    {
	gtk_widget_set_sensitive(dither_algo_button, TRUE);
    }

    for (i = 0; i < stp_dither_algorithm_count(); i ++)
    {
	item = gtk_menu_item_new_with_label(gettext(stp_dither_algorithm_name(i)));
	if (i == 0)
	    item0 = item;
	gtk_menu_append(GTK_MENU(dither_algo_menu), item);
	gtk_signal_connect(GTK_OBJECT(item),
			   "activate",
			   (GtkSignalFunc)gtk_dither_algo_callback,
			   (gpointer)i);
	gtk_widget_show(item);
    }

    gtk_option_menu_set_menu(GTK_OPTION_MENU(dither_algo_button ),
			     dither_algo_menu);

    for (i = 0; i < stp_dither_algorithm_count(); i ++)
    {
#ifdef DEBUG
	printf("item[%d] = \'%s\'\n", i, stp_dither_algorithm_name(i));
#endif /* DEBUG */

	if (strcmp(stp_dither_algorithm_name(i), plist[plist_current].v.dither_algorithm) == 0)
	{
	    gtk_option_menu_set_history(GTK_OPTION_MENU(dither_algo_button), i);
	    break;
	}
    }

    if (i == stp_dither_algorithm_count())
    {
	gtk_option_menu_set_history(GTK_OPTION_MENU(dither_algo_button), 0);
	gtk_signal_emit_by_name(GTK_OBJECT(item0), "activate");
    }
    gtk_widget_show(dither_algo_button);
}

/****************************************************************************
 *
 * gtk_dither_algo_Callback()
 *
 ****************************************************************************/
static void
gtk_dither_algo_callback (GtkWidget *widget,
			  gint   data)
{
  strcpy(vars.dither_algorithm, stp_dither_algorithm_name(data));
  strcpy(plist[plist_current].v.dither_algorithm,
	 stp_dither_algorithm_name(data));
}

#endif  /* ! NEW_UI_ONLY */
