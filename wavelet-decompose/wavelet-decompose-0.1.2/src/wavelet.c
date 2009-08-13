/* 
 * Wavelet decompose GIMP plugin
 * 
 * wavelet.c
 * Copyright 2008 by Marco Rossini
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2+
 * as published by the Free Software Foundation.
 */

#include "plugin.h"

/* into plugin.h */
//typedef signed short wdsint;

/* code copied from UFRaw (which originates from dcraw) */
static void
hat_transform (wdsint * temp, wdsint * base, int st, int size, int sc)
{
  int i;
  for (i = 0; i < sc; i++)
    temp[i] = 2 * base[st * i] + base[st * (sc - i)] + base[st * (i + sc)];
  for (; i + sc < size; i++)
    temp[i] = 2 * base[st * i] + base[st * (i - sc)] + base[st * (i + sc)];
  for (; i < size; i++)
    temp[i] = 2 * base[st * i] + base[st * (i - sc)]
      + base[st * (2 * size - 2 - (i + sc))];
}

/* actual decomposing algorithm */
void
wavelet_decompose (guint32 image, guint32 layer, wdsint ** img,
		   unsigned int width, unsigned int height,
		   unsigned int channels, unsigned int scales)
{
  wdsint *temp;
  unsigned int i, c, lev, lpass, hpass, size, col, row;
  wdsint *buffer[2][3];
  char str[256];
  unsigned int clip = 0;

  size = width * height;

  for (c = 0; c < channels; c++)
    {
      /* image buffers */
      buffer[0][c] = img[c];
      /* temporary storage */
      buffer[1][c] = malloc (size * sizeof (wdsint));
    }

  /* FIXME: replace by GIMP functions */
  temp = malloc (MAX2 (width, height) * sizeof (wdsint));

  /* iterate over wavelet scales */
  lpass = 1;
  hpass = 0;
  for (lev = 0; lev < scales; lev++)
    {
      lpass = (1 - (lev & 1));

      /* iterate over channels */
      for (c = 0; c < channels; c++)
	{
	  for (row = 0; row < height; row++)
	    {
	      hat_transform (temp, buffer[hpass][c] + row * width, 1, width,
			     1 << lev);
	      for (col = 0; col < width; col++)
		{
		  buffer[lpass][c][row * width + col] = temp[col];
		}
	    }
	  for (col = 0; col < width; col++)
	    {
	      hat_transform (temp, buffer[lpass][c] + col, width, height,
			     1 << lev);
	      for (row = 0; row < height; row++)
		{
		  buffer[lpass][c][row * width + col] = temp[row];
		}
	    }
	  for (i = 0; i < size; i++)
	    {
	      /* rounding errors introduced here (division by 16) */
	      buffer[lpass][c][i] = (buffer[lpass][c][i] + 8) / 16;
	      buffer[hpass][c][i] -= buffer[lpass][c][i] - 128;
	      if (buffer[hpass][c][i] > 255)
		{
		  buffer[hpass][c][i] = 255;
		  clip++;
		}
	      else if (buffer[hpass][c][i] < 0)
		{
		  buffer[hpass][c][i] = 0;
		  clip++;
		}
	    }
	}

      /* TRANSLATORS: This is a technical term which must be correct. It must
         not be longer than 255 bytes including the integer number. */
      sprintf (str, _("Wavelet scale %i"), lev + 1);
      add_layer (image, layer, buffer[hpass], str, settings.layer_modes);
      hpass = lpass;
      gimp_progress_update ((lev + 2) / (double) (scales + 1));
    }

  /* TRANSLATORS: This is a technical term which must be correct */
  add_layer (image, layer, buffer[lpass], _("Wavelet residual"),
	     GIMP_NORMAL_MODE);

  if (clip)
    {
      /* TRANSLATORS: This must not be longer than 255 bytes including the
         integer number. */
      sprintf (str,
	       _
	       ("Warning: Some pixels were clipped! Number of ccurences: %i"),
	       clip);
      gimp_message (str);
    }

  /* FIXME: replace by GIMP functions */
  free (temp);
  for (c = 0; c < channels; c++)
    {
      free (buffer[1][c]);
    }
}
