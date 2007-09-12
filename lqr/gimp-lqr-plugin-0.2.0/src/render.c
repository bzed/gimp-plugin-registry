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


#include "config.h"

#include <gtk/gtk.h>

#include <libgimp/gimp.h>

#include "main.h"
#include "render.h"

#include "plugin-intl.h"


/* #define __LQR_DEBUG__ */

/**** DATA CLASS FUNCTIONS ****/

/*** constructors ***/

/* standard constructor from RGB values */
LqrData *
init_lqr_data (double r, double g, double b)
{
  LqrData *d;

  d = (LqrData *) calloc (1, sizeof (LqrData));
  d->rgb[0] = r;
  d->rgb[1] = g;
  d->rgb[2] = b;
  d->e = 0;
  d->b = 0;
  d->m = 0;
  d->vs = 0;
  d->least = NULL;
  d->least_x = 0;
  return d;
}

/* reset data */
void
lqr_data_reset (LqrData * d, int bpp)
{
  int k;
  d->e = 0;
  d->b = 0;
  d->m = 0;
  d->vs = 0;
  d->least = NULL;
  d->least_x = 0;
  for (k = 0; k < bpp; k++)
    {
      d->rgb[k] = 0;
    }
}


/* read average color value */
double
lqr_data_read (LqrData * d, int bpp)
{
  double sum = 0;
  int k;
  for (k = 0; k < bpp; k++)
    {
      sum += d->rgb[k];
    }
  return sum / bpp;
}

/**** END OF LQR_DATA CLASS FUNCTIONS ****/



/**** LQR_CURSOR STRUCT FUNTIONS ****/

/*** constructor and destructor ***/

LqrCursor *
lqr_cursor_create (LqrRaster * owner, LqrData * m, int num)
{
  LqrCursor *c;
  int i;

  c = (LqrCursor *) calloc (num, sizeof (LqrCursor));
  for (i = 0; i < num; i++)
    {
      c[i].o = owner;
      c[i].map = m;
      c[i].initialized = 1;
      lqr_cursor_reset (&(c[i]));
    }
  return c;
}

void
lqr_cursor_destroy (LqrCursor * c)
{
  free (c);
}

/*** functions for moving around ***/

/* resets to starting point */
void
lqr_cursor_reset (LqrCursor * c)
{
  /* make sure the pointers are initialized */
#ifdef __LQR_DEBUG__
  assert (c->initialized);
#endif // __LQR_DEBUG__

  /* reset coordinates */
  c->x = 1;
  c->y = 1;

  /* set the current point to the beginning of the map */
  c->now = c->map;

  /* skip invisible points */
  while ((c->now->vs != 0) && (c->now->vs < c->o->level))
    {
      c->now++;
#ifdef __LQR_DEBUG__
      assert (c->now - c->map < c->o->w0);
#endif // __LQR_DEBUG__
    }
}

/* point to given data, assuming given coordinates */
void
lqr_cursor_seek (LqrCursor * c, LqrData * n, int x1, int y1)
{
  int pos;

  /* checks */
#ifdef __LQR_DEBUG__
  assert (c->initialized);
  assert ((x1 > 0) && (x1 <= c->o->w));
  assert ((y1 > 0) && (y1 <= c->o->h));
#endif // __LQR_DEBUG__

  /* set coordinates */
  c->x = x1;
  c->y = y1;

  /* set pointer */
  c->now = n;

  pos = c->now - c->map;
#ifdef __LQR_DEBUG__
  assert ((pos >= 0) && (pos < c->o->w0 * c->o->h0));
  assert ((c->now->vs == 0) || (c->now->vs < c->o->level));
#endif // __LQR_DEBUG__
}

/* jump to given coordinates (slow) */
void
lqr_cursor_seekxy (LqrCursor * c, int x1, int y1)
{
  int i;

#ifdef __LQR_DEBUG__
  assert (c->initialized);
  assert ((x1 >= 1) && (x1 <= c->o->w) && (y1 >= 1) && (y1 <= c->o->h));
#endif // __LQR_DEBUG__
  c->x = x1;
  c->y = y1;

  /* set position at the beginning of the y'th row */
  c->now = (&(c->map[(c->y - 1) * c->o->w0]));

  /* move right until x'th point is found */
  for (i = 0; i < c->x; i++)
    {
      while ((c->now->vs != 0) && (c->now->vs < c->o->level))
        {
          c->now++;
#ifdef __LQR_DEBUG__
          assert (c->now - c->map < c->y * c->o->w0);
#endif // __LQR_DEBUG__
        }
      c->now++;
    }
  c->now--;

  /* check that we are still in the y'th row */
#ifdef __LQR_DEBUG__
  assert (c->now - c->map < c->y * c->o->w0);
#endif // __LQR_DEBUG__
}


/* go to next data (first rows, then columns;
 * does nothing if we are already at the top-right corner) */
void
lqr_cursor_next (LqrCursor * c)
{
#ifdef __LQR_DEBUG__
  assert (c->initialized);
#endif // __LQR_DEBUG__

  /* update coordinates */
  if (c->x == c->o->w)
    {
      if (c->y == c->o->h)
        {
          /* top-right corner, do nothing */
          return;
        }
      /* end-of-line, carriage return */
      c->x = 1;
      c->y++;
    }
  else
    {
      /* simple right move */
      c->x++;
    }

  /* first move */
  c->now++;
#ifdef __LQR_DEBUG__
  assert (c->now - c->map < (c->o->w0 * c->o->h0));
#endif // __LQR_DEBUG__

  /* skip invisible points */
  while ((c->now->vs != 0) && (c->now->vs < c->o->level))
    {
      c->now++;
#ifdef __LQR_DEBUG__
      assert (c->now - c->map < (c->o->w0 * c->o->h0));
#endif // __LQR_DEBUG__
    }
}

