/*
 * "$Id: papers.h,v 1.5 2003/01/06 20:57:39 rleigh Exp $"
 *
 *   libgimpprint header.
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
 * Revision History:
 *
 *   See ChangeLog
 */

/*
 * This file must include only standard C header files.  The core code must
 * compile on generic platforms that don't support glib, gimp, gtk, etc.
 */

#ifndef GIMP_PRINT_INTERNAL_PAPERS_H
#define GIMP_PRINT_INTERNAL_PAPERS_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


typedef struct
{
  char *name;
  char *text;
  char *comment;
  unsigned width;
  unsigned height;
  unsigned top;
  unsigned left;
  unsigned bottom;
  unsigned right;
  stp_papersize_unit_t paper_unit;
} stp_internal_papersize_t;


extern stp_list_t *stp_paper_list;


extern int stp_init_paper_list(void);

extern int stp_paper_create(stp_papersize_t pt);
extern int stp_paper_destroy(stp_papersize_t pt);

extern void stp_default_media_size(const stp_vars_t v,
				   int *width, int *height);


#ifdef __cplusplus
  }
#endif

#endif /* GIMP_PRINT_INTERNAL_PAPERS_H */
/*
 * End of "$Id: papers.h,v 1.5 2003/01/06 20:57:39 rleigh Exp $".
 */
