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

#include <glib.h>
#include <math.h>
#include <stdlib.h>

#include "focusblur.h"
#include "focusblurparam.h"
#include "diffusion.h"
#ifdef HAVE_FFTW3
#  include "fftblurbuffer.h"
#endif


/*---- Types ----*/

typedef gfloat
(*FocusblurDiffusionModelFunc)  (FblurDiffusionTable    *diffusion,
                                 gfloat                  radius,
                                 gfloat                  difference);


/*---- Static values ----*/

const int        table_length = FBLUR_RADIUS_MAX + 1;
static gfloat   *difference_table = NULL;


/*---- Prototypes ----*/

static void     focusblur_diffusion_difference_table_init (void);
static void     focusblur_diffusion_make (FblurDiffusionTable   *diffusion,
                                          gint                   level,
                                          gint                   num);
static gfloat   focusblur_diffusion_make_density
                                        (FblurDiffusionTable    *diffusion,
                                         gfloat                  radius);
static void     focusblur_diffusion_blur (FblurDiffusionTable   *diffusion,
                                          gfloat                *data,
                                          gfloat                 model_radius,
                                          gint                   level);
static gfloat   focusblur_diffusion_model_flat
                                        (FblurDiffusionTable    *diffusion,
                                         gfloat                  radius,
                                         gfloat                 difference);
static gfloat   focusblur_diffusion_model_ring
                                        (FblurDiffusionTable    *diffusion,
                                         gfloat                  radius,
                                         gfloat                 difference);
static gfloat   focusblur_diffusion_model_concave
                                        (FblurDiffusionTable    *diffusion,
                                         gfloat                  radius,
                                         gfloat                 difference);

/*---- Functions ----*/

static void
focusblur_diffusion_difference_table_init (void)
{
  gfloat        *tp, *tp2;
  gint           x, y;

  if (difference_table)
    return;

  difference_table = g_new (gfloat, table_length * table_length);

  tp = difference_table;
  for (y = 0; y < table_length; y ++)
    {
      for (x = 0, tp2 = difference_table + y; x < y; x ++, tp2 += table_length)
        *tp ++ = *tp2;
      for (; x < table_length; x ++)
        *tp ++ = hypotf (x, y);
    }
}


void
focusblur_diffusion_update (FblurDiffusionTable **diffusion,
                            FblurFftBuffer       *fft,
                            FblurStoreParam      *store)
{
  gfloat radius;

  if (! difference_table)
    focusblur_diffusion_difference_table_init ();

  if (*diffusion)
    {
      if (store->model_type == (*diffusion)->model_type &&
          store->model_radius == (*diffusion)->model_radius &&
          store->model_fill == (*diffusion)->model_fill &&
          store->model_softness == (*diffusion)->model_softness &&
          store->model_softness_delay == (*diffusion)->model_softness_delay)
        {
          if (store->shine_radius != (*diffusion)->shine_radius)
            {
              (*diffusion)->shine_radius = store->shine_radius;
              (*diffusion)->density_max = focusblur_diffusion_make_density
                (*diffusion, (*diffusion)->shine_radius);
            }

          return;
        }

      focusblur_diffusion_destroy (diffusion);
    }

#ifdef HAVE_FFTW3
      if (fft)
        focusblur_fft_buffer_invalidate_diffusion (fft);
#endif

  *diffusion = g_new0 (FblurDiffusionTable, 1);

  (*diffusion)->model_type           = store->model_type;
  (*diffusion)->model_radius         = store->model_radius;
  (*diffusion)->model_fill           = store->model_fill;
  (*diffusion)->model_softness       = store->model_softness;
  (*diffusion)->model_softness_delay = store->model_softness_delay;
  (*diffusion)->shine_radius         = store->shine_radius;

  (*diffusion)->model_fill_float = (*diffusion)->model_fill / 100.0f;

  radius = (*diffusion)->model_radius;
  radius *= 1.0f + (*diffusion)->model_softness / 100.0f;
  (*diffusion)->model_radius_int = ceilf (radius);
  (*diffusion)->rowstride = 1 + (*diffusion)->model_radius_int;
  (*diffusion)->blocksize = sizeof (gfloat)
    * (*diffusion)->rowstride * (1 + (*diffusion)->model_radius_int);

  (*diffusion)->density_max = focusblur_diffusion_make_density
    (*diffusion, (*diffusion)->shine_radius);
}