/* go to previous data (behaves opposite to next) */
void
lqr_cursor_prev (LqrCursor * c)
{
  /* update coordinates */
  if (c->x == 1)
    {
      if (c->y == 1)
        {
          /* bottom-right corner, do nothing */
          return;
        }
      /* carriage return */
      c->x = c->o->w;
      c->y--;
    }
  else
    {
      /* simple left move */
      c->x--;
    }

  /* first move */
  c->now--;
#ifdef __LQR_DEBUG__
  assert (c->now - c->map >= 0);
#endif // __LQR_DEBUG__

  /* skip invisible points */
  while ((c->now->vs != 0) && (c->now->vs < c->o->level))
    {
      c->now--;
#ifdef __LQR_DEBUG__
      assert (c->now - c->map >= 0);
#endif // __LQR_DEBUG__
    }
}


/*** methods for exploring neighborhoods ***/

/* these return pointers to neighboring data
 * it is an error to ask for out-of-bounds data
 * right and left are farily efficient, top and bottom are not */

LqrData *
lqr_cursor_right (LqrCursor * c)
{
  /* create an auxiliary pointer */
  LqrData *ret = c->now;

#ifdef __LQR_DEBUG__
  assert (c->initialized);
  assert (c->x < c->o->w);
#endif // __LQR_DEBUG__

  /* first move */
  ret++;
#ifdef __LQR_DEBUG__
  assert (ret - c->map < c->o->w0 * c->o->h0);
#endif // __LQR_DEBUG__

  /* skip invisible points */
  while ((ret->vs != 0) && ret->vs < c->o->level)
    {
      ret++;
#ifdef __LQR_DEBUG__
      assert (ret - c->map < c->o->w0 * c->o->h0);
#endif // __LQR_DEBUG__
    }
  return ret;
}

LqrData *
lqr_cursor_left (LqrCursor * c)
{
  /* create an auxiliary pointer */
  LqrData *ret = c->now;

#ifdef __LQR_DEBUG__
  assert (c->initialized);
  assert (c->x > 1);
#endif // __LQR_DEBUG__

  /* first move */
  ret--;
#ifdef __LQR_DEBUG__
  assert (ret >= c->map);
#endif // __LQR_DEBUG__

  /* skip invisible points */
  while ((ret->vs != 0) && ret->vs < c->o->level)
    {
      ret--;
#ifdef __LQR_DEBUG__
      assert (ret >= c->map);
#endif // __LQR_DEBUG__
    }
  return ret;
}

LqrData *
lqr_cursor_up (LqrCursor * c)
{
  int i;

  /* create an auxiliary pointer
   * at the beginning of the line y + 1
   * (note: the "y" inside map[] reads "y + 1 - 1") */
  LqrData *ret = &(c->map[c->y * c->o->w0]);

#ifdef __LQR_DEBUG__
  assert (c->initialized);
  assert (c->y < c->o->h);
#endif // __LQR_DEBUG__

  /* span the line to find the point with the same x as current */
  for (i = 0; i < c->x; i++)
    {
      while ((ret->vs != 0) && ret->vs < c->o->level)
        {
          ret++;
#ifdef __LQR_DEBUG__
          assert (ret - c->map < c->o->w0 * c->o->h0);
#endif // __LQR_DEBUG__
        }
      ret++;
    }
  ret--;

  /* checks */
#ifdef __LQR_DEBUG__
  assert (ret - c->map < c->o->w0 * c->o->h0);
  assert (ret >= c->map);
#endif // __LQR_DEBUG__
  return ret;
}

LqrData *
lqr_cursor_down (LqrCursor * c)
{
  int i;

  /* create an auxiliary pointer
   * at the beginning of the line y - 1 */
  LqrData *ret = &(c->map[(c->y - 2) * (c->o->w0)]);

#ifdef __LQR_DEBUG__
  assert (c->initialized);
  assert (c->y > 1);
#endif // __LQR_DEBUG__

  /* span the line to find the point with the same x as current */
  for (i = 0; i < c->x; i++)
    {
      while ((ret->vs != 0) && ret->vs < c->o->level)
        {
          ret++;
#ifdef __LQR_DEBUG__
          assert (ret - c->map < c->o->w0 * c->o->h0);
#endif // __LQR_DEBUG__
        }
      ret++;
    }
  ret--;

  /* checks */
#ifdef __LQR_DEBUG__
  assert (ret - c->map < c->o->w0 * c->o->h0);
  assert (ret >= c->map);
#endif // __LQR_DEBUG__
  return ret;
}

/**** END OF LQR_CURSOR_CURSOR CLASS FUNCTIONS ****/



/**** GRADIENT FUNCTIONS ****/

inline double
norm (double x, double y)
{
  return sqrt (x * x + y * y);
}
inline double
norm_bias (double x, double y)
{
  return sqrt (x * x + 0.1 * y * y);
}
inline double
sumabs (double x, double y)
{
  return (fabs (x) + fabs (y)) / 2;
}
inline double
xabs (double x, double y)
{
  return fabs (x);
}
inline double
yabs (double x, double y)
{
  return fabs (y);
}

/**** END OF GRADIENT FUNCTIONS ****/




/**** LQR_RASTER CLASS FUNCTIONS ****/

/*** constructor & destructor ***/

/* constructor
 * first argument is the image to be manipulated
 * second argument (optional) specifies the gradient function */
