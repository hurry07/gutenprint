/*
 * "$Id: print.h,v 1.100 2001/01/01 23:58:52 rlk Exp $"
 *
 *   Print plug-in header file for the GIMP.
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

#ifndef PRINT_HEADER
#define PRINT_HEADER

/*
 * Include necessary header files...
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/*
 * Constants...
 */

#define OUTPUT_GRAY		0	/* Grayscale output */
#define OUTPUT_COLOR		1	/* Color output */
#define OUTPUT_GRAY_COLOR	2 	/* Grayscale output using color */

#define ORIENT_AUTO		-1	/* Best orientation */
#define ORIENT_PORTRAIT		0	/* Portrait orientation */
#define ORIENT_LANDSCAPE	1	/* Landscape orientation */
#define ORIENT_UPSIDEDOWN	2	/* Reverse portrait orientation */
#define ORIENT_SEASCAPE		3	/* Reverse landscape orientation */

#define MAX_CARRIAGE_WIDTH	80 /* This really needs to go away */
				/* For now, this is wide enough for 4B ISO */

#define IMAGE_LINE_ART		0
#define IMAGE_SOLID_TONE	1
#define IMAGE_CONTINUOUS	2
#define IMAGE_MONOCHROME	3
#define NIMAGE_TYPES		4

#define ECOLOR_C 0
#define ECOLOR_M 1
#define ECOLOR_Y 2
#define ECOLOR_K 3
#define NCOLORS (4)

/* Uncomment the next line to get performance statistics:
 * look for QUANT(#) in the code. At the end of escp2-print
 * run, it will print out how long and how many time did 
 * certain pieces of code take. Of course, don't forget about
 * overhead of call to gettimeofday - it's not zero.
 * If you need more detailed performance stats, just put
 * QUANT() calls in the interesting spots in the code */
/*#define QUANTIFY*/
#ifdef QUANTIFY
#include <assert.h>
#include <sys/time.h>
#include <unistd.h>
#else
#define QUANT(n)
#endif

/*
 * Printer driver control structure.  See "print.c" for the actual list...
 */

typedef struct					/* Plug-in variables */
{
  char	output_to[256],		/* Name of file or command to print to */
	driver[64],		/* Name of printer "driver" */
	ppd_file[256];		/* PPD file */
  int	output_type;		/* Color or grayscale output */
  char	resolution[64],		/* Resolution */
	media_size[64],		/* Media size */
	media_type[64],		/* Media type */
	media_source[64],	/* Media source */
	ink_type[64],		/* Ink or cartridge */
	dither_algorithm[64];	/* Dithering algorithm */
  float	brightness;		/* Output brightness */
  float	scaling;		/* Scaling, percent of printable area */
  int	orientation,		/* Orientation - 0 = port., 1 = land.,
				   -1 = auto */
	left,			/* Offset from lower-lefthand corner, points */
	top;			/* ... */
  float gamma;                  /* Gamma */
  float contrast,		/* Output Contrast */
	cyan,			/* Output red level */
	magenta,		/* Output green level */
	yellow;			/* Output blue level */
  int	linear;			/* Linear density (mostly for testing!) */
  float	saturation;		/* Output saturation */
  float	density;		/* Maximum output density */
  int	image_type;		/* Image type (line art etc.) */
  int	unit;			/* Units for preview area 0=Inch 1=Metric */
  float app_gamma;		/* Application gamma */
  int	page_width;		/* Width of page in points */
  int	page_height;		/* Height of page in points */
  void  *lut;			/* Look-up table */
  unsigned char *cmap;		/* Color map */
} vars_t;

typedef struct		/**** Printer List ****/
{
  int	active;			/* Do we know about this printer? */
  char	name[128];		/* Name of printer */
  vars_t v;
} plist_t;

typedef enum papersize_unit
{
  PAPERSIZE_ENGLISH,
  PAPERSIZE_METRIC
} papersize_unit_t;

typedef struct
{
  char name[32];
  unsigned width;
  unsigned height;
  papersize_unit_t paper_unit;
} papersize_t;

/*
 * Abstract data type for interfacing with the image creation program
 * (in this case, the Gimp).
 */
typedef void *Image;

/* For how to create an Image wrapping a Gimp drawable, see print_gimp.h */

