/* 
 
  Stylus Photo Printer driver for ghostscript

  -uses escp2-driver from gimp print plugin V3.0.3

  -gs frontend derived from gdevbmp and gdevcdj


  written in January 2000 by 
  Henryk Richter <buggs@comlab.uni-rostock.de>
  for ghostscript 5.x/6.x


  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
  for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*/
/*$Id: gdevstp.c,v 1.4 2000/02/25 02:15:04 rlk Exp $ */
/* epson stylus photo  output driver */
#include "gdevprn.h"
#include "gdevpccm.h"
#include "gdevstp.h"
#include "gsparam.h"

#include "gdevstp-print.h"

/* internal debugging output ? */
/*#define DRV_DEBUG*/
#undef DRV_DEBUG

/* ------ The device descriptors ------ */

private dev_proc_print_page(stp_print_page);

private dev_proc_get_params(stp_get_params);
private dev_proc_put_params(stp_put_params);

private int stp_put_param_int(P6(gs_param_list *, gs_param_name, int *, int, int, int));
private int stp_put_param_float(P6(gs_param_list *, gs_param_name, float *, float, float, int));
private dev_proc_open_device(stp_open);

/* 24-bit color. ghostscript driver */
private const gx_device_procs stpm_procs =
prn_color_params_procs(
                 stp_open, /*gdev_prn_open,*/ /* open file, delegated to gs */
                 gdev_prn_output_page,     /* output page, delegated to gs */
                 gdev_prn_close,           /* close file, delegated to gs */
                 stp_map_16m_rgb_color, /* map color (own) */
                 stp_map_16m_color_rgb, /* map color (own) */
                 stp_get_params,        /* get params (own) */
                 stp_put_params         /* put params (own) */
                      );
#if 0
prn_color_procs(gdev_prn_open, gdev_prn_output_page, gdev_prn_close,
		stp_map_16m_rgb_color, stp_map_16m_color_rgb);
#endif

gx_device_printer gs_stp_device =
prn_device(stpm_procs, "stp",
	   DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	   X_DPI, Y_DPI,
	   0, 0, 0, 0,		/* margins */
	   24, stp_print_page);


/* grab resolution list from escp2 driver */
#ifndef res_t
typedef struct {
  const char name[65];
  int hres;
  int vres;
  int softweave;
  int horizontal_passes;
  int vertical_passes;
} res_t;
#endif

/* private data structure */
typedef struct {
  int red;          /* 100 */
  int green;        /* ... */
  int blue;         /* ... */
  int brightness;   /* ... */
  int contrast;     /* ... */
  int linear;       /* 0   */
  float gamma;      /* 1   */
  int resnr;        /* 0 == 360 dpi */
  int color;        /* 1 == color, 0 == gray */
  float saturation; /* 1   */
  float density;    /* 0.8 */
  int model;        /* see gdevstp-print.h for model list
                       default: 6 == stylus photo 
                    */
  char media[100];  /* "Letter", "Legal", "A4", "A3"      */
  int  top;         /* top margin size in 1/72 inches     */
  int  bottom;      /* bottom margin size in 1/72 inches  */
  int  topoffset;   /* top offset in pixels               */
} privdata_t;

/* in gdevstp-escp2.c */
extern res_t escp2_reslist[];


/* global variables, RO for subfunctions */
private uint stp_raster;
private byte *stp_row;
private gx_device_printer *stp_pdev;
static privdata_t stp_data = { 100, /* r          */
                                  100, /* g          */
                                  100, /* b          */
                                  100, /* bright     */
                                  100, /* cont       */
                                  0,   /* lin        */
                                  1,   /* gamma      */
                                  0,   /* resnr      */
                                  1,   /* color      */
                                  1,   /* saturation */
                                  0.8, /* density    */
                                  6,   /* model      */
                                  "A4" /* media      */
                                 };

/* ------ Private definitions ------ */

/***********************************************************************
* ghostscript driver function calls                                    *
***********************************************************************/