LqrRaster *
lqr_raster_create (GimpDrawable * drawable, gint32 pres_layer_ID,
                   gint pres_coeff, gint32 disc_layer_ID, gint disc_coeff,
                   LqrGradFunc gf_ind, gboolean update_e)
{
  LqrRaster *r;

  r = (LqrRaster *) calloc (1, sizeof (LqrRaster));

  r->level = 1;
  r->max_level = 1;
  r->transposed = 0;
  r->update_e = update_e;

  r->h = gimp_drawable_height (drawable->drawable_id);
  r->w = gimp_drawable_width (drawable->drawable_id);
  r->bpp = gimp_drawable_bpp (drawable->drawable_id);

  g_assert (r->bpp <= _LQR_DATA_MAX_BPP);

  r->w0 = r->w;
  r->h0 = r->h;
  r->w_start = r->w;
  r->h_start = r->h;

  /* allocate memory for internal structures */
  //r->vpath = (LqrCursor *) calloc (r->h + 1, sizeof (LqrCursor));
  r->map = (LqrData *) calloc (r->w * r->h, sizeof (LqrData));
  r->vpath = lqr_cursor_create (r, r->map, r->h + 1);

  /* initialize cursors */

  r->c = lqr_cursor_create (r, r->map, 1);
  r->c_up = lqr_cursor_create (r, r->map, 1);
  r->c_down = lqr_cursor_create (r, r->map, 1);

  /* read input image */
  lqr_external_readimage (r, drawable);

  lqr_external_readbias (r, pres_layer_ID, pres_coeff);
  lqr_external_readbias (r, disc_layer_ID, -disc_coeff);

  /* set gradient function */
  lqr_raster_set_gf (r, gf_ind);

  return r;
}


/* destructor */
void
lqr_raster_destroy (LqrRaster * r)
{
  free (r->map);
  lqr_cursor_destroy (r->vpath);
  lqr_cursor_destroy (r->c);
  lqr_cursor_destroy (r->c_up);
  lqr_cursor_destroy (r->c_down);
  free (r);
}


/*** gradient related functions ***/

/* set the gradient function for energy computation 
 * choosing among those those listed in enum LqrGradFunc
 * the arguments are the x, y components of the gradient */
void
lqr_raster_set_gf (LqrRaster * r, LqrGradFunc gf_ind)
{
  switch (gf_ind)
    {
    case LQR_GF_NORM:
      r->gf = &norm;
      break;
    case LQR_GF_NORM_BIAS:
      r->gf = &norm_bias;
      break;
    case LQR_GF_SUMABS:
      r->gf = &sumabs;
      break;
    case LQR_GF_XABS:
      r->gf = &xabs;
      break;
    case LQR_GF_YABS:
      r->gf = &yabs;
      break;
    default:
      assert (0);
    }
}



/*** compute maps (energy, minpath & visibility ***/

/* build multisize image up to given depth
 * it is progressive (can be called multilple times) */
void
lqr_raster_build_maps (LqrRaster * r, int depth)
{
#ifdef __LQR_DEBUG__
  assert (depth <= r->w_start);
  assert (depth >= 1);
#endif // __LQR_DEBUG__

  /* only go deeper if needed */
  if (depth > r->max_level)
    {
      /* set to minimum width */
      lqr_raster_set_width (r, r->w_start - r->max_level + 1);

      /* compute energy & minpath maps */
      lqr_raster_build_emap (r);
      lqr_raster_build_mmap (r);

      /* compute visibility */
      lqr_raster_build_vsmap (r, depth);
    }
}

/* compute energy map */
void
lqr_raster_build_emap (LqrRaster * r)
{
  int x, y;

  /* reset current cursor */
  lqr_cursor_reset (r->c);

  /* set auxiliary up and down cursors */
  if (r->h > 1)
    {
      lqr_cursor_seekxy (r->c_up, 1, 2);
    }
  lqr_cursor_reset (r->c_down);

  for (y = 1; y <= r->h; y++)
    {
      for (x = 1; x <= r->w; x++)
        {
          lqr_raster_compute_e (r);
          lqr_cursor_next (r->c);
          lqr_cursor_next (r->c_up);
          if (y > 1)
            {
              /* this starts form the second row */
              lqr_cursor_next (r->c_down);
            }
        }
    }
}

/* compute auxiliary minpath map
 * defined as
 * y = 1 : m(x,y) = e(x,y)
 * y > 1 : m(x,y) = min{ m(x-1,y-1), m(x,y-1), m(x+1,y-1) } + e(x,y) */
void
lqr_raster_build_mmap (LqrRaster * r)
{
  int x, y;
  double m, m1, m2;

  /* span first row */
  lqr_cursor_reset (r->c);
  for (x = 1; x <= r->w; x++)
    {
      r->c->now->m = r->c->now->e;
      r->c->now->least = NULL;
      r->c->now->least_x = 0;
      lqr_cursor_next (r->c);
    }

  /* set down cursor */
  lqr_cursor_reset (r->c_down);

  /* span all other rows */
  for (y = 2; y <= r->h; y++)
    {
      for (x = 1; x <= r->w; x++)
        {
          /* find the min among the 3 neighbors
           * in the last row */
          m = r->c_down->now->m;
          r->c->now->least = r->c_down->now;
          r->c->now->least_x = 0;
          if (x == r->w)
            {
              m1 = 1 << 29;
            }
          else
            {
              m1 = (lqr_cursor_right (r->c_down))->m;
            }
          if (x == 1)
            {
              m2 = 1 << 29;
            }
          else
            {
              m2 = (lqr_cursor_left (r->c_down))->m;
            }
          if (m1 < m)
            {
              m = m1;
              r->c->now->least = lqr_cursor_right (r->c_down);
              r->c->now->least_x = 1;
            }
          if (m2 < m)
            {
              m = m2;
              r->c->now->least = lqr_cursor_left (r->c_down);
              r->c->now->least_x = -1;
            }
#ifdef __LQR_DEBUG__
          assert (m < 1 << 29);
#endif // __LQR_DEBUG__

          /* set current m */
          r->c->now->m = r->c->now->e + m;

          /* advance cursors */
          lqr_cursor_next (r->c);
          lqr_cursor_next (r->c_down);
        }
    }
}

/* compute (vertical) visibility map up to given depth
 * (it also calls inflate() to add image enlargment information) */