extern void Image_init(Image image);
extern void Image_reset(Image image);
extern void Image_transpose(Image image);
extern void Image_hflip(Image image);
extern void Image_vflip(Image image);
extern void Image_crop(Image image, int left, int top, int right, int bottom);
extern void Image_rotate_ccw(Image image);
extern void Image_rotate_cw(Image image);
extern void Image_rotate_180(Image image);
extern int  Image_bpp(Image image);
extern int  Image_width(Image image);
extern int  Image_height(Image image);
extern void Image_get_row(Image image, unsigned char *data, int row);

extern const char *Image_get_appname(Image image);
extern void Image_progress_init(Image image);
extern void Image_note_progress(Image image, double current, double total);
extern void Image_progress_conclude(Image image);


/*
 * Definition of a printer.  A printer definition contains some data
 * about the printer and a set of member functions that operate on it.
 *
 * The data members are:
 *
 * long_name is a human-readable name.  It is intended to be used by
 *   a user interface to print the name of the printer.
 *
 * driver is the short name of the printer.  This is an alternate name
 *   that is used internally.  A user interface may use this for input
 *   purposes, or a client program may use this to generate a filename.
 *   The driver name should consist of lowercase alphanumerics and hyphens
 *   only.
 *
 * model is a model number used only by the underlying driver.  It is
 *   treated as an opaque, but static, identifier.  It should not be a
 *   pointer value, but the exact interpretation of the model number
 *   is up to the driver implementation (it may be an index into an
 *   array, for example).
 *
 * printvars is the default settings for this printer.
 *
 * The member functions are:
 *
 * char **(*parameters)(const struct *printer,
 *                      char *ppd_file,
 *                      char *name,
 *                      int *count)
 *
 *   returns a list of option values of the specified parameter NAME
 *   for the specified PRINTER.  If a PPD filename is specified, the driver
 *   may use that to help generate the valid parameter list.  The number
 *   of options returned is placed in COUNT.  Both the array and the
 *   options themselves are allocated on the heap; it is the caller's
 *   responsibility to free them upon completion of use.  The driver
 *   must therefore return a copy of data.
 *
 *   In all cases, the returned option names should be appropriate for a
 *   user interface to display.
 *
 *   The list of parameters is subject to change.  The currently supported
 *   parameters are:
 *
 *     PageSize returns a list of legal page size names for the printer
 *       in question.
 *
 *     Resolution returns a list of valid resolution settings.  The
 *       resolutions are to be interpreted as opaque names; the caller
 *       must not attempt to interpret them except with the
 *       describe_resolution function described below.  There may be
 *       multiple resolution names that resolve to the same printing
 *       resolution; they may correspond to different quality settings,
 *       for example.
 *
 *     InkType returns a list of legal ink types.  The printer driver may
 *       define these as it sees fit.  If a printer offers a choice of
 *       ink cartridges, the choices would be enumerated here.
 *
 *     MediaType returns a list of legal media types.  The printer driver
 *       may define these as it sees fit.  This is normally different kinds
 *       of paper that the printer can handle.
 *
 *     InputSlot returns a list of legal input sources for the printer.
 *       This is typically things like different input trays, manual feed,
 *       roll feed, and the like.
 *
 * void (*media_size)(const struct printer *printer,
 *                    const vars_t *v,
 *                    int *width,
 *                    int *height)
 *
 *   returns the physical WIDTH and HEIGHT of the page using the settings
 *   in V.  The driver will almost always look at the media_size variable
 *   in V; it may look at other data in V to determine the physical page
 *   size.  WIDTH and HEIGHT are expressed in units of 1/72".
 *
 * void (*imageable_area)(const struct printer *printer,
 *                        const vars_t *v,
 *                        int *left,
 *                        int *right,
 *                        int *bottom,
 *                        int *top)
 *
 *   returns the width of the LEFT, RIGHT, BOTTOM, and TOP border of the
 *   page for the given printer and variable settings.  The caller can
 *   use this, in combination with the media_size member, to determine
 *   the printable region of the page, and if needed, exactly where to
 *   place the image to achieve a given physical placement (e. g.
 *   centering) on the page.  All returned values are in units of
 *   1/72".
 *
 * void (*limit)(const struct printer *printer,
 *               const vars_t *v,
 *               int *width,
 *               int *height)
 *
 *   returns the maximum page size the printer can handle, in units of
 *   1/72".
 *
 * void (*print)(const struct printer *printer,
 *               FILE *prn,
 *               Image image,
 *               const vars_t *v)
 *
 *   prints a page.  The variable settings provided in V are used to control
 *   the printing; PRN is a file pointer that the raw printer output
 *   is to be written to, and IMAGE is an object that sources the input
 *   data to the driver (the contents of which are opaque to the low level
 *   driver and are interpreted by the high level program).
 *
 * const char *(*default_resolution)(const struct printer *printer)
 *
 *   returns the name of the default resolution for the printer.  The
 *   caller must not attempt to free the returned value.
 *
 * void (*describe_resolution)(const struct printer *printer,
 *                             const char *resolution,
 *                             int *x,
 *                             int *y)
 *
 *   returns the horizontal (X) and vertical (Y) resolution of the chosen
 *   RESOLUTION name.  The high level program may choose to use this to
 *   rasterize at an appropriate resolution.
 *   
 */