/*----------- Write out a in escp2 format. ---------------*/
private int stp_print_page(gx_device_printer * pdev, FILE * file)
{
    int code;									/* return code */
    int model;
    vars_t stp_vars;

    code = 0;
    stp_pdev = pdev;
    stp_raster = gdev_prn_raster(pdev);
    stp_row = gs_alloc_bytes(pdev->memory, stp_raster, "stp file buffer");

#ifdef DRV_DEBUG
    fprintf(stderr,"Image: %d x %d pixels, %f x %f dpi\n",
            pdev->width,
            pdev->height,
            pdev->x_pixels_per_inch,
            pdev->y_pixels_per_inch
           );

    fprintf(stderr,"Settings: r: %d  g: %d  b: %d\n",
            stp_data.red,
            stp_data.green,
            stp_data.blue
           );

    fprintf(stderr,"Settings: model: %d  bright: %d  contrast: %d\n",
            stp_data.model,
            stp_data.brightness,
            stp_data.contrast
           );

    fprintf(stderr,"Settings: Gamma: %f  Saturation: %f  Density: %f\n",
            stp_data.gamma,
            stp_data.saturation,
            stp_data.density
           );
#endif

    if (stp_row == 0)		/* can't allocate row buffer */
	 return_error(gs_error_VMerror);

    /* Inititalize printer */
    /* int height = pdev->height;*/
    /* int width = pdev->width; */
    /* pdev->x_pixels_per_inch */
    /* pdev->y_pixels_per_inch */
    /* int depth = pdev->color_info.depth; */

#ifdef DRV_DEBUG
    fprintf(stderr,"1 step done!");
#endif

    stp_vars.brightness = stp_data.brightness; /*100;*/
    stp_vars.gamma      = stp_data.gamma;      /*1*/
    stp_vars.saturation = stp_data.saturation; /*1*/
    stp_vars.density    = 0.646*stp_data.density;    /*0.8;*/
    stp_vars.contrast   = stp_data.contrast;   /*100;*/
    stp_vars.red        = stp_data.red;        /*100;*/
    stp_vars.green      = stp_data.green;      /*100;*/
    stp_vars.blue       = stp_data.blue;       /*100;*/
    stp_vars.linear     = 0;
 
    strcpy(stp_vars.ppd_file,"");           /* no ppd file by now */

    strcpy(stp_vars.resolution,escp2_reslist[stp_data.resnr].name); /* 360 dpi default */

    stp_vars.top  = 0;                     /* */
    stp_vars.left = 0;

    stp_vars.scaling = -pdev->x_pixels_per_inch; /* resolution of image to print */

    if(stp_data.color == 1)
     stp_vars.output_type = OUTPUT_COLOR;
    else
     stp_vars.output_type = OUTPUT_GRAY;

    stp_vars.orientation = ORIENT_PORTRAIT; /* ORIENT_AUTO 
                                              ORIENT_LANDSCAPE or 
                                              ORIENT_PORTRAIT */
    strcpy(stp_vars.media_size,stp_data.media); /* "A4", "Letter", "Legal", "A3", ... */
    
    /* compute lookup table: lut_t*,float dest_gamma,float app_gamma,vars_t* */
    compute_lut(0.585 , 1 , &stp_vars);

#ifdef DRV_DEBUG
    fprintf(stderr,"lut done!");
#endif
 
    model = stp_data.model;                 /* 6 = Stylus Photo  */

#ifdef DRV_DEBUG
    fprintf(stderr,"prefs done, now skipping the top margin lines in input\n");
#endif

    stp_data.topoffset = 0;
#if 0
    /* correct top offset (row number) */
    {
     float grmpf;

     grmpf = (float)stp_data.top * (float)stp_pdev->x_pixels_per_inch / 72.;

     stp_data.topoffset = (int)grmpf;
#ifdef DRV_DEBUG
    fprintf(stderr,"top offset %d pixels\n",stp_data.topoffset);
#endif
    }
#endif

    escp2_print(model,		/* I - Model */
                1,		/* I - Number of copies */
                file,		/* I - File to print to */
                NULL,		/* I - Image to print (dummy) */
                NULL,		/* I - Colormap (for indexed images) */
                &stp_vars);	/* vars_t * */

    gs_free_object(pdev->memory, stp_row, "stp row buffer");
    stp_row = NULL;

    return code;
}

/* 24-bit color mappers (taken from gdevmem2.c). */

/* Map a r-g-b color to a color index. */
gx_color_index
stp_map_16m_rgb_color(gx_device * dev, gx_color_value r, gx_color_value g,
		  gx_color_value b)
{
    return gx_color_value_to_byte(b) +
	((uint) gx_color_value_to_byte(g) << 8) +
	((ulong) gx_color_value_to_byte(r) << 16);
}

/* Map a color index to a r-g-b color. */
int
stp_map_16m_color_rgb(gx_device * dev, gx_color_index color,
		  gx_color_value prgb[3])
{
    prgb[2] = gx_color_value_from_byte(color & 0xff);
    prgb[1] = gx_color_value_from_byte((color >> 8) & 0xff);
    prgb[0] = gx_color_value_from_byte(color >> 16);
    return 0;
}


