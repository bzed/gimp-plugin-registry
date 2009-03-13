/* 
 * Wavelet denoise GIMP plugin
 * 
 * denoise.c
 * Copyright 2008 by Marco Rossini
 * 
 * Implements the wavelet denoise code of UFRaw by Udi Fuchs
 * which itself bases on the code by Dave Coffin
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 * 
 */

#include "plugin.h"

void
denoise (GimpDrawable * drawable, GimpPreview * preview)
{
  GimpPixelRgn rgn_in, rgn_out;
  gint i, x1, y1, x2, y2, width, height, x, c;
  guchar *line;
  float times[3], totaltime;
  int channels_denoised;

  if (preview)
    {
      gimp_preview_get_position (preview, &x1, &y1);
      gimp_preview_get_size (preview, &width, &height);
      x2 = x1 + width;
      y2 = y1 + height;
    }
  else
    {
      gimp_drawable_mask_bounds (drawable->drawable_id, &x1, &y1, &x2, &y2);
      width = x2 - x1;
      height = y2 - y1;
    }

  gimp_pixel_rgn_init (&rgn_in, drawable, x1, y1, width, height, FALSE,
		       FALSE);
  gimp_pixel_rgn_init (&rgn_out, drawable, x1, y1, width, height,
		       preview == NULL, TRUE);

  /* cache some tiles to make reading/writing faster */
  gimp_tile_cache_ntiles (drawable->width / gimp_tile_width () + 1);

  totaltime = settings.times[0];
  totaltime += settings.times[2];
  for (i = 0; i < channels; i++)
    {
      if (channels > 2 && settings.colour_thresholds[i] > 0)
	totaltime += settings.times[1];
      else if (channels < 3 && settings.gray_thresholds[i] > 0)
	totaltime += settings.times[1];
    }

  /* FIXME: replace by GIMP functions */
  line = (guchar *) malloc (channels * width * sizeof (guchar));

  /* read the full image from GIMP */
  if (!preview)
    /* TRANSLATORS: This is the message displayed while denoising is in
       progress */
    gimp_progress_init (_("Wavelet denoising..."));
  times[0] = g_timer_elapsed (timer, NULL);
  for (i = 0; i < y2 - y1; i++)
    {
      if (!preview && i % 10 == 0)
	gimp_progress_update (settings.times[0] * i
			      / (double) height / totaltime);
      gimp_pixel_rgn_get_row (&rgn_in, line, x1, i + y1, width);

      /* convert pixel values to float [0,1] */
      for (c = 0; c < channels; c++)
	{
	  for (x = 0; x < width; x++)
	    fimg[c][i * width + x] = line[x * channels + c] / 255.0;
	}
    }
  times[0] = g_timer_elapsed (timer, NULL) - times[0];

  /* do colour model conversion sRGB[0,1] -> whatever */
  if (channels > 2) {
    if (settings.colour_mode == MODE_YCBCR) {
      srgb2ycbcr(fimg, width * height);
    } else if (settings.colour_mode == MODE_LAB) {
      srgb2lab(fimg, width * height);
    } else if (settings.colour_mode == MODE_RGB) {
      srgb2rgb(fimg, width * height);
    }
  }

  /* denoise the channels individually */
  times[1] = g_timer_elapsed (timer, NULL);
  /* FIXME: variable abuse (x) */
  channels_denoised = 0;
  for (c = 0; c < channels; c++)
    {
      double a, b;
      /* in preview mode only process the displayed channel */
      if (preview && settings.preview_mode > 0 &&
	  settings.preview_channel != c)
	continue;
      buffer[0] = fimg[c];
      b = preview ? 0.0 : settings.times[1] / totaltime;
      a = settings.times[0] + channels_denoised * settings.times[1];
      a /= totaltime;
      if (channels > 2 && settings.colour_thresholds[c] > 0)
	{
	  wavelet_denoise (buffer, width, height, (float)
			   settings.colour_thresholds[c],
			   settings.colour_low[c], a, b);
	  channels_denoised++;
	}
      if (channels < 3 && settings.gray_thresholds[c] > 0)
	{
	  wavelet_denoise (buffer, width, height, (float)
			   settings.gray_thresholds[c], settings.gray_low[c],
			   a, b);
	  channels_denoised++;
	}
    }
  times[1] = g_timer_elapsed (timer, NULL) - times[1];
  times[1] /= channels_denoised;

  /* retransform the image data */
  if (channels > 2) {
    int pc = 0;
    if (preview && settings.preview_mode == 1)
      pc = settings.preview_channel + 1;
    else if (preview && settings.preview_mode == 2)
      pc = settings.preview_channel + 4;

    if (settings.colour_mode == MODE_YCBCR) {
      ycbcr2srgb(fimg, width * height, pc);
    } else if (settings.colour_mode == MODE_LAB) {
      lab2srgb(fimg, width * height, pc);
    } else if (settings.colour_mode == MODE_RGB) {
      rgb2srgb(fimg, width * height, pc);
    }
  }

  /* if alpha channel preview */
  if (preview && channels % 2 == 0 && settings.preview_channel == channels - 1 && settings.preview_mode != 0)
    for (c = 0; c < channels - 1; c++) {
      for (i = 0; i < width * height; i++) {
        fimg[c][i] = fimg[channels - 1][i];
      }
    }

  /* set alpha to full opacity in preview mode */
  if (preview && channels % 2 == 0 && !(settings.preview_channel == channels - 1 && settings.preview_mode == 0))
    for (i = 0; i < width * height; i++)
      fimg[channels - 1][i] = 1.0;

  /* clip the values */
  for (c = 0; c < channels; c++)
    {
      for (i = 0; i < width * height; i++)
	{
	  fimg[c][i] = CLIP (fimg[c][i] * 255.0, 0, 255);
	}
    }

  /* write the image back to GIMP */
  times[2] = g_timer_elapsed (timer, NULL);
  for (i = 0; i < height; i++)
    {
      if (!preview)
	gimp_progress_update ((settings.times[0] + channels_denoised
			       * settings.times[1] + settings.times[2]
			       * i / (double) height) / totaltime);

      /* scale and convert back to guchar */
      for (c = 0; c < channels; c++)
	{
	  for (x = 0; x < width; x++)
	    {
	      /* avoiding rounding errors !!! */
	      line[x * channels + c] =
		(guchar) (fimg[c][i * width + x] + 0.5);
	    }
	}
      gimp_pixel_rgn_set_row (&rgn_out, line, x1, i + y1, width);
    }
  times[2] = g_timer_elapsed (timer, NULL) - times[2];

  if (!preview)
    {
      settings.times[0] = times[0];
      if (channels_denoised > 0)
	settings.times[1] = times[1];
      settings.times[2] = times[2];
      g_print("%f, %f, %f\n", times[0], times[1], times[2]);
    }

  /* FIXME: replace by gimp functions */
  free (line);

  if (preview)
    {
      gimp_drawable_preview_draw_region (GIMP_DRAWABLE_PREVIEW (preview),
					 &rgn_out);
      return;
    }
  gimp_drawable_flush (drawable);
  gimp_drawable_merge_shadow (drawable->drawable_id, TRUE);
  gimp_drawable_update (drawable->drawable_id, x1, y1, width, height);
}