void
lqr_raster_build_vsmap (LqrRaster * r, int depth)
{
  int z, l;

#ifdef __LQR_DEBUG__
  assert (depth <= r->w_start + 1);
  assert (depth >= 1);
#endif // __LQR_DEBUG__

  /* default behavior : compute all possible levels
   * (complete map) */
  if (depth == 0)
    {
      depth = r->w_start + 1;
    }

  /* here we assume that
   * lqr_raster_set_width(w_start - max_level + 1);
   * has been given */

  /* reset visibility map and level (WHY????) */
  if (r->max_level == 1)
    {
      for (z = 0; z < r->w0 * r->h0; z++)
        {
          r->map[z].vs = 0;
        }
    }

  /* cycle over levels */
  for (l = r->max_level; l < depth; l++)
    {

      if ((l - r->max_level) % 10 == 0)
        {
          gimp_progress_update ((gdouble) (l - r->max_level) /
                                (gdouble) (depth - r->max_level));
        }

      /* compute vertical seam */
      lqr_raster_build_vpath (r);

      /* update visibility map
       * (assign level to the seam) */
      lqr_raster_update_vsmap (r, l + r->max_level - 1);

      /* increase (in)visibility level
       * (make the last seam invisible) */
      r->level++;
      r->w--;

      if (r->w > 1)
        {
          /* update the energy */
          if (r->update_e == TRUE)
            {
              lqr_raster_update_emap (r);
            }
          /* recalculate the minpath map */
          //lqr_raster_update_mmap (r);
          lqr_raster_build_mmap (r);
        }
      else
        {
          /* complete the map (last seam) */
          lqr_raster_finish_vsmap (r);
        }
    }

  /* reset width to the maximum */
  lqr_raster_set_width (r, r->w0);

  /* insert seams for image enlargement */
  lqr_raster_inflate (r, depth - 1);

  /* set new max_level */
  r->max_level = depth;

  /* reset image size */
  lqr_raster_set_width (r, r->w_start);
}

/* enlarge the image by seam insertion
 * visibility map is updated and the resulting multisize image
 * is complete in both directions */
void
lqr_raster_inflate (LqrRaster * r, int l)
{
  int w1, z0, vs, k;
  LqrData *newmap;

#ifdef __LQR_DEBUG__
  assert (l + 1 > r->max_level);        /* otherwise is useless */
#endif // __LQR_DEBUG__

  /* scale to current maximum size
   * (this is the original size the first time) */
  lqr_raster_set_width (r, r->w0);

  /* final width */
  w1 = r->w0 + l - r->max_level + 1;

  /* allocate room for new map */
  newmap = (LqrData *) calloc (w1 * r->h0, sizeof (LqrData));

  /* span the image with a cursor
   * and build the new image */
  lqr_cursor_reset (r->c);
  for (z0 = 0; z0 < w1 * r->h0; z0++, lqr_cursor_next (r->c))
    {
      /* read visibility */
      vs = r->c->now->vs;
      if ((vs != 0) && (vs <= l + r->max_level - 1)
          && (vs >= 2 * r->max_level - 1))
        {
          /* the point belongs to a previously computed seam
           * and was not inserted during a previous
           * inflate() call : insert another seam */
          newmap[z0] = *(r->c->now);
          /* the new pixel value is equal to the average of its
           * left and right neighbors */
          if (r->c->x > 1)
            {
              for (k = 0; k < r->bpp; k++)
                {
                  newmap[z0].rgb[k] =
                    (lqr_cursor_left (r->c)->rgb[k] + r->c->now->rgb[k]) / 2;
                }
            }
          /* the first time inflate() is called
           * the new visibility should be -vs + 1 but we shift it
           * so that the final minimum visibiliy will be 1 again
           * and so that vs=0 still means "uninitialized"
           * subsequent inflations have to account for that */
          newmap[z0].vs = l - vs + r->max_level;
          z0++;
        }
      newmap[z0] = *(r->c->now);
      if (vs != 0)
        {
          /* visibility has to be shifted up */
          newmap[z0].vs = vs + l - r->max_level + 1;
        }
    }

  /* substitute maps */
  free (r->map);
  r->map = newmap;

  /* set new widths & levels (w_start is kept for reference) */
  r->level = l + 1;
  r->w0 = w1;
  r->w = r->w_start;

  /* reset seam path and cursors */
  lqr_cursor_destroy (r->vpath);
  r->vpath = lqr_cursor_create (r, r->map, r->h + 1);
  lqr_cursor_destroy (r->c);
  r->c = lqr_cursor_create (r, r->map, 1);
  lqr_cursor_destroy (r->c_up);
  r->c_up = lqr_cursor_create (r, r->map, 1);
  lqr_cursor_destroy (r->c_down);
  r->c_down = lqr_cursor_create (r, r->map, 1);
}



/*** internal functions for maps computations ***/

/* compute energy of point pointed to by cursor c
 * auxiliary cursors c_up and c_down have to be consistent */
