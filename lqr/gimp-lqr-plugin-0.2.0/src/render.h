/* GIMP LiquidRescaling Plug-in
 * Copyright (C) 2007 Carlo Baldassi (the "Author") <carlobaldassi@yahoo.it>.
 * (implementation based on the GIMP Plug-in Template by Michael Natterer)
 * All Rights Reserved.
 *
 * This plugin implements the algorithm described in the paper
 * "Seam Carving for Content-Aware Image Resizing"
 * by Shai Avidan and Ariel Shamir
 * which can be found at http://www.faculty.idc.ac.il/arik/imret.pdf
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 dated June, 1991.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef __RENDER_H__
#define __RENDER_H__

#include <stdlib.h>
#include <assert.h>
#include "plugin-intl.h"

#define _LQR_DATA_MAX_BPP (4)


/*  Render function  */

void render (gint32 image_ID,
	     GimpDrawable * drawable,
	     PlugInVals * vals,
	     PlugInImageVals * image_vals,
	     PlugInDrawableVals * drawable_vals);


/* Liquid rescaling */

typedef double (*p_to_f) (double, double);
typedef struct _LqrData LqrData;
typedef struct _LqrCursor LqrCursor;
typedef struct _LqrRaster LqrRaster;

/**** CLASSES DECLARATIONS ****/
struct _LqrData;		/* base LqrData                       */
struct _LqrCursor;		/* a "smart" index to read the raster */
struct _LqrRaster;		/* the multisize image raster         */

/**** LQR_DATA CLASS DEFINITION ****/
/* This is the basic class representing a point of the multisize image.
 * It holds RGB values, visibility levels and other auxiliary
 * quantities */
struct _LqrData
{
  double rgb[_LQR_DATA_MAX_BPP];	/* color map
					 * (doubles between 0.0 and 1.0)
					 */

  double e;			/* energy */
  double b;			/* energy bias */
  double m;			/* auxiliary value for seam construction */
  LqrData *least;		/* pointer ... */
  int least_x;
  int vs;			/* vertical seam level (determines visibiliy)
				 *   vs = 0       -> uninitialized (visible)
				 *   vs < level   -> invisible
				 *   vs >= level  -> visible
				 */
};

/* LQR_DATA CLASS FUNCTIONS */

/* constructor */
LqrData *init_lqr_data (double r, double g, double b);

/* reset */
void lqr_data_reset (LqrData * d, int bpp);

/* read average color value */
double lqr_data_read (LqrData * d, int bpp);



/**** LQR_CURSOR CLASS DEFINITION ****/
/* The lqr_cursors can scan a multisize image according to its
 * current visibility level, skipping invisible points */
struct _LqrCursor
{
  int initialized;		/* initialization flag */
  int x;			/* x coordinate of current data */
  int y;			/* y coordinate of current data */
  LqrRaster *o;			/* pointer to owner raster */
  LqrData *map;			/* pointer to owner's map */
  LqrData *now;			/* pointer to current data */
};

/* LQR_CURSOR CLASS FUNCTIONS */

/* constructor */
LqrCursor *lqr_cursor_create (LqrRaster * owner, LqrData * m, int num);

void lqr_cursor_destroy (LqrCursor * c);

/* functions for reading data */
double lqr_cursor_readc (LqrCursor * c, int col);
double lqr_cursor_read (LqrCursor * c);

/* functions for moving around */
void lqr_cursor_reset (LqrCursor * c);
void lqr_cursor_seek (LqrCursor * c, LqrData * n, int x1, int y1);
void lqr_cursor_seekxy (LqrCursor * c, int x1, int y1);
void lqr_cursor_next (LqrCursor * c);
void lqr_cursor_prev (LqrCursor * c);

/* functions for exploring neighborhoods */
LqrData *lqr_cursor_right (LqrCursor * c);
LqrData *lqr_cursor_left (LqrCursor * c);
LqrData *lqr_cursor_up (LqrCursor * c);
LqrData *lqr_cursor_down (LqrCursor * c);



/**** LQR_RASTER CLASS DEFINITION ****/
/* This is the representation of the multisize image
 * The image is stored internally as a one-dimentional
 * array of LqrData points, called map.
 * The points are ordered by rows. */
struct _LqrRaster
{
  int w_start, h_start;		/* original width & height */
  int w, h;			/* current width & height */
  int w0, h0;			/* map array width & height */
  int x_off, y_off;		/* layer offsets in the GIMP coordinate system */

  int level;			/* (in)visibility level (1 = full visibility) */
  int max_level;		/* max level computed so far
				 * it is not level <= max_level
				 * but rather level <= 2 * max_level - 1
				 * since levels are shifted upon inflation
				 */

  int bpp;			/* number of bpp of the image */

  int transposed;		/* flag to set transposed state */

  gboolean update_e;		/* flag to determine wether energy is updated */

  LqrData *map;			/* array of points */

  LqrCursor *c;			/* cursor to be used as current point */
  LqrCursor *c_up, *c_down;	/* auxiliary cursors, up and down form current */

  LqrCursor *vpath;		/* array of cursors representing a vertical seam */

  p_to_f gf;			/* pointer to a gradient function */

};


/* LQR_RASTER CLASS FUNCTIONS */

/* build maps */
void lqr_raster_build_maps (LqrRaster * r, int depth);	/* build all */
void lqr_raster_build_emap (LqrRaster * r);	/* energy */
void lqr_raster_build_mmap (LqrRaster * r);	/* minpath */
void lqr_raster_build_vsmap (LqrRaster * r, int depth);	/* visibility */

/* internal functions for maps computation */
void lqr_raster_compute_e (LqrRaster * r);	/* compute energy of point at c */
void lqr_raster_update_emap (LqrRaster * r);	/* update energy map after seam removal */
void lqr_raster_update_mmap (LqrRaster * r);	/* minpath */
void lqr_raster_build_vpath (LqrRaster * r);	/* compute seam path */
void lqr_raster_update_vsmap (LqrRaster * r, int l);	/* update visibility map after seam removal */
void lqr_raster_finish_vsmap (LqrRaster * r);	/* complete visibility map (last seam) */
void lqr_raster_inflate (LqrRaster * r, int l);	/* adds enlargment info to map */

/* image manipulations */
void lqr_raster_set_width (LqrRaster * r, int w1);
void lqr_raster_transpose (LqrRaster * r);

/* constructor & destructor */
LqrRaster *lqr_raster_create (GimpDrawable * drawable, gint32 pres_layer_ID, gint pres_coeff,
		              gint32 disc_layer_ID, gint disc_coeff, LqrGradFunc gf_int, gboolean update_e);
void lqr_raster_destroy (LqrRaster * r);

/* set gradient function */
void lqr_raster_set_gf (LqrRaster * r, LqrGradFunc gf_ind);

/* image manipulations */
void lqr_raster_resize (LqrRaster * r, int w1, int h1);	/* liquid resize */
void lqr_raster_flatten (LqrRaster * r);	/* flatten the multisize image */

/* output */

void lqr_raster_write_energy (LqrRaster * r /*, pngwriter& output */ );	/* output the energy */
void lqr_raster_write_vs (LqrRaster * r /*, pngwriter& output */ );	/* output the visibility map (the seams) */

/* LQR_EXTERNAL FUNCTIONS */

void lqr_external_readimage (LqrRaster * r, GimpDrawable * drawable);
void lqr_external_readbias (LqrRaster * r, gint32 layer_ID, gint bias_factor);
void lqr_external_writeimage (LqrRaster * r, GimpDrawable * drawable);

#endif /* __RENDER_H__ */
