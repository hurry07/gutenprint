/*
 * "$Id: testpattern.h,v 1.2.2.1 2001/09/14 02:15:51 sharkey Exp $"
 *
 *   Test pattern generator for Gimp-Print
 *
 *   Copyright 2001 Robert Krawitz <rlk@alum.mit.edu>
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

typedef struct
{
  double c_min;
  double c;
  double c_gamma;
  double m_min;
  double m;
  double m_gamma;
  double y_min;
  double y;
  double y_gamma;
  double k_min;
  double k;
  double k_gamma;
  double c_level;
  double m_level;
  double y_level;
  double lower;
  double upper;
} testpattern_t;

extern double global_c_level;
extern double global_c_gamma;
extern double global_m_level;
extern double global_m_gamma;
extern double global_y_level;
extern double global_y_gamma;
extern double global_k_gamma;
extern double global_gamma;
extern int levels;
extern double ink_limit;
extern char *printer;
extern char *ink_type;
extern char *resolution;
extern char *media_source;
extern char *media_type;
extern char *media_size;
extern char *dither_algorithm;
extern double density;
extern double xtop;
extern double xleft;
extern double hsize;
extern double vsize;
extern int noblackline;
extern char *c_strdup(const char *s);
extern testpattern_t *get_next_testpattern(void);

typedef union yylv {
  int ival;
  double dval;
  char *sval;
} YYSTYPE;

extern YYSTYPE yylval;

#include "testpatterny.h"