void
lqr_raster_compute_e (LqrRaster * r)
{
  int x = r->c->x;
  int y = r->c->y;
  double gx, gy;
  if (y == 1)
    {
#ifdef __LQR_DEBUG__
      assert ((r->c_up->x == x) && (r->c_up->y == y + 1));
#endif // __LQR_DEBUG__
      gy = lqr_data_read (r->c_up->now, r->bpp) -
        lqr_data_read (r->c->now, r->bpp);
    }
  else if (y < r->h)
    {
#ifdef __LQR_DEBUG__
      assert ((r->c_up->x == x) && (r->c_up->y == y + 1)
              && (r->c_down->x == x) && (r->c_down->y == y - 1));
#endif // __LQR_DEBUG__
      gy =
        (lqr_data_read (r->c_up->now, r->bpp) -
         lqr_data_read (r->c_down->now, r->bpp)) / 2;
    }
  else
    {
#ifdef __LQR_DEBUG__
      assert ((r->c_down->x == x) && (r->c_down->y == y - 1));
#endif // __LQR_DEBUG__
      gy = lqr_data_read (r->c->now, r->bpp) -
        lqr_data_read (r->c_down->now, r->bpp);
    }
  if (x == 1)
    {
      gx = lqr_data_read (lqr_cursor_right (r->c), r->bpp) -
        lqr_data_read (r->c->now, r->bpp);
    }
  else if (x < r->w)
    {
      gx = (lqr_data_read (lqr_cursor_right (r->c), r->bpp) -
            lqr_data_read (lqr_cursor_left (r->c), r->bpp)) / 2;
    }
  else
    {
      gx = lqr_data_read (r->c->now, r->bpp) -
        lqr_data_read (lqr_cursor_left (r->c), r->bpp);
    }
  r->c->now->e = (*(r->gf)) (gx, gy) + r->c->now->b;
}

/* update energy map after seam removal
 * (the only affected energies are to the
 * left and right of the removed seam) */
void
lqr_raster_update_emap (LqrRaster * r)
{
  int x, y;
  for (y = 1; y <= r->h; y++)
    {
      /* set the cursor to the seam point
       * (it is an invalid cursor since it points
       * to an invisible poinit)  */
      (*r->c) = r->vpath[y];

      x = r->c->x;
      if (x > 1)
        {
          /* move to the left
           * (the x is consistent in this case) */
          lqr_cursor_prev (r->c);

          /* set up & down cursors */
          if (y < r->h)
            {
              lqr_cursor_seekxy (r->c_up, x - 1, y + 1);
            }
          if (y > 1)
            {
              lqr_cursor_seekxy (r->c_down, x - 1, y - 1);
            }

          /* update */
          lqr_raster_compute_e (r);
        }
      if (x < r->w + 1)
        {
          /* move to the right */
          lqr_cursor_next (r->c);
          /* the x is inconsistent
           * (it is x+1 but should be x
           * due to seam removal) */
          r->c->x = x;

          /* set up & down cursors */
          if (y < r->h)
            {
              lqr_cursor_seekxy (r->c_up, x, y + 1);
            }
          if (y > 1)
            {
              lqr_cursor_seekxy (r->c_down, x, y - 1);
            }

          /* update */
          lqr_raster_compute_e (r);
        }
    }
}

void
lqr_raster_update_mmap (LqrRaster * r)
{
  int x, y;
  int xmin, xmax;
  double m, m1, m2;
  LqrCursor *cmin;
  LqrData *old_least;
  int old_least_x;

  cmin = lqr_cursor_create (r, r->map, 1);

  /* span first row */
  *(cmin) = r->vpath[1];

  if (cmin->x > 1)
    {
      lqr_cursor_prev (cmin);
    }
  else
    {
      lqr_cursor_next (cmin);
      cmin->x--;
    }
  xmin = cmin->x;

  xmax = MIN (r->vpath[1].x + 1, r->w);

  *(r->c) = *cmin;
  for (x = xmin; x <= xmax; x++)
    {
      r->c->now->m = r->c->now->e;
      //r->c->now->least = NULL;
      //r->c->now->least_x = 0;
      lqr_cursor_next (r->c);
    }

  for (y = 2; y <= r->h; y++)
    {
      while ((xmin > r->vpath[y].x) && (xmin > 1))
        {
          lqr_cursor_prev (cmin);
          xmin--;
        }
      if (xmax <= r->vpath[y].x)
        {
          xmax = MAX (r->vpath[y].x, r->w);
        }
      *(r->c_down) = *cmin;
      if (xmin > 1)
        {
          lqr_cursor_prev (r->c_down);
        }
      xmin = MAX (xmin - 1, 1);
      xmax = MIN (xmax + 1, r->w);
      lqr_cursor_seekxy (cmin, xmin, y);
      *(r->c) = *cmin;
      for (x = xmin; x <= xmax; x++)
        {
          m = r->c_down->now->m;
          old_least = r->c->now->least;
          old_least_x = r->c->now->least_x;
          r->c->now->least = r->c_down->now;
          r->c->now->least_x = 0;

          if (x == r->w)
            {
              m1 = 1 << 29;
            }
          else
            {
              m1 = (lqr_cursor_right (r->c_down))->m;
            }
          if (x == 1)
            {
              m2 = 1 << 29;
            }
          else
            {
              m2 = (lqr_cursor_left (r->c_down))->m;
            }

          if (m1 < m)
            {
              m = m1;
              r->c->now->least = lqr_cursor_right (r->c_down);
              r->c->now->least_x = 1;
            }
          if (m2 < m)
            {
              m = m2;
              r->c->now->least = lqr_cursor_left (r->c_down);
              r->c->now->least_x = -1;
            }

          if ((x == xmin) && (x < r->vpath[y].x)
              && (r->c->now->least == old_least)
              && (r->c->now->least_x == old_least_x)
              && (r->c->now->m == r->c->now->e + m))
            {
              xmin++;
              lqr_cursor_next (cmin);
            }
          if ((x == xmax) && (x >= r->vpath[y].x)
              && (r->c->now->least == old_least)
              && (r->c->now->least_x == old_least_x)
              && (r->c->now->m == r->c->now->e + m))
            {
              xmax--;
            }


          /* set current m */
          r->c->now->m = r->c->now->e + m;

          /* advance cursors */
          lqr_cursor_next (r->c);
          lqr_cursor_next (r->c_down);
#ifdef __LQR_DEBUG__
          assert (m < 1 << 29);
#endif // __LQR_DEBUG__
        }

    }
  lqr_cursor_destroy (cmin);
}