typedef struct printer
{
  char	*long_name,			/* Long name for UI */
	*driver;			/* Short name for printrc file */
  int	model;				/* Model number */
  char	**(*parameters)(const struct printer *printer, char *ppd_file,
                        char *name, int *count);
  void	(*media_size)(const struct printer *printer, const vars_t *v,
		      int *width, int *height);
  void	(*imageable_area)(const struct printer *printer, const vars_t *v,
                          int *left, int *right, int *bottom, int *top);
  void	(*limit)(const struct printer *printer, const vars_t *v,
		 int *width, int *height);
  void	(*print)(const struct printer *printer, FILE *prn,
		 Image image, const vars_t *v);
  const char *(*default_resolution)(const struct printer *printer);
  void  (*describe_resolution)(const struct printer *printer,
			       const char *resolution, int *x, int *y);
  vars_t printvars;
} printer_t;

/*
 * hue_map is an array of 49 doubles representing the mapping of hue
 * from (0..6) to (0..6) in increments of .125.  The hue_map is in CMY space,
 * so hue=0 is cyan.
 */
typedef void (*convert_t)(unsigned char *in, unsigned short *out, int width,
			  int bpp, unsigned char *cmap, const vars_t *vars,
			  const double *hue_map, const double *lum_map,
			  const double *sat_map);

typedef struct
{
  double value;
  unsigned bit_pattern;
  int is_dark;
  unsigned dot_size;
} simple_dither_range_t;

typedef struct
{
  double value;
  double lower;
  double upper;
  unsigned bit_pattern;
  int is_dark;
  unsigned dot_size;
} dither_range_t;

typedef struct
{
   double value_l;
   double value_h;
   unsigned bits_l;
   unsigned bits_h;
   int isdark_l;
   int isdark_h;
} full_dither_range_t;

/*
 * Prototypes...
 */

extern void *	init_dither(int in_width, int out_width, int horizontal_aspect,
			    int vertical_aspect, vars_t *vars);
extern void	dither_set_transition(void *vd, double);
extern void	dither_set_density(void *vd, double);
extern void	dither_set_black_density(void *vd, double);
extern void 	dither_set_black_lower(void *vd, double);
extern void 	dither_set_black_upper(void *vd, double);
extern void	dither_set_black_levels(void *vd, double, double, double);
extern void 	dither_set_randomizers(void *vd, double, double, double, double);
extern void 	dither_set_ink_darkness(void *vd, double, double, double);
extern void 	dither_set_light_inks(void *vd, double, double, double, double);
extern void	dither_set_ranges(void *vd, int color, int nlevels,
				  const simple_dither_range_t *ranges,
				  double density);
extern void	dither_set_ranges_full(void *vd, int color, int nlevels,
				       const full_dither_range_t *ranges,
				       double density);
extern void	dither_set_ranges_simple(void *vd, int color, int nlevels,
					 const double *levels, double density);
extern void	dither_set_ranges_complete(void *vd, int color, int nlevels,
					   const dither_range_t *ranges);
extern void	dither_set_ink_spread(void *vd, int spread);
extern void	dither_set_max_ink(void *vd, int, double);
extern void	dither_set_x_oversample(void *vd, int os);
extern void	dither_set_y_oversample(void *vd, int os);
extern void	dither_set_adaptive_divisor(void *vd, unsigned divisor);


extern void	free_dither(void *);