void
focusblur_diffusion_destroy (FblurDiffusionTable        **diffusion)
{
  gint i;

  if (*diffusion)
    {
      for (i = 0; i < FBLUR_DIFFUSION_NTABLES; i ++)
        if ((*diffusion)->distribution[i])
          g_slice_free1 ((*diffusion)->blocksize,
                         (*diffusion)->distribution[i]);

      g_free (*diffusion);
      *diffusion = NULL;
    }
}


gfloat
focusblur_diffusion_get (FblurDiffusionTable    *diffusion,
                         gint                    level,
                         gint                    pos_x,
                         gint                    pos_y,
                         gint                    x,
                         gint                    y)
{
  gfloat        *dp;
  gint           dx, dy;
  gint           num;

  dx = pos_x - x;
  dy = pos_y - y;

  if (dx < - diffusion->model_radius_int ||
      dx > diffusion->model_radius_int ||
      dy < - diffusion->model_radius_int ||
      dy > diffusion->model_radius_int)
    return 0.0f;

  g_assert (level <= FBLUR_DEPTH_MAX);
  g_assert (level >= -FBLUR_DEPTH_MAX);

  if (! level)
    return (dx || dy) ? 0.0f : 1.0f;

  level = abs (level);

  /* Farthest distribution is allocated at index 0. */
  num = FBLUR_DEPTH_MAX - level;

  g_assert (num >= 0);
  g_assert (num < FBLUR_DIFFUSION_NTABLES);

  if (! diffusion->distribution[num])
    focusblur_diffusion_make (diffusion, level, num);

  g_assert (diffusion->distribution[num] != NULL);

  dp = diffusion->distribution[num];
  if (diffusion->centeroffset == 0)
    {
      /* data in quarto */
      dx = abs (dx);
      dy = abs (dy);
    }
  else
    {
      dp += diffusion->centeroffset;
    }
  dp += dy * diffusion->rowstride + dx;

  return *dp;
}


gfloat
focusblur_diffusion_get_shine (FblurDiffusionTable      *diffusion,
                               gint                      depth_level,
                               gint                      shine_level)
{
  const gfloat   color_fnum = 1.0f / 255.0f;
  gfloat         shine_density;
  gfloat         fval;
  gint           num;

  g_assert (depth_level <= FBLUR_DEPTH_MAX);
  g_assert (depth_level >= -FBLUR_DEPTH_MAX);

  if (! depth_level ||
      ! shine_level ||
      ! diffusion->shine_radius)
    return 1.0f;

  depth_level = abs (depth_level);

  /* Farthest distribution is allocated at index 0. */
  num = FBLUR_DEPTH_MAX - depth_level;

  g_assert (num >= 0);
  g_assert (num < FBLUR_DIFFUSION_NTABLES);

  if (! diffusion->distribution[num])
    focusblur_diffusion_make (diffusion, depth_level, num);

  shine_density = diffusion->density[num];

  if (diffusion->density_max > 1.0f &&
      shine_density > diffusion->density_max)
    shine_density = diffusion->density_max;

  shine_density -= 1.0f;

  g_return_val_if_fail (shine_density >= 0.0f, 1.0f);

  fval = 1.0f + shine_level * shine_density * color_fnum;

  return fval;
}