/*
 * Get parameters.  In addition to the standard and printer 
 * parameters, we supply a lot of options to play around with
 * for maximum quality out of the photo printer
*/
/* Yeah, I could have used a list for the options but... */
private int stp_get_params(gx_device *pdev, gs_param_list *plist)
{
	int code = gdev_prn_get_params(pdev, plist);
    gs_param_string pmedia;

    param_string_from_string(pmedia, stp_data.media);

	if ( code < 0 ||
	    (code = param_write_int(plist, "Red", &stp_data.red)) < 0 ||
	    (code = param_write_int(plist, "Green", &stp_data.green)) < 0 ||
	    (code = param_write_int(plist, "Blue", &stp_data.blue)) < 0 ||
	    (code = param_write_int(plist, "Brightness", &stp_data.brightness)) < 0 ||
	    (code = param_write_int(plist, "Contrast", &stp_data.contrast)) < 0 ||
	    (code = param_write_int(plist, "Color", &stp_data.color)) < 0 ||
	    (code = param_write_int(plist, "Model", &stp_data.model)) < 0 ||
	    (code = param_write_int(plist, "Quality", &stp_data.resnr)) < 0 ||
	    (code = param_write_string(plist, "PAPERSIZE", &pmedia)) < 0 ||
	    (code = param_write_float(plist, "Gamma", &stp_data.gamma)) < 0 ||
	    (code = param_write_float(plist, "Saturation", &stp_data.saturation)) < 0 ||
	    (code = param_write_float(plist, "Density", &stp_data.density)) < 0
	   )
 	   return code;

	return 0;
}

/* Put parameters. */
/* Yeah, I could have used a list for the options but... */
private int stp_put_params(gx_device *pdev, gs_param_list *plist)
{
    gs_param_string pmedia;
	int red    = stp_data.red;
    int green  = stp_data.green;
    int blue   = stp_data.blue;
    int bright = stp_data.brightness;
    int cont   = stp_data.contrast;
    int model  = stp_data.model;
    int color  = stp_data.color;
    int qual   = stp_data.resnr;
	float gamma = stp_data.gamma;
	float sat = stp_data.saturation;
	float den = stp_data.density;
	int code   = 0;

    param_string_from_string(pmedia, stp_data.media);

	code = stp_put_param_int(plist, "Red", &red, 0, 200, code);
	code = stp_put_param_int(plist, "Green", &green, 0, 200, code);
	code = stp_put_param_int(plist, "Blue", &blue, 0, 200, code);
	code = stp_put_param_int(plist, "Brightness", &bright, 0, 400, code);
	code = stp_put_param_int(plist, "Contrast", &cont, 25, 400, code);
	code = stp_put_param_int(plist, "Color", &color, 0, 1, code);
	code = stp_put_param_int(plist, "Model", &model, 0, 15, code);
	code = stp_put_param_int(plist, "Quality", &qual, 0, 7, code);
	code = stp_put_param_float(plist, "Gamma", &gamma, 0.1, 3., code);
	code = stp_put_param_float(plist, "Saturation", &sat, 0.1, 9., code);
	code = stp_put_param_float(plist, "Density", &den, 0.1, 2., code);

    if( param_read_string(plist, "PAPERSIZE", &pmedia) == 0)
	{
/*
	 fprintf(stderr,"Media defined: %s\n",pmedia.data);
*/
	}

	if ( code < 0 )
	  return code;

	stp_data.red = red;
    stp_data.green = green;
    stp_data.blue = blue;
    stp_data.brightness = bright;
    stp_data.contrast = cont;
    stp_data.model = model;
    stp_data.color = color;
    stp_data.resnr = qual;
    strcpy(stp_data.media,pmedia.data);
    stp_data.gamma = gamma;
    stp_data.saturation = sat;
    stp_data.density = den;

    {
	 byte a;
	 
     a = *stp_data.media;
     a &= 255-32;      /* quick`n`dirty lcase->ucase for first letter ;-) */
     *stp_data.media = a;
#if 0
	 fprintf(stderr,"Media defined: %s\n",stp_data.media);
#endif
    }

    code = gdev_prn_put_params(pdev, plist);
	return code;
}

private int stp_put_param_int(gs_param_list *plist,
                                 gs_param_name pname, 
                                 int *pvalue,  
                                 int minval, 
                                 int maxval, 
                                 int ecode)
{
	int code, value;
	switch ( code = param_read_int(plist, pname, &value) )
	{
	default:
		return code;
	case 1:
		return ecode;
	case 0:
		if ( value < minval || value > maxval )
		{
         param_signal_error(plist, pname, gs_error_rangecheck);
         ecode = -100;
        }
		else
         *pvalue = value;

		return (ecode < 0 ? ecode : 1);
	}
}	

private int stp_put_param_float(gs_param_list *plist, 
                                   gs_param_name pname, 
                                   float *pvalue,
                                   float minval, 
                                   float maxval, 
                                   int ecode)
{	
    int code;
    float value;
    
	switch ( code = param_read_float(plist, pname, &value) )
	{
	default:
		return code;
	case 1:
		return ecode;
	case 0:
		if ( value < minval || value > maxval )
		{
         param_signal_error(plist, pname, gs_error_rangecheck);
         ecode = -100;
        }
		else
         *pvalue = value;

		return (ecode < 0 ? ecode : 1);
	}
}	

