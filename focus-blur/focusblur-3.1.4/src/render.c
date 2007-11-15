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

#include "render.h"
#include "focusblurparam.h"

#include "source.h"
#include "diffusion.h"
#include "depthmap.h"
#include "shine.h"


/*---- main function ----*/
void
focusblur_render_pixel (gint             pos_x,
                        gint             pos_y,
                        guchar          *dest_pixel,
                        gint             dest_bpp,
                        FblurParam      *param)
{
  const gfloat   color_fnum = 1.0 / 255.0;
  const gint     depth_ntables = FBLUR_DEPTH_MAX + 1;
  guchar         src_pixel[4];
  gfloat         distribution, val_shone, val_alpha;
  gfloat         sum_alpha, sum_pixel, sum_color[3];
  gfloat         tsum_alpha[depth_ntables], tsum_pixel[depth_ntables];
  gfloat         transit[depth_ntables], tsum_color[depth_ntables][3];
  gfloat         through;
  gboolean       enable_depth_precedence, enable_depth_fill_behind;
  gboolean       enable_shine;
  gint           shine;
  gint           depth, level;
  gint           x1, x2, y1, y2;
  gint           x, y, c, d;

  sum_alpha = 0.0f;
  sum_pixel = 0.0f;
  sum_color[0] = sum_color[1] = sum_color[2] = 0.0f;

  enable_depth_precedence = param->store.enable_depth_map &&
    param->store.enable_depth_precedence;
  enable_depth_fill_behind = param->store.enable_depth_map &&
    enable_depth_precedence && // it works without precedence
    param->store.enable_depth_fill_behind;

  if (enable_depth_precedence)
    {
      for (d = 0; d < depth_ntables; d ++)
        tsum_alpha[d] = 0.0f;
      for (d = 0; d < depth_ntables; d ++)
        tsum_pixel[d] = 0.0f;
      for (d = 0; d < depth_ntables; d ++)
        transit[d] = 0.0f;
      for (d = 0; d < depth_ntables; d ++)
        tsum_color[d][0] = tsum_color[d][1] = tsum_color[d][2] = 0.0f;
    }

  /* Sets farthest distance when depth map is not specified */
  depth = 0;
  level = FBLUR_DEPTH_MAX;
  enable_shine = focusblur_shine_check_enabled (&(param->store));

  x1 = pos_x - param->diffusion->model_radius_int;
  x2 = pos_x + param->diffusion->model_radius_int + 1;
  y1 = pos_y - param->diffusion->model_radius_int;
  y2 = pos_y + param->diffusion->model_radius_int + 1;

  if (x1 < param->source->x1)
    x1 = param->source->x1;
  if (x2 > param->source->x2)
    x2 = param->source->x2;
  if (y1 < param->source->y1)
    y1 = param->source->y1;
  if (y2 > param->source->y2)
    y2 = param->source->y2;

  for (y = y1; y < y2; y ++)
    for (x = x1; x < x2; x ++)
      {
        if (param->store.enable_depth_map)
          {
            depth = focusblur_depth_map_get_depth (param->depth_map, x, y);
            level = focusblur_depth_map_get_level (param->depth_map, depth);
          }

        distribution = focusblur_diffusion_get
          (param->diffusion, level, pos_x, pos_y, x, y);

        if (distribution <= 0.0f)
          continue;

        focusblur_source_get (param->source, x, y, src_pixel);

        val_shone = distribution;
        if (enable_shine)
          {
            shine = focusblur_shine_get (param->shine, x, y);
            if (shine)
              {
                val_shone *= focusblur_diffusion_get_shine
                  (param->diffusion, level, shine);
              }
          }

        val_alpha = val_shone;
        if (param->source->has_alpha)
          {
            gfloat aval = color_fnum * src_pixel[param->source->channels]; 
            val_alpha *= aval;
            distribution *= aval; // for precedence
          }

        if (! enable_depth_precedence)
          {
            for (c = 0; c < param->source->channels; c ++)
              sum_color[c] += val_alpha * src_pixel[c];
            sum_alpha += val_alpha;
            sum_pixel += val_shone;
          }
        else
          {
            for (c = 0; c < param->source->channels; c ++)
              tsum_color[depth][c] += val_alpha * src_pixel[c];
            tsum_alpha[depth] += val_alpha;
            tsum_pixel[depth] += val_shone;
            transit[depth] += distribution;
          }
      }

  if (enable_depth_precedence)
    {
      gdouble aval, fval;
      through = 1.0f;
      for (d = 0; d < depth_ntables; d ++)
        {
          if (transit[d] > 0.0f)
            {
              aval = transit[d];
              fval = MIN (aval, through);
              through -= fval;
              fval /= aval;
              sum_alpha += fval * tsum_alpha[d];
              sum_pixel += fval * tsum_pixel[d];
              for (c = 0; c < param->source->channels; c ++)
                sum_color[c] += fval * tsum_color[d][c];

              if (through < color_fnum)
                break;
            }
        }
    }

  if (enable_depth_fill_behind &&
      sum_pixel < 1.0f)
    {
      gint      depth_pos;
      gint      depth_nearest;
      gfloat    depth_fuzzy;
      gfloat    bsum_alpha, bsum_pixel, bsum_color[3];

      bsum_alpha = 0.0f;
      bsum_pixel = 0.0f;
      bsum_color[0] = bsum_color[1] = bsum_color[2] = 0.0f;

      depth_pos = focusblur_depth_map_get_depth
        (param->depth_map, pos_x, pos_y);
      level = focusblur_depth_map_get_level (param->depth_map, depth_pos);

      depth_fuzzy = 0.0f;
      depth_nearest = 0;
      if (enable_depth_precedence &&
          param->store.enable_depth_fuzzy)
        {
          depth_nearest = depth_pos;
          for (y = y1; y < y2; y ++)
            for (x = x1; x < x2; x ++)
              {
                distribution = focusblur_diffusion_get
                  (param->diffusion, level, pos_x, pos_y, x, y);

                if (distribution <= 0.0f)
                  continue;

                depth = focusblur_depth_map_get_depth (param->depth_map, x, y);
                if (depth < depth_nearest)
                  depth_nearest = depth;
              }
          if (depth_pos > depth_nearest)
            depth_fuzzy = 1.0f / (depth_pos - depth_nearest);
        }

      for (y = y1; y < y2; y ++)
        for (x = x1; x < x2; x ++)
          {
            distribution = focusblur_diffusion_get
              (param->diffusion, level, pos_x, pos_y, x, y);
            if (distribution <= 0.0f)
              continue;

            if (enable_depth_precedence)
              {
                depth = focusblur_depth_map_get_depth (param->depth_map, x, y);
                if (depth <= depth_pos)
                  {
                    if (depth_fuzzy)
                      distribution *= depth_fuzzy * (depth - depth_nearest);
                    else
                      continue;
                  }
              }

            focusblur_source_get (param->source, x, y, src_pixel);

            val_alpha = distribution;
            if (param->source->has_alpha)
              val_alpha *= color_fnum * src_pixel[param->source->channels];

            for (c = 0; c < param->source->channels; c ++)
              bsum_color[c] += val_alpha * src_pixel[c];
            bsum_alpha += val_alpha;
            bsum_pixel += distribution;
          }

      if (bsum_pixel > color_fnum)
        {
          through = 1.0f - sum_pixel;
          through /= bsum_pixel;
          sum_alpha += through * bsum_alpha;
          sum_pixel += through * bsum_pixel;
          for (c = 0; c < param->source->channels; c ++)
            sum_color[c] += through * bsum_color[c];
        }
    }

  if (! sum_pixel ||
      sum_alpha / sum_pixel < color_fnum)
    {
      for (c = 0; c < param->drawable->bpp; c ++)
        dest_pixel[c] = 0;
    }
  else
    {
      gfloat val;
      gint   col;
      for (c = 0; c < param->source->channels; c ++)
        {
          val = sum_color[c] / sum_alpha;
          col = rintf (val);
          dest_pixel[c] = CLAMP0255 (col);
        }
      if (param->source->has_alpha)
        {
          val = 255 * sum_alpha / sum_pixel;
          col = rintf (val);
          dest_pixel[param->source->channels] = CLAMP0255 (col);
        }
    }
}