/* compute seam path from minpath map */
void
lqr_raster_build_vpath (LqrRaster * r)
{
  int x, y, last_x;
  double m, m1;
  LqrCursor *last;

  /* we start at last row */
  y = r->h;

  /* we create an auxiliary cursor for reference */
  last_x = 1;
  last = lqr_cursor_create (r, r->map, 1);

  /* span the last row for the minimum mmap value */
  lqr_cursor_seekxy (r->c, 1, r->h);
  m = r->c->now->m;
  (*last) = (*(r->c));
  lqr_cursor_next (r->c);
  for (x = 2; x <= r->w; x++, lqr_cursor_next (r->c))
    {
      m1 = r->c->now->m;
      if (m1 < m)
        {
          last_x = x;
          (*last) = (*(r->c));
          m = m1;
        }
    }

#ifdef __LQR_DEBUG__
  assert (r->c->x == r->w);
  assert (last->x == last_x);
#endif // __LQR_DEBUG__

  /* we now have the endpoint of the seam */
  r->vpath[r->h] = (*last);
#ifdef __LQR_DEGUG__
  assert (r->vpath[r->h].y == r->h);
#endif // __LQR_DEBUG__

  /* we backtrack the seam following the min mmap */
  for (y = r->h - 1; y >= 1; y--)
    {
      lqr_cursor_seek (&(r->vpath[y]), last->now->least,
                       last->x + last->now->least_x, y);
      (*last) = (r->vpath[y]);
      assert (r->vpath[y].y == y);
    }

  /* release memory  */
  lqr_cursor_destroy (last);
}

/* update visibility map after seam computation */
void
lqr_raster_update_vsmap (LqrRaster * r, int l)
{
  int y;
  for (y = 1; y <= r->h; y++)
    {
#ifdef __LQR_DEBUG__
      assert (r->vpath[y].now->vs == 0);
#endif // __LQR_DEBUG__
      r->vpath[y].now->vs = l;
    }
}

/* complete visibility map (last seam) */
/* set the last column of pixels to vis. level w0 */
void
lqr_raster_finish_vsmap (LqrRaster * r)
{
  int y;

#ifdef __LQR_DEBUG__
  assert (r->w == 1);
#endif // __LQR_DEBUG__
  lqr_cursor_reset (r->c);
  for (y = 1; y <= r->h; y++, lqr_cursor_next (r->c))
    {
#ifdef __LQR_DEBUG__
      assert (r->c->now->vs == 0);
#endif // __LQR_DEBUG__
      r->c->now->vs = r->w0;
    }
}


/*** image manipulations ***/

/* set width of the multisize image
 * (maps have to be computed already) */
void
lqr_raster_set_width (LqrRaster * r, int w1)
{
#ifdef __LQR_DEBUG__
  assert (w1 <= r->w0);
  assert (w1 >= r->w_start - r->max_level + 1);
#endif // __LQR_DEBUG__
  r->w = w1;
  r->level = r->w0 - w1 + 1;
}



/* flatten the image to its current state
 * (all maps are reset, invisible points are lost) */
void
lqr_raster_flatten (LqrRaster * r)
{
  LqrData *newmap;
  int z0;

  /* allocate room for new map */
  newmap = (LqrData *) calloc (r->w * r->h, sizeof (LqrData));

  /* span the image with the cursor and copy
   * it in the new array  */
  lqr_cursor_reset (r->c);
  for (z0 = 0; z0 < r->w * r->h; z0++, lqr_cursor_next (r->c))
    {
      newmap[z0] = *(r->c->now);
      newmap[z0].e = 0;
      newmap[z0].m = 0;
      newmap[z0].vs = 0;
    }

  /* substitute the map */
  free (r->map);
  r->map = newmap;

  /* reset widths, heights & levels */
  r->w0 = r->w;
  r->h0 = r->h;
  r->w_start = r->w;
  r->h_start = r->h;
  r->level = 1;
  r->max_level = 1;

  /* reset seam path and cursors */
  lqr_cursor_destroy (r->vpath);
  r->vpath = lqr_cursor_create (r, r->map, r->h + 1);
  lqr_cursor_destroy (r->c);
  r->c = lqr_cursor_create (r, r->map, 1);
  lqr_cursor_destroy (r->c_up);
  r->c_up = lqr_cursor_create (r, r->map, 1);
  lqr_cursor_destroy (r->c_down);
  r->c_down = lqr_cursor_create (r, r->map, 1);
}

/* transpose the image, in its current state
 * (all maps and invisible points are lost) */
void
lqr_raster_transpose (LqrRaster * r)
{
  int x, y;
  int z0, z1;
  int d;
  LqrData *newmap;

  if (r->level > 1)
    {
      lqr_raster_flatten (r);
    }

  /* allocate room for the new map */
  newmap = (LqrData *) calloc (r->w0 * r->h0, sizeof (LqrData));

  /* compute trasposed map */
  for (x = 1; x <= r->w; x++)
    {
      for (y = 1; y <= r->h; y++)
        {
          z0 = (y - 1) * r->w0 + (x - 1);
          z1 = (x - 1) * r->h0 + (y - 1);
          newmap[z1] = r->map[z0];
        }
    }

  /* substitute the map */
  free (r->map);
  r->map = newmap;

  /* switch widths & heights */
  d = r->w0;
  r->w0 = r->h0;
  r->h0 = d;
  r->w = r->w0;
  r->h = r->h0;

  /* reset w_start, h_start & levels */
  r->w_start = r->w0;
  r->h_start = r->h0;
  r->level = 1;
  r->max_level = 1;

  /* reset seam path and cursors */
  lqr_cursor_destroy (r->vpath);
  r->vpath = lqr_cursor_create (r, r->map, r->h + 1);
  lqr_cursor_destroy (r->c);
  r->c = lqr_cursor_create (r, r->map, 1);
  lqr_cursor_destroy (r->c_up);
  r->c_up = lqr_cursor_create (r, r->map, 1);
  lqr_cursor_destroy (r->c_down);
  r->c_down = lqr_cursor_create (r, r->map, 1);

  /* set transposed flag */
  r->transposed = (r->transposed ? 0 : 1);
}