static void
focusblur_diffusion_make (FblurDiffusionTable   *diffusion,
                          gint                   level,
                          gint                   num)
{
  FocusblurDiffusionModelFunc func;

  gfloat        *dlp, *dp;
  gfloat        *tlp, *tp;
  gfloat         radius;
  gfloat         fval;
  gint           range;
  gint           x, y;

  g_assert (level != 0);

  radius = diffusion->model_radius;

  switch (diffusion->model_type)
    {
    case FBLUR_MODEL_FLAT:
      //level = abs (level);
      g_assert (level > 0);
      func = focusblur_diffusion_model_flat;
      break;

    case FBLUR_MODEL_RING:
      //level = abs (level);
      g_assert (level > 0);
      func = focusblur_diffusion_model_ring;
      break;

    case FBLUR_MODEL_CONCAVE:
      //level = abs (level);
      g_assert (level > 0);
      func = focusblur_diffusion_model_concave;
      break;

    default:
      g_assert_not_reached ();
      func = NULL; /* shut a warning */
    }

  radius *= (gfloat) level / (gfloat) FBLUR_DEPTH_MAX;
  range = ceilf (radius);

  g_assert (radius >= 0.0f);
  g_assert (range < table_length);

  diffusion->distribution[num] = g_slice_alloc0 (diffusion->blocksize);

  diffusion->density[num] = 0;
  dlp = diffusion->distribution[num];
  tlp = difference_table;

  if (diffusion->centeroffset == 0)
    {
      for (y = 0; y <= range; y ++, dlp += diffusion->rowstride, tlp += table_length)
        for (x = 0, dp = dlp, tp = tlp; x <= range; x ++, dp ++, tp ++)
          diffusion->density[num] += (*dp = (*func) (diffusion, radius, *tp));

      /* 4 times density */
      dp = diffusion->distribution[num];
      for (x = 0; x <= range; x ++, dp ++)
        diffusion->density[num] -= *dp;
      diffusion->density[num] *= 4;
      diffusion->density[num] += diffusion->distribution[num][0];
    }

  fval = 1.0f / diffusion->density[num];
  dlp = diffusion->distribution[num];

  /* normalize */
  if (diffusion->centeroffset == 0)
    {
      for (y = 0; y <= range; y ++, dlp += diffusion->rowstride)
        for (x = 0, dp = dlp; x <= range; x ++, dp ++)
          *dp *= fval;
    }

  //diffusion->density[num] -= 1.0f; // make shine

  focusblur_diffusion_blur
    (diffusion, diffusion->distribution[num], radius, level);
}


static gfloat
focusblur_diffusion_make_density (FblurDiffusionTable   *diffusion,
                                  gfloat                 radius)
{
  FocusblurDiffusionModelFunc func;

  gfloat        *tlp, *tp;
  gfloat         density;
  gint           range;
  gint           x, y;

  if (! radius)
    return 1.0f;

  if (radius < 0.0f)
    return 0.0f;

  switch (diffusion->model_type)
    {
    case FBLUR_MODEL_FLAT:
      func = focusblur_diffusion_model_flat;
      break;

    case FBLUR_MODEL_RING:
      func = focusblur_diffusion_model_ring;
      break;

    case FBLUR_MODEL_CONCAVE:
      func = focusblur_diffusion_model_concave;
      break;

    default:
      g_assert_not_reached ();
      func = NULL; /* shut a warning */
    }

  range = ceilf (radius);

  g_assert (radius >= 0.0f);
  g_assert (range < table_length);

  density = 0.0f;
  tlp = difference_table;

  if (diffusion->centeroffset == 0)
    {
      /* skip first line */
      tlp += table_length;

      for (y = 1; y <= range; y ++, tlp += table_length)
        for (x = 0, tp = tlp; x <= range; x ++, tp ++)
          density += (*func) (diffusion, radius, *tp);

      /* 4 times (like a pinwheel) */
      density *= 4;
      density += (*func) (diffusion, radius, 0.0f);
    }

  //density -= 1.0f; // make shine

  return density;
}