extern void	dither_monochrome(const unsigned short *, int, void *,
				 unsigned char *, int duplicate_line);

extern void	dither_black(const unsigned short *, int, void *,
			     unsigned char *, int duplicate_line);

extern void	dither_cmyk(const unsigned short *, int, void *,
			    unsigned char *,
			    unsigned char *, unsigned char *,
			    unsigned char *, unsigned char *,
			    unsigned char *, unsigned char *,
			    int duplicate_line);


extern void *	initialize_weave_params(int S, int J, int O,
		                        int firstrow, int lastrow,
		                        int pageheight, int strategy);
extern void	calculate_row_parameters(void *w, int row, int subpass,
		                         int *pass, int *jet, int *startrow,
					 int *phantomrows, int *jetsused);
extern void	destroy_weave_params(void *vw);

extern void	stp_fold(const unsigned char *line, int single_height,
			 unsigned char *outbuf);

extern void	stp_split_2(int height, int bits, const unsigned char *in,
			    unsigned char *outhi, unsigned char *outlo);

extern void	stp_split_4(int height, int bits, const unsigned char *in,
			    unsigned char *out0, unsigned char *out1,
			    unsigned char *out2, unsigned char *out3);

extern void	stp_unpack_2(int height, int bits, const unsigned char *in,
			     unsigned char *outlo, unsigned char *outhi);

extern void	stp_unpack_4(int height, int bits, const unsigned char *in,
			     unsigned char *out0, unsigned char *out1,
			     unsigned char *out2, unsigned char *out3);

extern void	stp_unpack_8(int height, int bits, const unsigned char *in,
			     unsigned char *out0, unsigned char *out1,
			     unsigned char *out2, unsigned char *out3,
			     unsigned char *out4, unsigned char *out5,
			     unsigned char *out6, unsigned char *out7);

extern int	stp_pack(const unsigned char *line, int height,
			 unsigned char *comp_buf, unsigned char **comp_ptr);

extern void	merge_printvars(vars_t *user, const vars_t *print);
extern void	free_lut(vars_t *v);
extern void	compute_lut(size_t steps, vars_t *v);


extern void	default_media_size(const printer_t *printer, const vars_t *v,
				   int *width, int *height);


extern char	**lexmark_parameters(const printer_t *printer, char *ppd_file,
		                   char *name, int *count);
extern void	lexmark_imageable_area(const printer_t *printer, const vars_t *v,
				     int *left, int *right,
				     int *bottom, int *top);
extern void	lexmark_limit(const printer_t *printer, const vars_t *v,
			    int *width, int *height);
extern void	lexmark_print(const printer_t *printer, FILE *prn,
			    Image image, const vars_t *v);
extern const char *lexmark_default_resolution(const printer_t *printer);
extern void     lexmark_describe_resolution(const struct printer *printer,
					    const char *resolution,
					    int *x, int *y);


extern char	**escp2_parameters(const printer_t *printer, char *ppd_file,
				   char *name, int *count);
extern void	escp2_imageable_area(const printer_t *printer, const vars_t *v,
				     int *left, int *right,
				     int *bottom, int *top);
extern void	escp2_limit(const printer_t *printer, const vars_t *v,
			    int *width, int *height);
extern void	escp2_print(const printer_t *printer, FILE *prn,
			    Image image, const vars_t *v);
extern const char *escp2_default_resolution(const printer_t *printer);
extern void     escp2_describe_resolution(const struct printer *printer,
					  const char *resolution,
					  int *x, int *y);


extern char	**canon_parameters(const printer_t *printer, char *ppd_file,
		                   char *name, int *count);
extern void	canon_imageable_area(const printer_t *printer, const vars_t *v,
				     int *left, int *right,
				     int *bottom, int *top);
extern void	canon_limit(const printer_t *printer, const vars_t *v,
			    int *width, int *height);
extern void	canon_print(const printer_t *printer, FILE *prn,
			    Image image, const vars_t *v);
extern const char *canon_default_resolution(const printer_t *printer);
extern void     canon_describe_resolution(const struct printer *printer,
					  const char *resolution,
					  int *x, int *y);


extern char	**pcl_parameters(const printer_t *printer, char *ppd_file,
		                 char *name, int *count);
extern void	pcl_imageable_area(const printer_t *printer, const vars_t *v,
		                   int *left, int *right,
				   int *bottom, int *top);