/* liquid resize : this is the main method
 * it automatically determines the depth of the map
 * according to the desired size */
void
lqr_raster_resize (LqrRaster * r, int w1, int h1)
{
  /* resize width */
  int delta, gamma;
  if (!r->transposed)
    {
      delta = w1 - r->w_start;
      gamma = w1 - r->w;
    }
  else
    {
      delta = w1 - r->h_start;
      gamma = w1 - r->h;
    }
  delta = delta > 0 ? delta : -delta;
  if (gamma)
    {
      if (r->transposed)
        {
          lqr_raster_transpose (r);
        }
      gimp_progress_init (_("Resizing width..."));
      lqr_raster_build_maps (r, delta + 1);
      lqr_raster_set_width (r, w1);
    }

  /* resize height */
  if (!r->transposed)
    {
      delta = h1 - r->h_start;
      gamma = h1 - r->h;
    }
  else
    {
      delta = h1 - r->w_start;
      gamma = h1 - r->w;
    }
  delta = delta > 0 ? delta : -delta;
  if (gamma)
    {
      if (!r->transposed)
        {
          lqr_raster_transpose (r);
        }
      gimp_progress_init (_("Resizing height..."));
      lqr_raster_build_maps (r, delta + 1);
      lqr_raster_set_width (r, h1);
    }
}



/*** output ***/

/* plot the energy (at current size / visibility) to a file
 * (greyscale) */
void
lqr_raster_write_energy (LqrRaster * r /*, pngwriter& output */ )
{
  int x, y;
  double e;

  if (!r->transposed)
    {
      /* external_resize(r->w, r->h); */
    }
  else
    {
      /* external_resize(r->h, r->w); */
    }

  lqr_cursor_reset (r->c);
  for (y = 1; y <= r->h; y++)
    {
      for (x = 1; x <= r->w; x++)
        {
          e = r->c->now->e;
          if (!r->transposed)
            {
              /* external_write(x, y, e, e, e); */
            }
          else
            {
              /* external_write(y, x, e, e, e); */
            }
          lqr_cursor_next (r->c);
        }
    }
}


/* plot the visibility level of the image
 * uses original size
 * uninitialized points are plotted black
 * others fade from yellow (first seams) to blue */
void
lqr_raster_write_vs (LqrRaster * r /*, pngwriter& output */ )
{
  int w1, x, y, vs;
  double value, rd, gr, bl;

  /* save current size */
  w1 = r->w;

  /* temporarily set the size to the original */
  lqr_raster_set_width (r, r->w_start);

  /* plot to file */
  if (!r->transposed)
    {
      /* external_resize(r->w, r->h); */
    }
  else
    {
      /* external_resize(r->h, r->w); */
    }

  lqr_cursor_reset (r->c);
  for (y = 1; y <= r->h; y++)
    {
      for (x = 1; x <= r->w; x++)
        {
          vs = r->c->now->vs;
          if (vs == 0)
            {
              if (!r->transposed)
                {
                  /* external_write(x, y, 0.0, 0.0, 0.0); */
                }
              else
                {
                  /* external_write(y, x, 0.0, 0.0, 0.0); */
                }
            }
          else
            {
              value =
                (double) (r->max_level -
                          (vs - r->w0 + r->w_start)) / r->max_level;
              rd = value > 0.5 ? 2 * value - 1 : 0;
              gr = rd;
              bl = value > 0.5 ? 1 - value : value;
              if (!r->transposed)
                {
                  /* external_write(x, y, rd, gr, bl); */
                }
              else
                {
                  /* external_write(y, x, rd, gr, bl); */
                }
            }
          lqr_cursor_next (r->c);
        }
    }

  /* recover size */
  lqr_raster_set_width (r, w1);
}


/**** EXTERNAL FUNCTIONS ****/

void
lqr_external_readimage (LqrRaster * r, GimpDrawable * drawable)
{
  gint x, y, k, bpp;
  gint x1, y1, x2, y2;
  GimpPixelRgn rgn_in;
  guchar *inrow;


  gimp_drawable_mask_bounds (drawable->drawable_id, &x1, &y1, &x2, &y2);

  bpp = gimp_drawable_bpp (drawable->drawable_id);

  gimp_pixel_rgn_init (&rgn_in,
                       drawable, x1, y1, x2 - x1, y2 - y1, FALSE, FALSE);

  gimp_drawable_offsets(drawable->drawable_id, &r->x_off, &r->y_off);

#ifdef __LQR_DEBUG__
  assert (x2 - x1 == r->w);
  assert (y2 - y1 == r->h);
#endif // __LQR_DEBUG__

  inrow = g_new (guchar, bpp * r->w);

  for (y = 0; y < r->h; y++)
    {
      gimp_pixel_rgn_get_row (&rgn_in, inrow, x1, y, r->w);

      for (x = 0; x < r->w; x++)
        {
          for (k = 0; k < bpp; k++)
            {
              r->map[y * r->w + x].rgb[k] = (double) inrow[bpp * x + k] / 255;
            }
        }

    }

  g_free (inrow);

}

