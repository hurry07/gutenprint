/*
 * "$Id: printers.c,v 1.2 2002/10/27 00:43:02 rlk Exp $"
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
#include <string.h>

#define FMIN(a, b) ((a) < (b) ? (a) : (b))

const stp_printer_t
stp_get_printer_by_long_name(const char *long_name)
{
  int i;
  if (!long_name)
    return NULL;
  for (i = 0; i < stp_known_printers(); i++)
    {
      const stp_printer_t val = stp_get_printer_by_index(i);
      if (!strcmp(stp_printer_get_long_name(val), long_name))
	return val;
    }
  return NULL;
}

const stp_printer_t
stp_get_printer_by_driver(const char *driver)
{
  int i;
  if (!driver)
    return NULL;
  for (i = 0; i < stp_known_printers(); i++)
    {
      const stp_printer_t val = stp_get_printer_by_index(i);
      if (!strcmp(stp_printer_get_driver(val), driver))
	return val;
    }
  return NULL;
}

int
stp_get_printer_index_by_driver(const char *driver)
{
  int idx = 0;
  if (!driver)
    return -1;
  for (idx = 0; idx < stp_known_printers(); idx++)
    {
      const stp_printer_t val = stp_get_printer_by_index(idx);
      if (!strcmp(stp_printer_get_driver(val), driver))
	return idx;
    }
  return -1;
}