extern void	pcl_limit(const printer_t *printer, const vars_t *v,
			  int *width, int *height);
extern void	pcl_print(const printer_t *printer, FILE *prn,
			  Image image, const vars_t *v);
extern const char *pcl_default_resolution(const printer_t *printer);
extern void     pcl_describe_resolution(const struct printer *printer,
					const char *resolution,
					int *x, int *y);


extern char	**ps_parameters(const printer_t *printer, char *ppd_file,
		                char *name, int *count);
extern void	ps_media_size(const printer_t *printer, const vars_t *v,
			      int *width, int *height);
extern void	ps_imageable_area(const printer_t *printer, const vars_t *v,
				  int *left, int *right,
		                  int *bottom, int *top);
extern void	ps_limit(const printer_t *printer, const vars_t *v,
			 int *width, int *height);
extern void	ps_print(const printer_t *printer, FILE *prn,
			 Image image, const vars_t *v);
extern const char *ps_default_resolution(const printer_t *printer);
extern void     ps_describe_resolution(const struct printer *printer,
				       const char *resolution,
				       int *x, int *y);

extern const char *default_dither_algorithm(void);

extern int	      		known_papersizes(void);
extern const papersize_t	*get_papersizes(void);
extern const papersize_t	*get_papersize_by_name(const char *);
extern const papersize_t 	*get_papersize_by_size(int l, int w);

extern int			known_printers(void);
extern const printer_t		*get_printers(void);
extern const printer_t		*get_printer_by_index(int);
extern const printer_t		*get_printer_by_long_name(const char *);
extern const printer_t		*get_printer_by_driver(const char *);
extern int			get_printer_index_by_driver(const char *);

extern int			num_dither_algos;
extern char			*dither_algo_names[];
extern convert_t choose_colorfunc(int, int, const unsigned char *, int *,
				  const vars_t *);
extern void
compute_page_parameters(int page_right, int page_left, int page_top,
			int page_bottom, double scaling, int image_width,
			int image_height, Image image, int *orientation,
			int *page_width, int *page_height, int *out_width,
			int *out_height, int *left, int *top);

extern int
verify_printer_params(const printer_t *, const vars_t *);
extern const vars_t *print_default_settings(void);
extern const vars_t *print_maximum_settings(void);
extern const vars_t *print_minimum_settings(void);

#ifdef QUANTIFY
/* Used for performance analysis - to be called before and after
 * the interval to be quantified */
#define NUM_QUANTIFY_BUCKETS 1024
extern unsigned quantify_counts[NUM_QUANTIFY_BUCKETS];
extern struct timeval quantify_buckets[NUM_QUANTIFY_BUCKETS];
extern int quantify_high_index;
extern int quantify_first_time;
extern struct timeval quantify_cur_time;
extern struct timeval quantify_prev_time;

#define QUANT(number) \
{\
    gettimeofday(&quantify_cur_time, NULL);\
    assert(number < NUM_QUANTIFY_BUCKETS);\
    quantify_counts[number]++;\
\
    if (quantify_first_time) {\
        quantify_first_time = 0;\
    } else {\
        if (number > quantify_high_index) quantify_high_index = number;\
        if (quantify_prev_time.tv_usec > quantify_cur_time.tv_usec) {\
           quantify_buckets[number].tv_usec += ((quantify_cur_time.tv_usec + 1000000) - quantify_prev_time.tv_usec);\
           quantify_buckets[number].tv_sec += (quantify_cur_time.tv_sec - quantify_prev_time.tv_sec - 1);\
        } else {\
           quantify_buckets[number].tv_sec += quantify_cur_time.tv_sec - quantify_prev_time.tv_sec;\
           quantify_buckets[number].tv_usec += quantify_cur_time.tv_usec - quantify_prev_time.tv_usec;\
        }\
        if (quantify_buckets[number].tv_usec >= 1000000)\
        {\
           quantify_buckets[number].tv_usec -= 1000000;\
           quantify_buckets[number].tv_sec++;\
        }\
    }\
\
    gettimeofday(&quantify_prev_time, NULL);\
}

extern void  print_timers(void );
#endif

#endif /* PRINT_HEADER */
/*
 * End of "$Id: print.h,v 1.100 2001/01/01 23:58:52 rlk Exp $".
 */