private int
stp_open(gx_device *pdev)
{
  /* Change the margins if necessary. */
  float st[4];
  int left,right,bottom,top,width,length;
  char none[5];
  
  strcpy(none,"");

  default_media_size(stp_data.model,
                     none,
                     stp_data.media,
                     &width,
                     &length);

  escp2_imageable_area(stp_data.model,	/* I - Printer model */
                       none,		/* I - PPD file (not used) */
                       stp_data.media,	/* I - Media size */
                       &left,		/* O - Left position in points */
                       &right,		/* O - Right position in points */
                       &bottom,		/* O - Bottom position in points */
                       &top);		/* O - Top position in points */

#if 0
  /* This is now fixed in the driver.  The safest fix is to push the */
  /* top in corresponding to the number of rows and spacing */
  /*!!!! fix for "+32 hack" in escp2-driver when setting printer
    Top/bottom margins */
  if(escp2_reslist[stp_data.resnr].softweave != 0)
  {
   top -= (32-6);
  }
#endif

  st[1] = (float)bottom / 72;        /* bottom margin */
  st[3] = (float)(length-top) / 72;  /* top margin    */
  st[0] = (float)left / 72;          /* left margin   */
  st[2] = (float)(width-right) / 72; /* right margin  */

  stp_data.top    = length-top;
  stp_data.bottom = bottom;
#if 0
    /*
     * The margins must be set so that the resulting page length will be 
     * expressed exactly as a multiple of tenthes of inches.
     *
     */

    /**/ {
	float *bjcm = (float *) st;

	byte pdimen = (byte)
	    (pdev->height / pdev->y_pixels_per_inch * 10.
	     - bjcm[3] * 10. - bjcm[1] * 10. + .5) + 1;
	do {
	    --pdimen;
	    bjcm[1] = pdev->height / pdev->y_pixels_per_inch
	        - bjcm[3] - (float) pdimen / 10.;
	} while (bjcm[1] < (9.54 / 25.4) );

    bjcm[1] += .1;
    }
#endif

#ifdef DRV_DEBUG
  fprintf(stderr,"margins: %f %f %f %f\n",st[0],st[1],st[2],st[3]);
#endif

  gx_device_set_margins(pdev, st, true);
  return gdev_prn_open(pdev);
}

/***********************************************************************
* escp2 driver function callback routines                              *
***********************************************************************/

/* get one row of the image */
void Image_get_row(Image image, unsigned char *data, int row)
{
#ifdef DRV_DEBUG
 gdev_prn_copy_scan_lines(stp_pdev, stp_data.topoffset+row , data, stp_raster);
#else
 int a;
 a = gdev_prn_copy_scan_lines(stp_pdev, stp_data.topoffset+row , data, stp_raster);
#if 0 /* exzessive output */
 fprintf(stderr,"read scanline, %d lines copied\n",a);
 fprintf(stderr,"%x %x %x %x %x %x %x %x\n",*data,
  *(data+1),
  *(data+2),
  *(data+3),
  *(data+4),
  *(data+5),
  *(data+6),
  *(data+7));
#endif
#endif

}

/* return bpp of picture (24 here) */
int Image_bpp(Image image)
{
  return 3;
}

/* return width of picture */
int Image_width(Image image)
{
  return stp_pdev->width;
}

/*
  return height of picture and
  subtract margins from image size so that the
  driver only reads the correct number of lines from the
  input

*/
int Image_height(Image image)
{
  float tmp,tmp2;
                      
  tmp =   stp_data.top + stp_data.bottom; /* top margin + bottom margin */

                                           /* calculate height in 1/72 inches */
  tmp2 = (float)stp_pdev->height / (float)stp_pdev->x_pixels_per_inch * 72.;

  tmp2 -= tmp;                                 /* subtract margins from sizes */
                                               
                                               /* calculate new image height */
  tmp2 *= (float)stp_pdev->x_pixels_per_inch / 72.;

#ifdef DRV_DEBUG
  fprintf(stderr,"corrected page length %f\n",tmp2);
#endif  

  return (int)tmp2;
}

/* get an image column */
void Image_get_col(Image img,unsigned char *data,int column)
{
 /* dummy function, Landscape printing unsupported atm */
}

void Image_init(Image image)
{
 /* dummy function */
}

void Image_progress_init(Image image)
{
 /* dummy function */
}

/* progress display */
void Image_note_progress(Image image, double current, double total)
{
 /* dummy function */
}
