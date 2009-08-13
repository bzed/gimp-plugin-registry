/* 
 * Wavelet decompose GIMP plugin
 * 
 * decompose.c
 * Copyright 2008 by Marco Rossini
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2+
 * as published by the Free Software Foundation.
 */

#include "plugin.h"

void
add_layer (gint32 image, gint32 parent, wdsint ** img, const char *name,
	   GimpLayerModeEffects mode)
{
  gint offx, offy;
  gint32 layer;
  unsigned int x, y, width, height, channels, c;
  GimpPixelRgn rgn;
  GimpDrawable *drawable;
  guchar *line;

  width = gimp_drawable_width (parent);
  height = gimp_drawable_height (parent);
  channels = gimp_drawable_bpp (parent);

  if (settings.add_alpha)
    layer = gimp_layer_new (image, name, width, height,
			    gimp_drawable_type_with_alpha (parent), 100.0,
			    mode);
  else
    layer = gimp_layer_new (image, name, width, height,
			    gimp_drawable_type (parent), 100.0, mode);
  drawable = gimp_drawable_get (layer);
  gimp_image_add_layer (image, layer, -1);

  gimp_pixel_rgn_init (&rgn, drawable, 0, 0, width, height, TRUE, FALSE);

  if (settings.add_alpha && channels % 2 != 0)
    line = malloc (width * height * (channels + 1) * sizeof (guchar));
  else
    line = malloc (width * height * channels * sizeof (guchar));
  for (y = 0; y < height; y++)
    {
      if (settings.add_alpha && channels % 2 != 0)
	{
	  /* no alpha channel, but add one */
	  for (x = 0; x < width; x++)
	    {
	      for (c = 0; c < channels; c++)
		{
		  line[x * (channels + 1) + c] = img[c][y * width + x];
		}
	      line[x * (channels + 1) + channels] = 255;
	    }
	}
      else if (channels % 2 != 0)
	{
	  /* no alpha channel */
	  for (x = 0; x < width; x++)
	    {
	      for (c = 0; c < channels; c++)
		{
		  line[x * channels + c] = img[c][y * width + x];
		}
	    }
	}
      else
	{
	  for (x = 0; x < width; x++)
	    {
	      for (c = 0; c < channels - 1; c++)
		{
		  line[x * channels + c] = img[c][y * width + x];
		}
	      line[x * channels + channels - 1] = 255;
	    }
	}
      gimp_pixel_rgn_set_row (&rgn, line, 0, y, width);
    }
  free (line);

  gimp_drawable_offsets (parent, &offx, &offy);
  gimp_layer_translate (layer, offx, offy);
  gimp_image_set_active_layer (image, parent);
  gimp_drawable_flush (drawable);
  gimp_drawable_update (layer, 0, 0, width, height);
}

void
decompose (gint32 image, GimpDrawable * drawable, unsigned int scales)
{
  /* this function prepares for decomposing, which is done in the
     function wavelet_decompose() */

  int i, c, x, width, height, channels;
  GimpPixelRgn rgn_in;
  wdsint *img[3];
  guchar *line;
  gint32 layer;

  width = gimp_drawable_width (drawable->drawable_id);
  height = gimp_drawable_height (drawable->drawable_id);
  channels = gimp_drawable_bpp (drawable->drawable_id);

  /* pixel access */
  gimp_pixel_rgn_init (&rgn_in, drawable, 0, 0, width, height, FALSE, FALSE);
  gimp_tile_cache_ntiles (drawable->width / gimp_tile_width () + 1);

  for (c = 0; c < (channels % 2 ? channels : channels - 1); c++)
    {
      img[c] = malloc (width * height * sizeof (wdsint));
    }

  gimp_progress_init (_("Wavelet decompose..."));

  if (settings.new_image)
    {
      image = gimp_image_new (width, height, gimp_image_base_type (image));
      gimp_display_new (image);
      layer = gimp_layer_new_from_drawable (drawable->drawable_id, image);
      gimp_drawable_set_name (layer, _("Original"));
      gimp_image_add_layer (image, layer, 0);
      gimp_layer_set_offsets (layer, 0, 0);
      drawable = gimp_drawable_get (layer);
      gimp_image_undo_disable (image);
    }
  else
    {
      gimp_image_undo_group_start (image);
    }

  /* read the image into memory */
  line = (guchar *) malloc (channels * width * sizeof (guchar));
  for (i = 0; i < height; i++)
    {
      gimp_pixel_rgn_get_row (&rgn_in, line, 0, i, width);

      for (x = 0; x < width; x++)
	{
	  for (c = 0; c < (channels % 2 ? channels : channels - 1); c++)
	    {
	      img[c][i * width + x] = line[x * channels + c];
	    }
	}
    }

  gimp_progress_update (1.0 / (double) (scales + 1));

  wavelet_decompose (image, drawable->drawable_id, img, width, height,
		     channels % 2 ? channels : channels - 1, scales);

  if (settings.new_image)
    {
      gimp_image_undo_enable (image);
    }
  else
    {
      gimp_image_undo_group_end (image);
    }

  for (c = 0; c < (channels % 2 ? channels : channels - 1); c++)
    {
      free (img[c]);
    }
}
