/*
 * "$Id: print-dither.c,v 1.38 2000/05/06 02:12:35 rlk Exp $"
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
 *
 * Revision History:
 *
 * See bottom
 */


/* #define PRINT_DEBUG */


#include "print.h"

#define IABS(a) ((a) >= 0 ? (a) : -(a))


#define MATRIX_NB0 (7)		/* How many iterations to generate matrix */
#define MATRIX_BASE0 (2)	/* Base size of matrix (before iterations) */
#define MATRIX_SIZE0 (1 << (MATRIX_NB0)) /* Axis length */
#define MATRIX_SIZE0_2 ((MATRIX_SIZE0) * (MATRIX_SIZE0)) /* Matrix points */
#define MODOP0(x, y) ((x) & ((y) - 1)) /* Operation to generate a mod */
				/* Normally a mod; if it's a power of 2, */
				/* we can use a bitwise and for efficiency */

#define MATRIX_NB1 (4)
#define MATRIX_BASE1 (3)
#define MATRIX_SIZE1 (3 * 3 * 3 * 3)
#define MATRIX_SIZE1_2 ((MATRIX_SIZE1) * (MATRIX_SIZE1))
#define MODOP1(x, y) ((x) % (y))

#define MATRIX_NB2 (3)
#define MATRIX_BASE2 (5)
#define MATRIX_SIZE2 (5 * 5 * 5)
#define MATRIX_SIZE2_2 ((MATRIX_SIZE2) * (MATRIX_SIZE2))
#define MODOP2(x, y) ((x) % (y))

#define MATRIX_NB3 (3)
#define MATRIX_BASE3 (5)
#define MATRIX_SIZE3 (5 * 5 * 5)
#define MATRIX_SIZE3_2 ((MATRIX_SIZE3) * (MATRIX_SIZE3))
#define MODOP3(x, y) ((x) % (y))

