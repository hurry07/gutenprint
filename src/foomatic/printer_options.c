/*
 * "$Id: printer_options.c,v 1.12 2002/11/01 03:18:44 rlk Exp $"
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

const char *params[] =
{
  "PageSize",
  "Resolution",
  "InkType",
  "MediaType",
  "InputSlot"
};

int nparams = sizeof(params) / sizeof(const char *);

int
main(int argc, char **argv)
{
  int i, j, k;
  for (i = 0; i < stp_known_printers(); i++)
    {
      const stp_printer_t p = stp_get_printer_by_index(i);
      stp_vars_t pv = stp_allocate_copy(stp_printer_get_printvars(p));
      stp_param_t *retval;
      const char *retval1;
      int count;
      int tcount = 0;
      printf("# Printer model %s, long name `%s'\n",
	     stp_printer_get_driver(p), stp_printer_get_long_name(p));
      for (k = 0; k < nparams; k++)
	{
	  retval1 = stp_printer_get_default_parameter(p, pv, params[k]);
	  if (retval1)
	    printf("$defaults{'%s'}{'%s'} = '%s';\n",
		   stp_printer_get_driver(p), params[k], retval1);
	  retval = stp_printer_get_parameters(p, pv, params[k], &count);
	  if (count > 0)
	    {
	      for (j = 0; j < count; j++)
		{
		  printf("$stpdata{'%s'}{'%s'}{'%s'} = '%s';\n",
			 stp_printer_get_driver(p), params[k], retval[j].name,
			 retval[j].text);
		  if (strcmp(params[k], "Resolution") == 0)
		    {
		      int x, y;
		      stp_set_resolution(pv, retval[j].name);
		      stp_printer_describe_resolution(p, pv, &x, &y);
		      if (x > 0 && y > 0)
			{
			  printf("$stpdata{'%s'}{'%s'}{'%s'} = '%d';\n",
				 stp_printer_get_driver(p), "x_resolution",
				 retval[j].name, x);
			  printf("$stpdata{'%s'}{'%s'}{'%s'} = '%d';\n",
				 stp_printer_get_driver(p), "y_resolution",
				 retval[j].name, y);
			}
		    }
		  free((void *)retval[j].name);
		  free((void *)retval[j].text);
		}
	      free(retval);
	    }
	  tcount += count;
	}
      if (tcount > 0)
	{
	  printf("$defaults{'%s'}{'%s'} = '%s';\n",
		 stp_printer_get_driver(p), "Dither",
		 stp_dither_algorithm_name(0));
	  for (k = 0; k < stp_dither_algorithm_count(); k++)
	    printf("$stpdata{'%s'}{'%s'}{'%s'} = '%s';\n",
		   stp_printer_get_driver(p), "Dither",
		   stp_dither_algorithm_name(k),
		   stp_dither_algorithm_text(k));
	  if (stp_get_output_type(pv) == OUTPUT_COLOR)
	    {
	      printf("$defaults{'%s'}{'%s'} = '%s';\n",
		     stp_printer_get_driver(p), "Color", "Color");
	      printf("$stpdata{'%s'}{'%s'}{'%s'} = '%s';\n",
		     stp_printer_get_driver(p), "Color", "Color",
		     "Color");
	      printf("$stpdata{'%s'}{'%s'}{'%s'} = '%s';\n",
		     stp_printer_get_driver(p), "Color", "RawCMYK",
		     "Raw CMYK");
	      printf("$stpdata{'%s'}{'%s'}{'%s'} = '%s';\n",
		     stp_printer_get_driver(p), "Color", "Grayscale",
		     "Gray Scale");
	      printf("$stpdata{'%s'}{'%s'}{'%s'} = '%s';\n",
		     stp_printer_get_driver(p), "Color", "BlackAndWhite",
		     "Black and White");
	    }
	  else
	    {
	      printf("$defaults{'%s'}{'%s'} = '%s';\n",
		     stp_printer_get_driver(p), "Color", "Grayscale");
	      printf("$stpdata{'%s'}{'%s'}{'%s'} = '%s';\n",
		     stp_printer_get_driver(p), "Color", "Grayscale",
		     "Grayscale");
	      printf("$stpdata{'%s'}{'%s'}{'%s'} = '%s';\n",
		     stp_printer_get_driver(p), "Color", "BlackAndWhite",
		     "Black and White");
	    }
	}
      stp_free_vars(pv);
    }
  return 0;
}