static void
focusblur_diffusion_blur (FblurDiffusionTable   *diffusion,
                          gfloat                *data,
                          gfloat                 model_radius,
                          gint                   level)
{
  const gint     width = 1 + 2 * FBLUR_RADIUS_MAX;
  gfloat         blur_table[width];
  gfloat        *tp = &(blur_table[FBLUR_RADIUS_MAX]);
  gfloat        *dlp, *dp, *sp;

  gfloat         blur_radius;
  gint           blur_radius_int;
  gint           range;
  gfloat         vr, fval, sval;
  gint           x, y, i, j;

  gfloat         softness, delay;
  gint           delay_start, level_low;

  if (! diffusion->model_softness)
    return;

  delay = 2.0f * diffusion->model_softness_delay / 100.0f;
  if (delay <= 1.0f)
    {
      delay_start = (1.0f - delay);
      level_low = 0 - 1;
    }
  else
    {
      delay_start = 0.0f;
      level_low = rintf (FBLUR_DEPTH_MAX * (delay - 1.0f)) - 1;
    }
  if (level < level_low)
    return;
  softness = diffusion->model_softness / 100.0f;
  softness *= delay_start +
    (1.0f - delay_start) * (level - level_low) / (FBLUR_DEPTH_MAX - level_low);


  range = diffusion->model_radius_int;
  blur_radius = model_radius * softness;
  blur_radius_int = ceilf (blur_radius);

  vr = 0.3003866304f * (blur_radius + 1);
  vr = -1.0f / (2.0f * SQR (vr));
  sval = 0.0f;
  for (i = - blur_radius_int; i <= blur_radius_int; i ++)
    {
      fval = expf (SQR (i) * vr);
      tp[i] = fval;
      sval += fval;
    }

  if (diffusion->centeroffset == 0)
    {
      gfloat     sum[1 + range];

      for (y = 0, dlp = data; y <= range; y ++, dlp += diffusion->rowstride)
        {
          for (x = 0, sp = sum; x <= range; x ++, sp ++)
            {
              fval = 0.0f;
              for (i = - blur_radius_int; i <= blur_radius_int; i ++)
                {
                  j = abs (x + i);
                  if (j <= range)
                    fval += tp[i] * dlp[j];
                }
              *sp = fval / sval;
            }
          for (x = 0, dp = dlp, sp = sum; y <= range; y ++, dp ++, sp ++)
            *dp = *sp;
        }

      for (x = 0, dlp = data; x <= range; x ++, dlp ++)
        {
          for (y = 0, sp = sum; y <= range; y ++, sp ++)
            {
              fval = 0.0f;
              for (i = - blur_radius_int; i <= blur_radius_int; i ++)
                {
                  j = abs (y + i);
                  if (j <= range)
                    fval += tp[i] * dlp[j * diffusion->rowstride];
                }
              *sp = fval / sval;
            }
          for (y = 0, dp = dlp, sp = sum; y <= range;
               y ++, dp += diffusion->rowstride, sp ++)
            *dp = *sp;
        }
    }
  else
    {
      // not implemented
    }
}


static gfloat
focusblur_diffusion_model_flat (FblurDiffusionTable     *diffusion,
                                gfloat                   radius,
                                gfloat                   difference)
{
  gfloat        distribution;

  distribution = 1.0f + radius - difference;

  if (distribution <= 0.0f)
    distribution = 0.0f;

  else if (distribution >= 1.0f)
    distribution = 1.0f;

  return distribution;
}


static gfloat
focusblur_diffusion_model_ring (FblurDiffusionTable     *diffusion,
                                gfloat                   radius,
                                gfloat                   difference)
{
  gfloat        distribution;

  distribution = 1.0f + radius - difference;

  if (distribution <= 0.0f)
    distribution = 0.0f;

  else if (distribution >= 2.0f)
    distribution = diffusion->model_fill_float;

  else if (distribution > 1.0f)
    {
      distribution = 2.0f - distribution;

      distribution *= 1.0f - diffusion->model_fill_float;
      distribution += diffusion->model_fill_float;
    }

  return distribution;
}


static gfloat
focusblur_diffusion_model_concave (FblurDiffusionTable  *diffusion,
                                   gfloat                radius,
                                   gfloat                difference)
{
  gfloat        distribution;

  distribution = 1.0f + radius - difference;

  if (distribution <= 0.0f)
    distribution = 0.0f;

  else if (distribution > 1.0f)
    {
      distribution = (difference + 0.5f) / (radius + 0.5f);
      distribution *= distribution;

      distribution *= 1.0f - diffusion->model_fill_float;
      distribution += diffusion->model_fill_float;
    }

  return distribution;
}
