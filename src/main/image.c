/*
 * "$Id: image.c,v 1.3.12.1 2003/08/18 23:29:20 rlk Exp $"
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <gimp-print/gimp-print.h>
#include "gimp-print-internal.h"

void
stpi_image_init(stp_image_t *image)
{
  if (image->init)
    image->init(image);
}

void
stpi_image_reset(stp_image_t *image)
{
  if (image->reset)
    image->reset(image);
}

stp_image_type_t
stpi_image_type(stp_image_t *image)
{
  return image->image_type(image);
}

int
stpi_image_bits_per_channel(stp_image_t *image)
{
  return image->bits_per_channel(image);
}

int
stpi_image_channels(stp_image_t *image)
{
  return image->channels(image);
}

int
stpi_image_width(stp_image_t *image)
{
  return image->width(image);
}

int
stpi_image_height(stp_image_t *image)
{
  return image->height(image);
}

stp_image_status_t
stpi_image_get_row(stp_image_t *image, unsigned char *data,
		  size_t byte_limit, int row)
{
  return image->get_row(image, data, byte_limit, row);
}

const char *
stpi_image_get_appname(stp_image_t *image)
{
  if (image->get_appname)
    return image->get_appname(image);
  else
    return NULL;
}

void
stpi_image_progress_init(stp_image_t *image)
{
  if (image->progress_init)
    image->progress_init(image);
}

void
stpi_image_note_progress(stp_image_t *image, double current, double total)
{
  if (image->note_progress)
    image->note_progress(image, current, total);
}

void
stpi_image_progress_conclude(stp_image_t *image)
{
  if (image->progress_conclude)
    image->progress_conclude(image);
}

void
stpi_image_transpose(stp_image_t *image)
{
  if (image->transpose)
    image->transpose(image);
}

void
stpi_image_hflip(stp_image_t *image)
{
  if (image->hflip)
    image->hflip(image);
}

void
stpi_image_vflip(stp_image_t *image)
{
  if (image->vflip)
    image->vflip(image);
}

void
stpi_image_rotate_ccw(stp_image_t *image)
{
  if (image->rotate_ccw)
    image->rotate_ccw(image);
}

void
stpi_image_rotate_cw(stp_image_t *image)
{
  if (image->rotate_cw)
    image->rotate_cw(image);
}

void
stpi_image_rotate_180(stp_image_t *image)
{
  if (image->rotate_180)
    image->rotate_180(image);
}

void
stpi_image_crop(stp_image_t *image, int left, int top, int right, int bottom)
{
  if (image->crop)
    image->crop(image, left, top, right, bottom);
}