#define DITHERPOINT(d, x, y, m) \
((d)->ordered_dither_matrix##m[MODOP##m((x), MATRIX_SIZE##m)][MODOP##m((y), MATRIX_SIZE##m)])

#define D_FLOYD_HYBRID 0
#define D_FLOYD 1
#define D_ADAPTIVE_BASE 2
#define D_ADAPTIVE_HYBRID (D_ADAPTIVE_BASE | D_FLOYD_HYBRID)
#define D_ADAPTIVE_RANDOM (D_ADAPTIVE_BASE | D_FLOYD)
#define D_ORDERED_BASE 4
#define D_ORDERED (D_ORDERED_BASE)
#define D_ORDERED_PERTURBED (D_ORDERED_BASE + 1)

char *dither_algo_names[] =
{
  "Hybrid Floyd-Steinberg",
  "Ordered",
  "Random Floyd-Steinberg",
  "Adaptive Hybrid",
  "Adaptive Random",
  "Perturbed Ordered",
};

int num_dither_algos = sizeof(dither_algo_names) / sizeof(char *);


#define ERROR_ROWS 2
#define NCOLORS (4)

#define ECOLOR_C 0
#define ECOLOR_M 1
#define ECOLOR_Y 2
#define ECOLOR_K 3


/*
 * A segment of the entire 0-65536 intensity range.
 */
typedef struct dither_segment
{
  unsigned range_l;		/* Bottom of range */
  unsigned range_h;		/* Top of range */
  unsigned value_l;		/* Value of lighter ink */
  unsigned value_h;		/* Value of upper ink */
  unsigned bits_l;		/* Bit pattern of lower */
  unsigned bits_h;		/* Bit pattern of upper */
  unsigned range_span;		/* Span (to avoid calculation on the fly) */
  unsigned value_span;		/* Span of values */
  char isdark_l;		/* Is lower value dark ink? */
  char isdark_h;		/* Is upper value dark ink? */
} dither_segment_t;

typedef struct dither_color
{
  int nlevels;
  unsigned bit_max;
  unsigned signif_bits;
  dither_segment_t *ranges;
} dither_color_t;

typedef struct dither
{
  int src_width;		/* Input width */
  int dst_width;		/* Output width */

  int density;			/* Desired density, 0-1.0 (scaled 0-65536) */

  int spread;			/* With Floyd-Steinberg, how widely the */
				/* error is distributed.  This should be */
				/* between 12 (very broad distribution) and */
				/* 19 (very narrow) */

  int k_lower;			/* Transition range (lower/upper) for CMY */
  int k_upper;			/* vs. K */

  int c_randomizer;		/* With Floyd-Steinberg dithering, control */
  int m_randomizer;		/* how much randomness is applied to the */
  int y_randomizer;		/* threshold values (0-65536).  With ordered */
  int k_randomizer;		/* dithering, how much randomness is added */
				/* to the matrix value. */

  int k_clevel;			/* Amount of each ink (in 64ths) required */
  int k_mlevel;			/* to create equivalent black */
  int k_ylevel;

  int c_darkness;		/* Perceived "darkness" of each ink, */
  int m_darkness;		/* in 64ths, to calculate CMY-K transitions */
  int y_darkness;

  int dither_type;

  int d_cutoff;			/* When ordered dither is used, threshold */
				/* above which no randomness is used. */
  int adaptive_divisor;
  int adaptive_limit;

  dither_color_t c_dither;
  dither_color_t m_dither;
  dither_color_t y_dither;
  dither_color_t k_dither;

  int *errs[ERROR_ROWS][NCOLORS];
  int *offset0_table;
  int *offset1_table;

  /* Hardwiring these matrices in here is an abomination.  This */
  /* eventually needs to be cleaned up. */
  unsigned ordered_dither_matrix0[MATRIX_SIZE0][MATRIX_SIZE0];
  unsigned ordered_dither_matrix1[MATRIX_SIZE1][MATRIX_SIZE1];
  unsigned ordered_dither_matrix2[MATRIX_SIZE2][MATRIX_SIZE2];
  unsigned ordered_dither_matrix3[MATRIX_SIZE3][MATRIX_SIZE3];
} dither_t;

/*
 * Bayer's dither matrix using Judice, Jarvis, and Ninke recurrence relation
 * http://www.cs.rit.edu/~sxc7922/Project/CRT.htm
 */

static int sq2[] =
{
  0, 2,
  3, 1
};

static int sq3[] =
{
  3, 2, 7,
  8, 4, 0,
  1, 6, 5
};

/*
 * This magic square taken from
 * http://www.pse.che.tohoku.ac.jp/~msuzuki/MagicSquare.5x5.selfsim.html
 *
 * It is magic in the following ways:
 * Rows and columns
 * Major and minor diagonals
 * Self-complementary
 * Four neighbors at distance of 1 or 2 (diagonal or lateral)
 */

static int msq0[] =
{
  00, 14, 21, 17,  8,
  22, 18,  5,  4, 11,
  9,   1, 12, 23, 15,
  13, 20, 19,  6,  2,
  16,  7,  3, 10, 24 
};

static int msq1[] = 
{
  03, 11, 20, 17,  9,
  22, 19,  8,  1, 10,
  06,  0, 12, 24, 18,
  14, 23, 16,  5,  2,
  15,  7,  4, 13, 21
};

#define CALC_MATRIX(matid, init)				\
do {								\
  for (x = 0; x < MATRIX_SIZE##matid; x++)			\
    for (y = 0; y < MATRIX_SIZE##matid; y++)			\
      {								\
	d->ordered_dither_matrix##matid[x][y] =			\
	  calc_ordered_point_n(x, y, MATRIX_NB##matid, 1,	\
			       MATRIX_BASE##matid, init);	\
	d->ordered_dither_matrix##matid[x][y] =			\
	  d->ordered_dither_matrix##matid[x][y] *		\
	  65536 / (MATRIX_SIZE##matid##_2);			\
      }								\
} while (0)

static int
calc_ordered_point_n(unsigned x, unsigned y, int steps, int multiplier,
		     int size, int *map)
{
  int i, j;
  unsigned retval = 0;
  int divisor = 1;
  int div1;
  for (i = 0; i < steps; i++)
    {
      int xa = (x / divisor) % size;
      int ya = (y / divisor) % size;
      unsigned base;
      base = map[ya + (xa * size)];
      div1 = 1;
      for (j = i; j < steps - 1; j++)
	div1 *= size * size;
      retval += base * div1;
      divisor *= size;
    }
  return retval * multiplier;
}


void *
init_dither(int in_width, int out_width, vars_t *v)
{
  int x, y;
  dither_t *d = malloc(sizeof(dither_t));
  simple_dither_range_t r;
  memset(d, 0, sizeof(dither_t));
  r.value = 1.0;
  r.bit_pattern = 1;
  r.is_dark = 1;
  dither_set_c_ranges(d, 1, &r, 1.0);
  dither_set_m_ranges(d, 1, &r, 1.0);
  dither_set_y_ranges(d, 1, &r, 1.0);
  dither_set_k_ranges(d, 1, &r, 1.0);
  d->offset0_table = NULL;
  d->offset1_table = NULL;

  CALC_MATRIX(0, sq2);
  CALC_MATRIX(1, sq3);
  CALC_MATRIX(2, msq0);
  CALC_MATRIX(3, msq1);

  if (!strcmp(v->dither_algorithm, "Hybrid Floyd-Steinberg"))
    d->dither_type = D_FLOYD_HYBRID;
  else if (!strcmp(v->dither_algorithm, "Random Floyd-Steinberg"))
    d->dither_type = D_FLOYD;
  else if (!strcmp(v->dither_algorithm, "Ordered"))
    d->dither_type = D_ORDERED;
  else if (!strcmp(v->dither_algorithm, "Perturbed Ordered"))
    d->dither_type = D_ORDERED_PERTURBED;
  else if (!strcmp(v->dither_algorithm, "Adaptive Hybrid"))
    d->dither_type = D_ADAPTIVE_HYBRID;
  else if (!strcmp(v->dither_algorithm, "Adaptive Random"))
    d->dither_type = D_ADAPTIVE_RANDOM;
  else
    d->dither_type = D_FLOYD_HYBRID;

  d->src_width = in_width;
  d->dst_width = out_width;

  dither_set_ink_spread(d, 13);
  dither_set_black_lower(d, .4);
  dither_set_black_upper(d, .7);
  dither_set_black_levels(d, 1.0, 1.0, 1.0);
  dither_set_randomizers(d, 1.0, 1.0, 1.0, 1.0);
  dither_set_ink_darkness(d, .4, .3, .2);
  dither_set_density(d, 1.0);
  return d;
}  

void
dither_set_density(void *vd, double density)
{
  dither_t *d = (dither_t *) vd;
  if (density > 1)
    density = 1;
  else if (density < 0)
    density = 0;
  d->k_upper = d->k_upper * density;
  d->k_lower = d->k_lower * density;
  d->density = (int) ((65536 * density) + .5);
  d->d_cutoff = d->density / 16;
  d->adaptive_divisor = 128 << ((16 - d->spread) >> 1);
  d->adaptive_limit = d->density / d->adaptive_divisor;
}

void
dither_set_black_lower(void *vd, double k_lower)
{
  dither_t *d = (dither_t *) vd;
  d->k_lower = (int) (k_lower * 65536);
}

void
dither_set_black_upper(void *vd, double k_upper)
{
  dither_t *d = (dither_t *) vd;
  d->k_upper = (int) (k_upper * 65536);
}

void
dither_set_ink_spread(void *vd, int spread)
{
  dither_t *d = (dither_t *) vd;
  if (d->offset0_table)
    {
      free(d->offset0_table);
      d->offset0_table = NULL;
    }
  if (d->offset1_table)
    {
      free(d->offset1_table);
      d->offset1_table = NULL;
    }
  if (spread >= 16)
    {
      d->spread = 16;
    }
  else
    {
      int max_offset;
      int i;
      d->spread = spread;
      max_offset = (1 << (16 - spread)) + 1;
      d->offset0_table = malloc(sizeof(int) * max_offset);
      d->offset1_table = malloc(sizeof(int) * max_offset);
      for (i = 0; i < max_offset; i++)
	{
	  d->offset0_table[i] = (i + 1) * (i + 1);
	  d->offset1_table[i] = ((i + 1) * i) / 2;
	}
    }

  d->adaptive_divisor = 128 << ((16 - d->spread) >> 1);
  d->adaptive_limit = d->density / d->adaptive_divisor;
}

void
dither_set_black_levels(void *vd, double c, double m, double y)
{
  dither_t *d = (dither_t *) vd;
  d->k_clevel = (int) (c * 64);
  d->k_mlevel = (int) (m * 64);
  d->k_ylevel = (int) (y * 64);
}

void
dither_set_randomizers(void *vd, double c, double m, double y, double k)
{
  dither_t *d = (dither_t *) vd;
  d->c_randomizer = c * 65536;
  d->m_randomizer = m * 65536;
  d->y_randomizer = y * 65536;
  d->k_randomizer = k * 65536;
}

void
dither_set_ink_darkness(void *vd, double c, double m, double y)
{
  dither_t *d = (dither_t *) vd;
  d->c_darkness = (int) (c * 64);
  d->m_darkness = (int) (m * 64);
  d->y_darkness = (int) (y * 64);
}

void
dither_set_light_inks(void *vd, double c, double m, double y, double density)
{
  simple_dither_range_t range[2];
  range[0].bit_pattern = 1;
  range[0].is_dark = 0;
  range[1].value = 1;
  range[1].bit_pattern = 1;
  range[1].is_dark = 1;
  if (c > 0)
    {
      range[0].value = c;
      dither_set_c_ranges(vd, 2, range, density);
    }
  if (m > 0)
    {
      range[0].value = m;
      dither_set_m_ranges(vd, 2, range, density);
    }
  if (y > 0)
    {
      range[0].value = y;
      dither_set_y_ranges(vd, 2, range, density);
    }
}

static void
dither_set_ranges(dither_color_t *s, int nlevels,
		  const simple_dither_range_t *ranges, double density)
{
  int i;
  unsigned lbit;
  if (s->ranges)
    free(s->ranges);
  s->nlevels = nlevels > 1 ? nlevels + 1 : nlevels;
  s->ranges = (dither_segment_t *)
    malloc(s->nlevels * sizeof(dither_segment_t));
  s->bit_max = 0;
#if 0
  fprintf(stderr, "dither_set_ranges nlevels %d density %f\n", nlevels, density);
  for (i = 0; i < nlevels; i++)
    fprintf(stderr, "  level %d value %f pattern %x is_dark %d\n", i,
	    ranges[i].value, ranges[i].bit_pattern, ranges[i].is_dark);
#endif
  s->ranges[0].range_l = 0;
  s->ranges[0].value_l = ranges[0].value * 65536.0;
  s->ranges[0].bits_l = ranges[0].bit_pattern;
  s->ranges[0].isdark_l = ranges[0].is_dark;
  if (nlevels == 1)
    s->ranges[0].range_h = 65536;
  else
    s->ranges[0].range_h = ranges[0].value * 65536.0 * density;
  if (s->ranges[0].range_h > 65536)
    s->ranges[0].range_h = 65536;
  s->ranges[0].value_h = ranges[0].value * 65536.0;
  if (s->ranges[0].value_h > 65536)
    s->ranges[0].value_h = 65536;
  s->ranges[0].bits_h = ranges[0].bit_pattern;
  if (ranges[0].bit_pattern > s->bit_max)
    s->bit_max = ranges[0].bit_pattern;
  s->ranges[0].isdark_h = ranges[0].is_dark;
  s->ranges[0].range_span = s->ranges[0].range_h;
  s->ranges[0].value_span = 0;
  if (s->nlevels > 1)
    {
      for (i = 0; i < nlevels - 1; i++)
	{
	  int l = i + 1;
	  s->ranges[l].range_l = s->ranges[i].range_h;
	  s->ranges[l].value_l = s->ranges[i].value_h;
	  s->ranges[l].bits_l = s->ranges[i].bits_h;
	  s->ranges[l].isdark_l = s->ranges[i].isdark_h;
	  if (i == nlevels - 1)
	    s->ranges[l].range_h = 65536;
	  else
	    s->ranges[l].range_h =
	      (ranges[l].value + ranges[l].value) * 65536.0 * density / 2;
	  if (s->ranges[l].range_h > 65536)
	    s->ranges[l].range_h = 65536;
	  s->ranges[l].value_h = ranges[l].value * 65536.0;
	  if (s->ranges[l].value_h > 65536)
	    s->ranges[l].value_h = 65536;
	  s->ranges[l].bits_h = ranges[l].bit_pattern;
	  if (ranges[l].bit_pattern > s->bit_max)
	    s->bit_max = ranges[l].bit_pattern;
	  s->ranges[l].isdark_h = ranges[l].is_dark;
	  s->ranges[l].range_span =
	    s->ranges[l].range_h - s->ranges[l].range_l;
	  s->ranges[l].value_span =
	    s->ranges[l].value_h - s->ranges[l].value_l;
	}
      i++;
      s->ranges[i].range_l = s->ranges[i - 1].range_h;
      s->ranges[i].value_l = s->ranges[i - 1].value_h;
      s->ranges[i].bits_l = s->ranges[i - 1].bits_h;
      s->ranges[i].isdark_l = s->ranges[i - 1].isdark_h;
      s->ranges[i].range_h = 65536;
      s->ranges[i].value_h = s->ranges[i].value_l;
      s->ranges[i].bits_h = s->ranges[i].bits_l;
      s->ranges[i].isdark_h = s->ranges[i].isdark_l;
      s->ranges[i].range_span = s->ranges[i].range_h - s->ranges[i].range_l;
      s->ranges[i].value_span = s->ranges[i].value_h - s->ranges[i].value_l;
    }
  lbit = s->bit_max;
  s->signif_bits = 0;
  while (lbit > 0)
    {
      s->signif_bits++;
      lbit >>= 1;
    }
#if 0
  for (i = 0; i < s->nlevels; i++)
    {
      fprintf(stderr, "    level %d value_l %d value_h %d range_l %d range_h %d\n",
	      i, s->ranges[i].value_l, s->ranges[i].value_h,
	      s->ranges[i].range_l, s->ranges[i].range_h);
      fprintf(stderr, "       bits_l %d bits_h %d isdark_l %d isdark_h %d\n",
	      s->ranges[i].bits_l, s->ranges[i].bits_h,
	      s->ranges[i].isdark_l, s->ranges[i].isdark_h);
      fprintf(stderr, "       rangespan %d valuespan %d\n",
	      s->ranges[i].range_span, s->ranges[i].value_span);
    }
  fprintf(stderr, "  bit_max %d signif_bits %d\n", s->bit_max, s->signif_bits);
#endif
}

void
dither_set_c_ranges(void *vd, int nlevels, const simple_dither_range_t *ranges,
		    double density)
{
  dither_t *d = (dither_t *) vd;
  dither_set_ranges(&(d->c_dither), nlevels, ranges, density);
}

void
dither_set_c_ranges_simple(void *vd, int nlevels, const double *levels,
			   double density)
{
  simple_dither_range_t *r = malloc(nlevels * sizeof(simple_dither_range_t));
  int i;
  for (i = 0; i < nlevels; i++)
    {
      r[i].bit_pattern = i + 1;
      r[i].value = levels[i];
      r[i].is_dark = 1;
    }
  dither_set_c_ranges(vd, nlevels, r, density);
  free(r);
}

void
dither_set_m_ranges(void *vd, int nlevels, const simple_dither_range_t *ranges,
		    double density)
{
  dither_t *d = (dither_t *) vd;
  dither_set_ranges(&(d->m_dither), nlevels, ranges, density);
}

void
dither_set_m_ranges_simple(void *vd, int nlevels, const double *levels,
			   double density)
{
  simple_dither_range_t *r = malloc(nlevels * sizeof(simple_dither_range_t));
  int i;
  for (i = 0; i < nlevels; i++)
    {
      r[i].bit_pattern = i + 1;
      r[i].value = levels[i];
      r[i].is_dark = 1;
    }
  dither_set_m_ranges(vd, nlevels, r, density);
  free(r);
}
  
void
dither_set_y_ranges(void *vd, int nlevels, const simple_dither_range_t *ranges,
		    double density)
{
  dither_t *d = (dither_t *) vd;
  dither_set_ranges(&(d->y_dither), nlevels, ranges, density);
}

void
dither_set_y_ranges_simple(void *vd, int nlevels, const double *levels,
			   double density)
{
  simple_dither_range_t *r = malloc(nlevels * sizeof(simple_dither_range_t));
  int i;
  for (i = 0; i < nlevels; i++)
    {
      r[i].bit_pattern = i + 1;
      r[i].value = levels[i];
      r[i].is_dark = 1;
    }
  dither_set_y_ranges(vd, nlevels, r, density);
  free(r);
}
  
void
dither_set_k_ranges(void *vd, int nlevels, const simple_dither_range_t *ranges,
		    double density)
{
  dither_t *d = (dither_t *) vd;
  dither_set_ranges(&(d->k_dither), nlevels, ranges, density);
}

void
dither_set_k_ranges_simple(void *vd, int nlevels, const double *levels,
			   double density)
{
  simple_dither_range_t *r = malloc(nlevels * sizeof(simple_dither_range_t));
  int i;
  for (i = 0; i < nlevels; i++)
    {
      r[i].bit_pattern = i + 1;
      r[i].value = levels[i];
      r[i].is_dark = 1;
    }
  dither_set_k_ranges(vd, nlevels, r, density);
  free(r);
}
  

void
free_dither(void *vd)
{
  dither_t *d = (dither_t *) vd;
  int i;
  int j;
  for (i = 0; i < ERROR_ROWS; i++)
    {
      for (j = 0; j < NCOLORS; j++)
	{
	  if (d->errs[i][j])
	    {
	      free(d->errs[i][j]);
	      d->errs[i][j] = NULL;
	    }
	}
    }
  free(d->c_dither.ranges);
  d->c_dither.ranges = NULL;
  free(d->m_dither.ranges);
  d->m_dither.ranges = NULL;
  free(d->y_dither.ranges);
  d->y_dither.ranges = NULL;
  free(d->k_dither.ranges);
  d->k_dither.ranges = NULL;
  if (d->offset0_table)
    {
      free(d->offset0_table);
      d->offset0_table = NULL;
    }
  if (d->offset1_table)
    {
      free(d->offset1_table);
      d->offset1_table = NULL;
    }
  free(d);
}

static int *
get_errline(dither_t *d, int row, int color)
{
  if (row < 0 || color < 0 || color >= NCOLORS)
    return NULL;
  if (d->errs[row & 1][color])
    return d->errs[row & 1][color];
  else
    {
      int size = (16 * ((d->dst_width + 7) / 8));
      d->errs[row & 1][color] = malloc(size * sizeof(int));
      memset(d->errs[row & 1][color], 0, size * sizeof(int));
      return d->errs[row & 1][color];
    }
}

/*
 * Dithering macros (shared between routines)
 */

/*
 * Advance to the next point
 */

#define INCREMENT_BLACK()			\
do {						\
  if (direction == 1)				\
    {						\
      if (bit == 1)				\
	{					\
	  kptr ++;				\
	  bit = 128;				\
	}					\
      else					\
	bit >>= 1;				\
    }						\
  else						\
    {						\
      if (bit == 128)				\
	{					\
	  kptr --;				\
	  bit = 1;				\
	}					\
      else					\
	bit <<= 1;				\
    }						\
						\
  gray   += xstep;				\
  xerror += xmod;				\
  if (xerror >= d->dst_width)			\
    {						\
      xerror -= d->dst_width;			\
      gray   += direction;			\
    }						\
  else if (xerror < 0)				\
    {						\
      xerror += d->dst_width;			\
      gray   += direction;			\
    }						\
} while (0)

#define INCREMENT_COLOR()						  \
do {									  \
  if (direction == 1)							  \
    {									  \
      if (bit == 1)							  \
	{								  \
	  cptr ++;							  \
	  if (lcptr)							  \
	    lcptr ++;							  \
	  mptr ++;							  \
	  if (lmptr)							  \
	    lmptr ++;							  \
	  yptr ++;							  \
	  if (lyptr)							  \
	    lyptr ++;							  \
	  if (kptr)							  \
	    kptr ++;							  \
	  bit       = 128;						  \
	}								  \
      else								  \
	bit >>= 1;							  \
    }									  \
  else									  \
    {									  \
      if (bit == 128)							  \
	{								  \
	  cptr --;							  \
	  if (lcptr)							  \
	    lcptr --;							  \
	  mptr --;							  \
	  if (lmptr)							  \
	    lmptr --;							  \
	  yptr --;							  \
	  if (lyptr)							  \
	    lyptr --;							  \
	  if (kptr)							  \
	    kptr --;							  \
	  bit       = 1;						  \
	}								  \
      else								  \
	bit <<= 1;							  \
    }									  \
									  \
  rgb    += xstep;							  \
  xerror += xmod;							  \
  if (xerror >= d->dst_width)						  \
    {									  \
      xerror -= d->dst_width;						  \
      rgb    += 3 * direction;						  \
    }									  \
  else if (xerror < 0)							  \
    {									  \
      xerror += d->dst_width;						  \
      rgb    += 3 * direction;						  \
    }      								  \
} while (0)

/*
 * Add the error to the input value.  Notice that we micro-optimize this
 * to save a division when appropriate.
 */

#define UPDATE_COLOR(r)				\
do {						\
  if (!(d->dither_type & D_ORDERED_BASE))	\
    {						\
      if (dither##r >= 0)			\
	r += dither##r >> 3;			\
      else					\
	r += dither##r / 8;			\
    }						\
} while (0)

/*
 * For Floyd-Steinberg, distribute the error residual.  We spread the
 * error to nearby points, spreading more broadly in lighter regions to
 * achieve more uniform distribution of color.  The actual distribution
 * is a triangular function.
 */

static int
update_dither(int r, int o, int x, int width, int odb, int odb_mask,
	      int xdw1, int direction, int *error0, int *error1,
	      dither_t *d)
{
  int tmp = r;
  if (tmp != 0)
    {
      int i, dist;
      int dist1;
      int offset;
      int delta, delta1;
      int myspread;
      if (odb >= 16 || o >= 2048)
	offset = 0;
      else
	{
	  int tmpo = o * 32;
	  offset = (65535 - (tmpo & 0xffff)) >> odb;
	  if ((rand() & odb_mask) > (tmpo & odb_mask))
	    offset++;
	  if (offset > x)
	    offset = x;
	  else if (offset > xdw1)
	    offset = xdw1;
	}
      if (tmp > 65535)
	tmp = 65535;
      myspread = 4;
      if (offset == 0)
	{
	  dist = myspread * tmp;
	  if ((x > 0 && direction < 0) || (xdw1 > 0 && direction > 0))
	    error0[direction] += (8 - myspread) * tmp;
	  delta1 = 0;
	  dist1 = 0;
	}
      else
	{
	  dist = myspread * tmp / d->offset0_table[offset];
	  dist1 = (8 - myspread) * tmp / d->offset1_table[offset];
	  delta1 = dist1 * offset;
	}
      delta = dist;
      for (i = -offset; i <= offset; i++)
	{
	  error1[i] += delta;
	  if ((i > 0 && direction > 0) || (i < 0 && direction < 0))
	    {
	      error0[i] += delta1;
	      delta1 -= dist1;
	    }
	  if (i < 0)
	    delta += dist;
	  else
	    delta -= dist;
	}
    }
  if ((x > 0 && direction < 0) || (xdw1 > 0 && direction > 0))
    return error0[direction];
  else
    return 0;
}

/*
 * Print a single dot.  This routine has become awfully complicated
 * awfully fast!
 */

static int
print_color(dither_t *d, dither_color_t *rv, int base, int density,
	    int adjusted, int x, int y, unsigned char *c, unsigned char *lc,
	    unsigned char bit, int length, int invert_x, int invert_y,
	    unsigned randomizer, int dontprint)
{
  int i;
  int levels = rv->nlevels - 1;
  if (adjusted <= 0 || base == 0 || density == 0)
    return adjusted;
  if (density > 65536)
    density = 65536;

  /*
   * Look for the appropriate range into which the input value falls.
   * Notice that we use the input, not the error, to decide what dot type
   * to print (if any).  We actually use the "density" input to permit
   * the caller to use something other that simply the input value, if it's
   * desired to use some function of overall density, rather than just
   * this color's input, for this purpose.
   */
  for (i = levels; i >= 0; i--)
    {
      dither_segment_t *dd = &(rv->ranges[i]);
      int dither_value = adjusted;
      unsigned rangepoint;
      unsigned virtual_value;
      unsigned vmatrix;
      int dither_type = d->dither_type;
      if (density <= dd->range_l)
	continue;

      /*
       * If we're using an adaptive dithering method, decide whether
       * to use the Floyd-Steinberg or the ordered method based on the
       * input value.  The choice of 1/128 is somewhat arbitrary and
       * could stand to be parameterized.  Another possibility would be
       * to scale to something less than pure ordered at 0 input value.
       */
      if (dither_type & D_ADAPTIVE_BASE)
	{
	  dither_type -= D_ADAPTIVE_BASE;
	  if (base < d->adaptive_limit)
	    {
	      unsigned dtmp = base * d->adaptive_divisor * 65536 / d->density;
	      if (((rand() & 0xffff000) >> 12) > dtmp)
		{
		  dither_type = D_ORDERED;
		  dither_value = base;
		}
	    }
	}

      /*
       * Where are we within the range.  If we're going to print at
       * all, this determines the probability of printing the darker
       * vs. the lighter ink.  If the inks are identical (same value
       * and darkness), it doesn't matter.
       *
       * We scale the input linearly against the top and bottom of the
       * range.
       */
      if (dd->range_span == 0 ||
	  (dd->value_span == 0 && dd->isdark_l == dd->isdark_h))
	rangepoint = 32768;
      else
	rangepoint =
	  ((unsigned) (density - dd->range_l)) * 65536 / dd->range_span;

      /*
       * Compute the virtual dot size that we're going to print.
       * This is somewhere between the two candidate dot sizes.
       * This is scaled between the high and low value.
       */

      if (dd->value_span == 0)
	virtual_value = dd->value_h;
      else if (dd->range_span == 0)
	virtual_value = (dd->value_h + dd->value_l) / 2;
      else if (dd->value_h == 65536 && rangepoint == 65536)
	virtual_value = 65536;
      else
	virtual_value = dd->value_l + (dd->value_span * rangepoint / 65536);

      /*
       * Reduce the randomness as the base value increases, to get
       * smoother output in the midtones.  Idea suggested by
       * Thomas Tonino.
       */
      if (!(dither_type & D_ORDERED_BASE))
	{
	  if (base > d->d_cutoff)
	    randomizer = 0;
	  else if (base > d->d_cutoff / 2)
	    randomizer = randomizer * 2 * (d->d_cutoff - base) / d->d_cutoff;
	}
      else
	randomizer = 65536;	/* With ordered dither, we need this */

      /*
       * A hack to get a bit more choice out of a single matrix.
       * Fiddle the x and y coordinates.
       */
      if (invert_y)
	{
	  unsigned tmp = x;
	  x = y + 33;
	  y = tmp + 29;
	      
	}

      /*
       * Compute the comparison value to decide whether to print at
       * all.  If there is no randomness, simply divide the virtual
       * dotsize by 2 to get standard "pure" Floyd-Steinberg (or "pure"
       * matrix dithering, which degenerates to a threshold).
       */
      if (randomizer == 0)
	vmatrix = virtual_value / 2;
      else
	{
	  /*
	   * First, compute a value between 0 and 65536 that will be
	   * scaled to produce an offset from the desired threshold.
	   */
	  switch (dither_type)
	    {
	    case D_FLOYD:
	      /*
	       * Floyd-Steinberg: use a mildly Gaussian random number.
	       * This might be a bit too Gaussian.
	       */
	      vmatrix = ((rand() & 0xffff000) +
			 (rand() & 0xffff000)) >> 13;
	      break;
	    case D_FLOYD_HYBRID:
	      /*
	       * Hybrid Floyd-Steinberg: use a matrix (or a really ugly
	       * combination of matrices) to generate the offset.
	       */
	      vmatrix = DITHERPOINT(d, x, y, 1) ^ DITHERPOINT(d, x, y, 2);
	      break;
	    case D_ORDERED:
	    case D_ORDERED_PERTURBED:
	    default:
	      /*
	       * Ordered: again, we use a matrix to generate the offset.
	       * This time, however, we use a different matrix.
	       * We also generate some random low-order bits to ensure that
	       * even very small values have a chance to print.
	       */
	      {
		int imatrix;
		int rand0 = rand();
		int ix, iy;
		if (dither_type == D_ORDERED_PERTURBED)
		  {
		    /*
		     * "Twist" the matrix to break up lines.  This is
		     * somewhat peculiar to the iterated-2 matrix we've
		     * chosen.  A better matrix may not need this.
		     */
		    ix = x + y / (((x / 11) % 7) + 3);
		    iy = y + x / (((y / 11) % 7) + 3);
		  }
		else
		  {
		    /*
		     * Improve the iterated-2 matrix.  A better matrix
		     * may not need this treatment.
		     */
		    ix = x + y / 3;
		    iy = y + x / 3;
		  }
		imatrix = DITHERPOINT(d, ix, iy, 0);

		/*
		 * Your low order bits, sir...
		 */
		rand0 = rand();
		imatrix += (rand0 + (rand0 >> 7) +
			    (rand0 >> 14) + (rand0 >> 21)) & 127;
		imatrix -= 63;
		if (imatrix < 0)
		  vmatrix = 0;
		else if (imatrix > 65536)
		  vmatrix = 65536;
		else
		  vmatrix = imatrix;
	      }
	    }

	  /*
	   * Another way to get more use out of the matrix.  If the
	   * matrix is evenly distributed, it doesn't matter which way
	   * we do the comparison.  At this point vmatrix is simply
	   * a number between 0 and 65536; subtracting it from 65536
	   * won't change anything.
	   */
	  if (invert_x)
	    vmatrix = 65536 - vmatrix;

	  if (vmatrix == 65536 && virtual_value == 65536)
	    /*
	     * These numbers will break 32-bit unsigned arithmetic!
	     * Maybe this is so rare that we'd be better off using
	     * long long arithmetic, but that's likely to be much more
	     * expensive on 32-bit architectures.
	     */
	    vmatrix = 32768;
	  else
	    {
	      /*
	       * Now, scale the virtual dot size appropriately.  Note that
	       * we'll get something evenly distributed between 0 and
	       * the virtual dot size, centered on the dot size / 2,
	       * which is the normal threshold value.
	       */
	      vmatrix = vmatrix * virtual_value / 65536;
	      if (randomizer != 65536)
		{
		  /*
		   * We want vmatrix to be scaled between 0 and
		   * virtual_value when randomizer is 65536 (fully random).
		   * When it's less, we want it to scale through part of
		   * that range. In all cases, it should center around
		   * virtual_value / 2.
		   *
		   * vbase is the bottom of the scaling range.
		   */
		  unsigned vbase = virtual_value * (65536u - randomizer) /
		    131072u;
		  vmatrix = vmatrix * randomizer / 65536;
		  vmatrix += vbase;
		}
	    }
	} /* randomizer != 0 */

      /*
       * After all that, printing is almost an afterthought.
       * Pick the actual dot size (using a matrix here) and print it.
       */
      if (dither_value >= vmatrix)
	{
	  int j;
	  int isdark;
	  unsigned char *tptr;
	  unsigned bits;
	  unsigned v;
	      
	  if (dd->isdark_h == dd->isdark_l && dd->bits_h == dd->bits_l)
	    {
	      isdark = dd->isdark_h;
	      bits = dd->bits_h;
	      v = dd->value_h;
	    }
	  else if (rangepoint >= DITHERPOINT(d, x, y, 3))
	    {
	      isdark = dd->isdark_h;
	      bits = dd->bits_h;
	      v = dd->value_h;
	    }
	  else
	    {
	      isdark = dd->isdark_l;
	      bits = dd->bits_l;
	      v = dd->value_l;
	    }
	  tptr = isdark ? c : lc;

	  /*
	   * Lay down all of the bits in the pixel.
	   */
	  if (!dontprint)
	    {
	      for (j = 1; j <= bits; j += j, tptr += length)
		{
		  if (j & bits)
		    *tptr |= bit;
		}
	    }
	  adjusted -= v;
	}
      return adjusted;
    }
  return adjusted;
}

/*
 * Dithering functions!
 *
 * Documentation moved to README.dither
 */

/*
 * 'dither_fastblack()' - Dither grayscale pixels to black using a hard
 * threshold.  This is for use with predithered output, or for text
 * or other pure black and white only.
 */

void
dither_fastblack(unsigned short     *gray,	/* I - Grayscale pixels */
		 int           	    row,	/* I - Current Y coordinate */
		 void 		    *vd,
		 unsigned char 	    *black)	/* O - Black bitmap pixels */
{
  int		x,		/* Current X coordinate */
		xerror,		/* X error count */
		xstep,		/* X step */
		xmod,		/* X error modulus */
		length;		/* Length of output bitmap in bytes */
  unsigned char	bit,		/* Current bit */
		*kptr;		/* Current black pixel */
  int		k;		/* Current black error */
  dither_t *d = (dither_t *) vd;
  int terminate;
  int direction = row & 1 ? 1 : -1;

  bit = (direction == 1) ? 128 : 1 << (7 - ((d->dst_width - 1) & 7));
  x = (direction == 1) ? 0 : d->dst_width - 1;
  terminate = (direction == 1) ? d->dst_width : -1;

  xstep  = d->src_width / d->dst_width;
  xmod   = d->src_width % d->dst_width;
  length = (d->dst_width + 7) / 8;

  memset(black, 0, length * d->k_dither.signif_bits);
  kptr = black;
  xerror = 0;
  if (direction == -1)
    {
      kptr = black + length - 1;
      xstep = -xstep; 
      gray += d->src_width - 1;
      xerror = ((d->dst_width - 1) * xmod) % d->dst_width;
      xmod = -xmod;
    }

  for (; x != terminate; x += direction)
    {
      k = 65535 - *gray;

      if (k >= 32768)
	{
	  if (d->density >= DITHERPOINT(d, x + row / 3, row + x / 3, 0))
	    *kptr |= bit;
	}

      INCREMENT_BLACK();
    }
}

/*
 * 'dither_black_n()' - Dither grayscale pixels to n levels of black.
 * This is for grayscale output.
 */

void
dither_black(unsigned short   *gray,		/* I - Grayscale pixels */
	     int           	row,		/* I - Current Y coordinate */
	     void 		*vd,
	     unsigned char 	*black)		/* O - Black bitmap pixels */
{
  int		x,		/* Current X coordinate */
		xerror,		/* X error count */
		xstep,		/* X step */
		xmod,		/* X error modulus */
		length;		/* Length of output bitmap in bytes */
  unsigned char	bit,		/* Current bit */
		*kptr;		/* Current black pixel */
  int		k, ok,		/* Current black error */
		ditherk,	/* Next error value in buffer */
		*kerror0,	/* Pointer to current error row */
		*kerror1;	/* Pointer to next error row */
  dither_t *d = (dither_t *) vd;
  int terminate;
  int direction = row & 1 ? 1 : -1;
  int odb = d->spread;
  int odb_mask = (1 << odb) - 1;
  int ddw1 = d->dst_width - 1;

  bit = (direction == 1) ? 128 : 1 << (7 - ((d->dst_width - 1) & 7));
  x = (direction == 1) ? 0 : d->dst_width - 1;
  terminate = (direction == 1) ? d->dst_width : -1;

  xstep  = d->src_width / d->dst_width;
  xmod   = d->src_width % d->dst_width;
  length = (d->dst_width + 7) / 8;

  kerror0 = get_errline(d, row, ECOLOR_K);
  kerror1 = get_errline(d, row + 1, ECOLOR_K);
  memset(kerror1, 0, d->dst_width * sizeof(int));

  memset(black, 0, length * d->k_dither.signif_bits);
  kptr = black;
  xerror = 0;
  if (direction == -1)
    {
      kerror0 += d->dst_width - 1;
      kerror1 += d->dst_width - 1;
      kptr = black + length - 1;
      xstep = -xstep;
      gray += d->src_width - 1;
      xerror = ((d->dst_width - 1) * xmod) % d->dst_width;
      xmod = -xmod;
    }

  for (ditherk = kerror0[0];
       x != terminate;
       x += direction,
	 kerror0 += direction,
	 kerror1 += direction)
  {
    int xdw1 = ddw1 - x;

    k = 65535 - *gray;
    ok = k;
    if (d->dither_type & D_ORDERED_BASE)
      print_color(d, &(d->k_dither), k, k, k, x, row, kptr, NULL, bit,
		  length, 0, 0, d->k_randomizer, 0);
    else
      {
	UPDATE_COLOR(k);
	k = print_color(d, &(d->k_dither), ok, ok, k, x, row, kptr, NULL, bit,
			length, 0, 0, d->k_randomizer, 0);
	if (!(d->dither_type & D_ORDERED_BASE))
	  ditherk = update_dither(k, ok, x, d->src_width, odb, odb_mask, xdw1,
				  direction, kerror0, kerror1, d);
      }

    INCREMENT_BLACK();
  }
}

/*
 * 'dither_cmyk_n()' - Dither RGB pixels to n levels of cyan, magenta, yellow,
 *                     and black.
 */

void
dither_cmyk(unsigned short  *rgb,	/* I - RGB pixels */
	    int           row,	/* I - Current Y coordinate */
	    void 	    *vd,
	    unsigned char *cyan,	/* O - Cyan bitmap pixels */
	    unsigned char *lcyan,	/* O - Light cyan bitmap pixels */
	    unsigned char *magenta,	/* O - Magenta bitmap pixels */
	    unsigned char *lmagenta,	/* O - Light magenta bitmap pixels */
	    unsigned char *yellow,	/* O - Yellow bitmap pixels */
	    unsigned char *lyellow,	/* O - Light yellow bitmap pixels */
	    unsigned char *black)	/* O - Black bitmap pixels */
{
  int		x,		/* Current X coordinate */
		xerror,		/* X error count */
		xstep,		/* X step */
		xmod,		/* X error modulus */
		length;		/* Length of output bitmap in bytes */
  int		c, m, y, k,	/* CMYK values */
		oc, om, ok, oy;
  int   	diff;		/* Average color difference */
  unsigned char	bit,		/* Current bit */
		*cptr,		/* Current cyan pixel */
		*mptr,		/* Current magenta pixel */
		*yptr,		/* Current yellow pixel */
		*lmptr,		/* Current light magenta pixel */
		*lcptr,		/* Current light cyan pixel */
		*lyptr,		/* Current light yellow pixel */
		*kptr;		/* Current black pixel */
  int		ditherc,	/* Next error value in buffer */
		*cerror0,	/* Pointer to current error row */
		*cerror1;	/* Pointer to next error row */
  int		dithery,	/* Next error value in buffer */
		*yerror0,	/* Pointer to current error row */
		*yerror1;	/* Pointer to next error row */
  int		ditherm,	/* Next error value in buffer */
		*merror0,	/* Pointer to current error row */
		*merror1;	/* Pointer to next error row */
  int		ditherk,	/* Next error value in buffer */
		*kerror0,	/* Pointer to current error row */
		*kerror1;	/* Pointer to next error row */
  int		ditherbit;	/* Random dither bitmask */
  int		ck;
  int		bk = 0;
  int		ub, lb;
  dither_t	*d = (dither_t *) vd;

  int		terminate;
  int		direction = row & 1 ? 1 : -1;
  int		odb = d->spread;
  int		odb_mask = (1 << odb) - 1;
  int		ddw1 = d->dst_width - 1;

  bit = (direction == 1) ? 128 : 1 << (7 - ((d->dst_width - 1) & 7));
  x = (direction == 1) ? 0 : d->dst_width - 1;
  terminate = (direction == 1) ? d->dst_width : -1;

  xstep  = 3 * (d->src_width / d->dst_width);
  xmod   = d->src_width % d->dst_width;
  length = (d->dst_width + 7) / 8;

  cerror0 = get_errline(d, row, ECOLOR_C);
  cerror1 = get_errline(d, row + 1, ECOLOR_C);

  merror0 = get_errline(d, row, ECOLOR_M);
  merror1 = get_errline(d, row + 1, ECOLOR_M);

  yerror0 = get_errline(d, row, ECOLOR_Y);
  yerror1 = get_errline(d, row + 1, ECOLOR_Y);

  kerror0 = get_errline(d, row, ECOLOR_K);
  kerror1 = get_errline(d, row + 1, ECOLOR_K);
  memset(kerror1, 0, d->dst_width * sizeof(int));
  memset(cerror1, 0, d->dst_width * sizeof(int));
  memset(merror1, 0, d->dst_width * sizeof(int));
  memset(yerror1, 0, d->dst_width * sizeof(int));
  cptr = cyan;
  mptr = magenta;
  yptr = yellow;
  lcptr = lcyan;
  lmptr = lmagenta;
  lyptr = lyellow;
  kptr = black;
  xerror = 0;
  if (direction == -1)
    {
      cerror0 += d->dst_width - 1;
      cerror1 += d->dst_width - 1;
      merror0 += d->dst_width - 1;
      merror1 += d->dst_width - 1;
      yerror0 += d->dst_width - 1;
      yerror1 += d->dst_width - 1;
      kerror0 += d->dst_width - 1;
      kerror1 += d->dst_width - 1;
      cptr = cyan + length - 1;
      if (lcptr)
	lcptr = lcyan + length - 1;
      mptr = magenta + length - 1;
      if (lmptr)
	lmptr = lmagenta + length - 1;
      yptr = yellow + length - 1;
      if (lyptr)
	lyptr = lyellow + length - 1;
      if (kptr)
	kptr = black + length - 1;
      xstep = -xstep;
      rgb += 3 * (d->src_width - 1);
      xerror = ((d->dst_width - 1) * xmod) % d->dst_width;
      xmod = -xmod;
    }

  memset(cyan, 0, length * d->c_dither.signif_bits);
  if (lcyan)
    memset(lcyan, 0, length * d->c_dither.signif_bits);
  memset(magenta, 0, length * d->m_dither.signif_bits);
  if (lmagenta)
    memset(lmagenta, 0, length * d->m_dither.signif_bits);
  memset(yellow, 0, length * d->y_dither.signif_bits);
  if (lyellow)
    memset(lyellow, 0, length * d->y_dither.signif_bits);
  if (black)
    memset(black, 0, length * d->k_dither.signif_bits);

  /*
   * Main loop starts here!
   */
  for (ditherc = cerror0[0], ditherm = merror0[0], dithery = yerror0[0],
	 ditherk = kerror0[0];
       x != terminate;
       x += direction,
	 cerror0 += direction,
	 cerror1 += direction,
	 merror0 += direction,
	 merror1 += direction,
	 yerror0 += direction,
	 yerror1 += direction,
	 kerror0 += direction,
	 kerror1 += direction)
    {
      int xdw1 = ddw1 - x;	/* For error distribution */
      int ak;
      int kdarkness;
      int tk;
      int printed_black = 0;
      int omd, oyd, ocd;

      /*
       * First compute the standard CMYK separation color values...
       */

      c = 65535 - (unsigned) rgb[0];
      m = 65535 - (unsigned) rgb[1];
      y = 65535 - (unsigned) rgb[2];
      oc = c;
      om = m;
      oy = y;
      k = MIN(c, MIN(m, y));

      if (black != NULL)
	{
	  /*
	   * Since we're printing black, adjust the black level based upon
	   * the amount of color in the pixel (colorful pixels get less
	   * black)...
	   */
	  int xdiff = (IABS(c - m) + IABS(c - y) + IABS(m - y)) / 3;

	  diff = 65536 - xdiff;
	  diff = ((long long) diff * (long long) diff * (long long) diff)
	    >> 32;
	  diff--;
	  if (diff < 0)
	    diff = 0;
	  k = (int) (((unsigned) diff * (unsigned) k) >> 16);
	  ok = k;
	  ak = k;

	  /*
	   * kdarkness is an artificially computed darkness value for deciding
	   * how much black vs. CMY to use for the k component.  This is
	   * empirically determined.
	   *
	   * Above k_upper, we print black components with all black ink.
	   * Below k_lower, we print black components with all color inks.
	   * In between we scale.  We actually choose, for each point,
	   * whether we're going to print black or color.
	   */
#if 0
	  tk = (((oc * d->c_darkness) + (om * d->m_darkness) +
		 (oy * d->y_darkness)) >> 6);
#else
	  tk = k;
#endif
	  kdarkness = tk;
	  if (kdarkness < d->k_upper) /* Possibility of printing color */
	    {
	      int rb;
	      ub = d->k_upper;	/* Upper bound */
	      lb = d->k_lower;	/* Lower bound */
	      rb = ub - lb;	/* Range */
	      if (kdarkness <= lb) /* All color */
		{
		  bk = 0;
		  ub = 0;
		  lb = 1;
		}
	      else if (kdarkness < ub) /* Probabilistic */
		{
		  /*
		   * Pick a range point, depending upon which dither
		   * method we're using
		   */
		  if ((d->dither_type & ~D_ADAPTIVE_BASE) == D_FLOYD)
		    ditherbit = ((rand() & 0xffff000) >> 12);
		  else
		    ditherbit = (DITHERPOINT(d, row, x, 1) ^
				 (DITHERPOINT(d, row, x, 3) >> 2));
		  ditherbit = ditherbit * rb / 65536;
		  if (rb == 0 || (ditherbit < (kdarkness - lb)))
		    bk = ok;
		  else
		    bk = 0;
		}
	      else		/* All black */
		{
		  ub = 1;
		  lb = 1;
		  bk = ok;
		}
	    }
	  else			/* All black */
	    {
	      bk = ok;
	    }
	  ck = ok - bk;
    
	  if (bk > 0)
	    {
	      c -= (d->k_clevel * bk) >> 6;
	      m -= (d->k_mlevel * bk) >> 6;
	      y -= (d->k_ylevel * bk) >> 6;
	      if (c < 0)
		c = 0;
	      if (m < 0)
		m = 0;
	      if (y < 0)
		y = 0;
	    }

	  k = bk;
	  UPDATE_COLOR(k);
	  tk = print_color(d, &(d->k_dither), bk, bk, k, x, row, kptr,
			   NULL, bit, length, 0, 0, 0, 0);
	  if (tk != k)
	    printed_black = 1;
	  k = tk;
	  if (!(d->dither_type & D_ORDERED_BASE))
	    ditherk = update_dither(k, ok, x, d->src_width, odb, odb_mask,
				    xdw1, direction, kerror0, kerror1, d);
	}
      else
	{
	  /*
	   * We're not printing black, but let's adjust the CMY levels to
	   * produce better reds, greens, and blues...
	   *
	   * This code needs to be tuned
	   */

	  unsigned ck = c - k;
	  unsigned mk = m - k;
	  unsigned yk = y - k;

	  ok = 0;
	  c  = ((unsigned) (65535 - rgb[1] / 4)) * ck / 65535 + k;
	  m  = ((unsigned) (65535 - rgb[2] / 4)) * mk / 65535 + k;
	  y  = ((unsigned) (65535 - rgb[0] / 4)) * yk / 65535 + k;
	}

      /*
       * Done handling the black.  Now print the color.
       * Isn't this easy by comparison?
       */
      UPDATE_COLOR(c);
      UPDATE_COLOR(m);
      UPDATE_COLOR(y);

      ocd = oc * d->c_darkness;
      omd = om * d->m_darkness;
      oyd = oy * d->y_darkness;
      c = print_color(d, &(d->c_dither), oc, oc + ((omd + oyd) >> 7),
		      c, x, row, cptr, lcptr, bit, length, 0, 1,
		      d->c_randomizer, printed_black);
      m = print_color(d, &(d->m_dither), om, om + ((ocd + oyd) >> 7),
		      m, x, row, mptr, lmptr, bit, length, 1, 0,
		      d->m_randomizer, printed_black);
      y = print_color(d, &(d->y_dither), oy, oy + ((ocd + omd) >> 7),
		      y, x, row, yptr, lyptr, bit, length, 1, 1,
		      d->y_randomizer, printed_black);

      if (!(d->dither_type & D_ORDERED_BASE))
	{
	  ditherc = update_dither(c, oc, x, d->src_width, odb, odb_mask, xdw1,
				  direction, cerror0, cerror1, d);
	  ditherm = update_dither(m, om, x, d->src_width, odb, odb_mask, xdw1,
				  direction, merror0, merror1, d);
	  dithery = update_dither(y, oy, x, d->src_width, odb, odb_mask, xdw1,
				  direction, yerror0, yerror1, d);
	}

      /*****************************************************************
       * Advance the loop
       *****************************************************************/

      INCREMENT_COLOR();
    }
  /*
   * Main loop ends here!
   */
}

/*
 *   $Log: print-dither.c,v $
 *   Revision 1.38  2000/05/06 02:12:35  rlk
 *   Convert UPDATE_DITHER from a macro to a function.  This should make it
 *   easier to profile this code (it needs it, badly!).  We can always make
 *   this inline if need be.
 *
 *   Point optimizations in dither_cmyk.
 *
 *   Silly bug in hybrid Floyd-Steinberg.  It actually looks quite nice
 *   now!
 *
 *   Revision 1.37  2000/05/05 02:41:41  rlk
 *   Minor cleanup
 *
 *   Revision 1.36  2000/05/03 23:17:14  rlk
 *   Avoid out of range problems
 *
 *   Revision 1.35  2000/05/02 11:33:57  rlk
 *   Improved dither code.  Deposits significantly less ink than previous version,
 *   and gives better saturation.
 *
 *   Revision 1.34  2000/04/29 01:14:19  rlk
 *   Improve photo and line art mode
 *
 *   Revision 1.33  2000/04/27 02:07:53  rlk
 *   Comments
 *
 *   Revision 1.32  2000/04/27 00:24:24  rlk
 *   Add Thomas Tonino's 23x23 matrix
 *
 *   Revision 1.31  2000/04/26 02:52:02  rlk
 *   Minor improvements
 *
 *   Revision 1.30  2000/04/24 01:56:37  rlk
 *   Give the primary more weight, and the other colors less weight, in the
 *   density calculation.  The previous weights resulted in too much dark
 *   ink mixing in, which caused excessive graininess in some situations.
 *
 *   Revision 1.29  2000/04/24 01:04:26  rlk
 *   fix warning in gtk_main_window.c
 *
 *   Add perturbed ordered dither (perturbed to break up the fine diagonal
 *   lines characteristic of the matrix used).
 *
 *   Improve transfer function for Epson photo printers (change the
 *   constants).
 *
 *   Revision 1.28  2000/04/22 23:28:55  rlk
 *   Adaptive algorithms.  These are the normal random and hybrid Floyd-Steinberg
 *   algorithms except in very pale regions, where ordered dithering is used to
 *   improve smoothness.
 *
 *   Revision 1.27  2000/04/22 03:57:47  rlk
 *   Break up ordered dither pattern a bit.
 *
 *   Fix Ghostscript driver slightly
 *
 *   Revision 1.26  2000/04/22 03:29:50  rlk
 *   Try to vary the randomness -- more random at paler colors.
 *
 *   Revision 1.25  2000/04/20 02:42:54  rlk
 *   Reduce initial memory footprint.
 *
 *   Add random Floyd-Steinberg dither.
 *
 *   Revision 1.24  2000/04/18 12:21:52  rlk
 *   Fix incorrect printing for variable drop sizes
 *
 *   Revision 1.23  2000/04/17 13:22:25  rlk
 *   Better matrix for ordered dither
 *
 *   Revision 1.22  2000/04/16 21:55:20  rlk
 *   We really do need to randomize the black transition
 *
 *   Revision 1.21  2000/04/16 21:31:32  rlk
 *   Choice of dithering algorithms
 *
 *   Revision 1.20  2000/04/16 02:52:39  rlk
 *   New dithering code
 *
 *   Revision 1.19.2.6  2000/04/16 02:37:46  rlk
 *   Final
 *
 *   Revision 1.19.2.5  2000/04/16 02:13:55  rlk
 *   More improvements
 *
 *   Revision 1.19.2.4  2000/04/14 12:46:18  rlk
 *   Other dithering options
 *
 *   Revision 1.19.2.3  2000/04/13 12:01:44  rlk
 *   Much improved
 *
 *   Revision 1.19.2.2  2000/04/12 02:27:57  rlk
 *   some improvement
 *
 *   Revision 1.19.2.1  2000/04/11 01:53:06  rlk
 *   Yet another dither hack
 *
 *   Revision 1.13  2000/03/13 13:31:26  rlk
 *   Add monochrome mode
 *
 *   Revision 1.12  2000/03/11 23:27:06  rlk
 *   Finish the dither job, and fix up the Ghostscript driver
 *
 *   Revision 1.11  2000/03/11 17:30:15  rlk
 *   Significant dither changes; addition of line art/solid color/continuous tone modes
 *
 *   Revision 1.10  2000/03/09 02:50:17  rlk
 *   Performance optimizations, documentation
 *
 *   Revision 1.9  2000/03/07 02:54:05  rlk
 *   Move CVS history logs to the end of the file
 *
 *   Revision 1.8  2000/03/02 03:09:03  rlk
 *   Performance, by replacing long long with int
 *
 *   Revision 1.7  2000/02/28 01:26:11  rlk
 *   Try to improve high resolution quality
 *
 *   Revision 1.6  2000/02/26 00:14:44  rlk
 *   Rename dither_{black,cmyk}4 to dither_{black,cmyk}_n, and add argument to specify how levels are to be encoded
 *
 *   Revision 1.5  2000/02/21 20:32:37  rlk
 *   Important dithering bug fixes:
 *
 *   1) Avoid runaway black buildup.
 *
 *   2) Some conversion functions weren't doing density
 *
 *   Revision 1.4  2000/02/18 02:30:01  rlk
 *   A few more dithering bugs
 *
 *   Revision 1.3  2000/02/16 00:59:19  rlk
 *   1) Use correct convert functions (canon, escp2, pcl, ps).
 *
 *   2) Fix gray_to_rgb increment (print-util)
 *
 *   3) Fix dither update (print-dither)
 *
 *   Revision 1.2  2000/02/07 01:35:05  rlk
 *   Try to improve variable dot stuff
 *
 *   Revision 1.1  2000/02/06 18:40:53  rlk
 *   Split out dither stuff from print-util
 *
 * End of "$Id: print-dither.c,v 1.38 2000/05/06 02:12:35 rlk Exp $".
 */