void
lqr_external_readbias (LqrRaster * r, gint32 layer_ID, gint bias_factor)
{
  gint x, y, k, bpp;
  gint x1, y1, x2, y2;
  gint x_off, y_off;
  gint lw, lh;
  gint sum;
  GimpPixelRgn rgn_in;
  guchar *inrow;

  if ((layer_ID == 0) || (bias_factor == 0))
    {
      return;
    }

  gimp_drawable_offsets(layer_ID, &x_off, &y_off);

  gimp_drawable_mask_bounds (layer_ID, &x1, &y1, &x2, &y2);

  bpp = gimp_drawable_bpp (layer_ID);

  gimp_pixel_rgn_init (&rgn_in,
                       gimp_drawable_get (layer_ID), x1, y1, x2 - x1, y2 - y1,
                       FALSE, FALSE);

  x_off -= r->x_off;
  y_off -= r->y_off;

  lw = (MIN (r->w, x2 + x_off) - MAX (0, x1 + x_off));
  lh = (MIN (r->h, y2 + y_off) - MAX (0, y1 + y_off));

  //printf(" x1, y1=%i,%i  x2, y2=%i,%i  xo, yo=%i,%i  lw, lh=%i,%i\n", x1, y1, x2, y2, x_off, y_off, lw, lh);
  //printf("rx1,ry1=%i,%i rx2,ry2=%i,%i rxo,ryo=%i,%i rlw,rlh=%i,%i\n", 0, 0, r->w, r->h, r->x_off, r->y_off, r->w, r->h); fflush(stdout);

  inrow = g_new (guchar, bpp * lw);


  for (y = MAX (0, y1 + y_off); y < MIN (r->h, y2 + y_off); y++)
    {
      gimp_pixel_rgn_get_row (&rgn_in, inrow, MAX(x1, -x_off), y - y1 - y_off, lw);

      for (x = 0; x < lw; x++)
        {
          sum = 0;
          for (k = 0; k < bpp; k++)
            {
              sum += inrow[bpp * x + k];
            }

          r->map[y * r->w + (x + MAX(0, x1 + x_off))].b +=
            (double) bias_factor * sum / (100 * 255 * bpp);
        }

    }

  g_free (inrow);

}

/* write to file the currently visible image */
void
lqr_external_writeimage (LqrRaster * r, GimpDrawable * drawable)
{
  gint x, y, k;
  gint x1, y1, x2, y2;
  GimpPixelRgn rgn_out;
  guchar *outrow;

  gimp_drawable_mask_bounds (drawable->drawable_id, &x1, &y1, &x2, &y2);

  gimp_pixel_rgn_init (&rgn_out,
                       drawable, x1, y1, x2 - x1, y2 - y1, TRUE, TRUE);



  if (!r->transposed)
    {
      outrow = g_new (guchar, r->bpp * (x2 - x1));
    }
  else
    {
      outrow = g_new (guchar, r->bpp * (y2 - y1));
    }

  lqr_cursor_reset (r->c);

  for (y = 0; y < r->h; y++)
    {
      for (x = 0; x < r->w; x++)
        {
          for (k = 0; k < r->bpp; k++)
            {
              //outrow[r->bpp * x + k] = (int) (255 * fabs(r->c->now->b) / 10);
              outrow[r->bpp * x + k] = (int) (255 * r->c->now->rgb[k]);
            }
          lqr_cursor_next (r->c);
        }
      if (!r->transposed)
        {
          gimp_pixel_rgn_set_row (&rgn_out, outrow, x1, y + y1, x2 - x1);
        }
      else
        {
          gimp_pixel_rgn_set_col (&rgn_out, outrow, y + x1, y1, y2 - y1);
        }

    }

  g_free (outrow);

  gimp_drawable_flush (drawable);
  gimp_drawable_merge_shadow (drawable->drawable_id, TRUE);
  gimp_drawable_update (drawable->drawable_id, x1, y1, x2 - x1, y2 - y1);

}


/**** END OF LQR_RASTER CLASS FUNCTIONS ****/



void
render (gint32 image_ID,
        GimpDrawable * drawable,
        PlugInVals * vals,
        PlugInImageVals * image_vals, PlugInDrawableVals * drawable_vals)
{
  LqrRaster *rasta;
  gint32 mask_ID;
  gint32 layer_ID;

  drawable = gimp_drawable_get (gimp_image_get_active_drawable (image_ID));

  if (gimp_drawable_is_layer_mask (drawable->drawable_id) == TRUE)
    {
      layer_ID = gimp_layer_from_mask (drawable->drawable_id);
      drawable = gimp_drawable_get (layer_ID);
    }

  if (gimp_drawable_is_layer (drawable->drawable_id) == TRUE)
    {
      mask_ID = gimp_layer_get_mask (drawable->drawable_id);
      if (mask_ID != -1)
        {
          gimp_layer_remove_mask (drawable->drawable_id, vals->mask_behavior);
        }
    }

#ifdef __LQR_DEBUG__
  printf ("[ clock sofar: %g ]\n", (double) clock () / CLOCKS_PER_SEC);
#endif // __LQR_DEBUG__

  rasta = lqr_raster_create (drawable, vals->pres_layer_ID, vals->pres_coeff,
                             vals->disc_layer_ID, vals->disc_coeff,
                             vals->grad_func, vals->update_en);

  lqr_raster_resize (rasta, vals->new_width, vals->new_height);

  if (vals->resize_canvas == TRUE)
    {
      gimp_image_resize (image_ID, vals->new_width, vals->new_height, 0, 0);
      drawable =
        gimp_drawable_get (gimp_image_get_active_drawable (image_ID));
      gimp_layer_resize_to_image_size (drawable->drawable_id);
    }
  else
    {
      gimp_layer_resize (drawable->drawable_id, vals->new_width,
                         vals->new_height, 0, 0);
    }
  drawable = gimp_drawable_get (gimp_image_get_active_drawable (image_ID));

#ifdef __LQR_DEBUG__
  printf ("[ clock sofar: %g ]\n", (double) clock () / CLOCKS_PER_SEC);
#endif // __LQR_DEBUG__


  lqr_external_writeimage (rasta, drawable);
  lqr_raster_destroy (rasta);

#ifdef __LQR_DEBUG__
  printf ("[ clock sofar: %g ]\n", (double) clock () / CLOCKS_PER_SEC);
#endif // __LQR_DEBUG__

  //gimp_image_scale(image_ID, rasta->w_start, rasta->h_start);
}
