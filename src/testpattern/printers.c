/*
 * "$Id: printers.c,v 1.3 2003/04/15 02:24:58 rlk Exp $"
 *
 *   Dump the per-printer options for Grant Taylor's *-omatic database
 *
 *   Copyright 2000 Robert Krawitz (rlk@alum.mit.edu)
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
#include <stdio.h>
#ifdef INCLUDE_GIMP_PRINT_H
#include INCLUDE_GIMP_PRINT_H
#else
#include <gimp-print/gimp-print.h>
#endif
#include "../../lib/libprintut.h"
#include <string.h>

int
main(int argc, char **argv)
{
  int i;

  stp_init();
  for (i = 0; i < stp_printer_model_count(); i++)
    {
      stp_const_printer_t p = stp_get_printer_by_index(i);
      if (strcmp(stp_printer_get_family(p), "ps") &&
	  strcmp(stp_printer_get_family(p), "raw"))
	printf("%s\n", stp_printer_get_driver(p));
    }
  return 0;
}
