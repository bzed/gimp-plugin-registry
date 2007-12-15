/* Focus Blur -- blur with focus plug-in.
 * Copyright (C) 2002-2007 Kyoichiro Suda
 *
 * The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <libgimp/gimp.h>

#include "focusblur.h"
#include "focusblurparam.h"
#include "depthmap.h"
#ifdef HAVE_FFTW3
#  include "fftblurbuffer.h"
#endif
 

/*---- Structure ----*/

struct _FblurDepthMap
{
  /* specified values */
  gint32                 depth_map_ID;
  gfloat                 focal_depth;
  /* pre-computed values */
  gint                   focal_depth_int;
  /* main data */
  gint                   width, height;
  guchar                *data;
};


/*---- Prototypes ----*/

static gboolean focusblur_depth_map_make        (FblurDepthMap  *depth_map);


/*---- Functions ----*/

gboolean
focusblur_depth_map_update (FblurDepthMap               **depth_map,
                            FblurFftBuffer               *fft,
                            FblurStoreParam              *store)
{
  if (! store->enable_depth_map)
    return TRUE;

  if (*depth_map)
    {
      if ((*depth_map)->depth_map_ID == store->depth_map_ID)
        {
          if (store->focal_depth == (*depth_map)->focal_depth)
            /* not changed */
            return TRUE;

          (*depth_map)->focal_depth = store->focal_depth;
          (*depth_map)->focal_depth_int =
            rintf (FBLUR_DEPTH_MAX * (*depth_map)->focal_depth / 100.0f);

          if (! gimp_drawable_has_alpha ((*depth_map)->depth_map_ID))
            /* do not need to update */
            return TRUE;
        }

      focusblur_depth_map_destroy (depth_map);
    }

#ifdef HAVE_FFTW3
  if (fft)
    focusblur_fft_buffer_invalidate_depth_map (fft);
#endif

  if (! gimp_drawable_is_valid (store->depth_map_ID))
    return TRUE; /* ignore */

  (*depth_map) = g_new0 (FblurDepthMap, 1);

  (*depth_map)->depth_map_ID = store->depth_map_ID;
  (*depth_map)->focal_depth = store->focal_depth;
  /* pre-computed values */
  (*depth_map)->focal_depth_int =
    rintf (FBLUR_DEPTH_MAX * (*depth_map)->focal_depth / 100.0f);

  if (! focusblur_depth_map_make (*depth_map))
    {
      focusblur_depth_map_destroy (depth_map);
      return FALSE;
    }

  return TRUE;
}

static gboolean
focusblur_depth_map_make (FblurDepthMap *depth_map)
{
  GimpDrawable  *drawable;
  GimpPixelRgn   pr;
  gpointer       p;
  guchar        *slp, *sp;
  guchar        *dlp, *dp;
  gint           x, y, w, h;
  gint           focal;

  drawable = gimp_drawable_get (depth_map->depth_map_ID);

  if (! drawable)
    return FALSE;

  w = depth_map->width = drawable->width;
  h = depth_map->height = drawable->height;
  depth_map->data = g_new (guchar, w * h);
  if (! depth_map->data)
    return FALSE;

  gimp_pixel_rgn_init (&pr, drawable, 0, 0, w, h, FALSE, FALSE);
  p = gimp_pixel_rgns_register (1, &pr);

  switch (gimp_drawable_type (depth_map->depth_map_ID))
    {
    case GIMP_GRAY_IMAGE:
      g_assert (pr.bpp == 1);
      for (; p; p = gimp_pixel_rgns_process (p))
        for (y = pr.h, slp = pr.data,
               dlp = depth_map->data + pr.y * w + pr.x;
             y --; slp += pr.rowstride, dlp += w)
          for (x = pr.w, sp = slp, dp = dlp; x --; sp ++, dp ++)
            {
              *dp = *sp;
              /* variable depth division
              *dp = FBLUR_DEPTH_MAX * (*dp & mask) / mask;
                 or simply */
              *dp /= 2; // FBLUR_DEPTH_MAX = 127;
            }
      break;

    case GIMP_GRAYA_IMAGE:
      g_assert (pr.bpp == 2);
      focal = rintf (255 * depth_map->focal_depth / 100.0f);
      for (; p; p = gimp_pixel_rgns_process (p))
        for (y = pr.h, slp = pr.data,
               dlp = depth_map->data + pr.y * w + pr.x;
             y --; slp += pr.rowstride, dlp += w)
          for (x = pr.w, sp = slp, dp = dlp; x --; sp += 2, dp ++)
            {
              *dp = (sp[0] * sp[1] + focal * (255 - sp[1])) / 255;
              *dp /= 2; // FBLUR_DEPTH_MAX = 127;
            }
      break;

    case GIMP_RGB_IMAGE:
      g_assert (pr.bpp == 3);
      for (; p; p = gimp_pixel_rgns_process (p))
        for (y = pr.h, slp = pr.data,
               dlp = depth_map->data + pr.y * w + pr.x;
             y --; slp += pr.rowstride, dlp += w)
          for (x = pr.w, sp = slp, dp = dlp; x --; sp += 3, dp ++)
            {
              *dp = (sp[0] + sp[1] + sp[2]) / 3;
              *dp /= 2; // FBLUR_DEPTH_MAX = 127;
            }
      break;

    case GIMP_RGBA_IMAGE:
      g_assert (pr.bpp == 4);
      focal = rintf (255 * depth_map->focal_depth / 100.0f);
      for (; p; p = gimp_pixel_rgns_process (p))
        for (y = pr.h, slp = pr.data,
               dlp = depth_map->data + pr.y * w + pr.x;
             y --; slp += pr.rowstride, dlp += w)
          for (x = pr.w, sp = slp, dp = dlp; x --; sp += 4, dp ++)
            {
              *dp = ((sp[0] + sp[1] + sp[2]) / 3 * sp[3]
                     + focal * (255 - sp[3])) / 255;
              *dp /= 2; // FBLUR_DEPTH_MAX = 127;
            }
      break;

    default:
      g_assert_not_reached ();
    }

  gimp_drawable_detach (drawable);

  return TRUE;
}


void
focusblur_depth_map_destroy (FblurDepthMap      **depth_map)
{
  if (*depth_map)
    {
      if ((*depth_map)->data)
        g_free ((*depth_map)->data);

      g_free (*depth_map);
      *depth_map = NULL;
    }
}


gint
focusblur_depth_map_get_depth (FblurDepthMap *depth_map,
                               gint           x,
                               gint           y)
{
  gsize offset;
  gint  depth;

  if (x >= depth_map->width)
    x %= depth_map->width;
  else if (x < 0)
    x = depth_map->width + (x % depth_map->width);

  if (y >= depth_map->height)
    y %= depth_map->height;
  else if (y < 0)
    y = depth_map->height + (y % depth_map->height);

  offset = y * depth_map->width + x;
  depth = depth_map->data[offset];

  return depth;
}


gint
focusblur_depth_map_get_level (FblurDepthMap *depth_map,
                               gint           depth)
{
  gint  level;

  g_return_val_if_fail (depth_map != NULL, FBLUR_DEPTH_MAX);

  level = depth - depth_map->focal_depth_int;

  return level;
}


gint
focusblur_depth_map_focal_depth (FblurDepthMap  *depth_map)
{
  g_return_val_if_fail (depth_map != NULL, 0);

  return depth_map->focal_depth_int;
}
