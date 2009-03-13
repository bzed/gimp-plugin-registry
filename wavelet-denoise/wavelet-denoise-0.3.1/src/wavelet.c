/* 
 * Wavelet denoise GIMP plugin
 * 
 * wavelet.c
 * Copyright 2008 by Marco Rossini
 * 
 * Implements the wavelet denoise code of UFRaw by Udi Fuchs
 * which itself bases on the code by Dave Coffin
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 * 
 * Instructions:
 * compile with gimptool, eg. 'gimptool-2.0 --install wavelet-denoise.c'
 */

#include "plugin.h"

/* code copied from UFRaw (which originates from dcraw) */
static void
hat_transform (float *temp, float *base, int st, int size, int sc)
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

/* actual denoising algorithm. code copied from UFRaw (originates from dcraw) */
void
wavelet_denoise (float *fimg[3], unsigned int width,
		 unsigned int height, float threshold, double low, float a,
		 float b)
{
  float *temp, thold;
  unsigned int i, lev, lpass, hpass, size, col, row;
  double stdev[5];
  unsigned int samples[5];

  size = width * height;

  /* FIXME: replace by GIMP functions */
  temp = (float *) malloc (MAX2 (width, height) * sizeof (float));

  hpass = 0;
  for (lev = 0; lev < 5; lev++)
    {
      if (b != 0)
	gimp_progress_update (a + b * lev / 5.0);
      lpass = ((lev & 1) + 1);
      for (row = 0; row < height; row++)
	{
	  hat_transform (temp, fimg[hpass] + row * width, 1, width, 1 << lev);
	  for (col = 0; col < width; col++)
	    {
	      fimg[lpass][row * width + col] = temp[col] * 0.25;
	    }
	}
      if (b != 0)
	gimp_progress_update (a + b * (lev + 0.25) / 5.0);
      for (col = 0; col < width; col++)
	{
	  hat_transform (temp, fimg[lpass] + col, width, height, 1 << lev);
	  for (row = 0; row < height; row++)
	    {
	      fimg[lpass][row * width + col] = temp[row] * 0.25;
	    }
	}
      if (b != 0)
	gimp_progress_update (a + b * (lev + 0.5) / 5.0);

      thold =
	5.0 / (1 << 6) * exp (-2.6 * sqrt (lev + 1)) * 0.8002 / exp (-2.6);

      /* initialize stdev values for all intensities */
      stdev[0] = stdev[1] = stdev[2] = stdev[3] = stdev[4] = 0.0;
      samples[0] = samples[1] = samples[2] = samples[3] = samples[4] = 0;

      /* calculate stdevs for all intensities */
      for (i = 0; i < size; i++)
	{
	  fimg[hpass][i] -= fimg[lpass][i];
	  if (fimg[hpass][i] < thold && fimg[hpass][i] > -thold)
	    {
	      if (fimg[lpass][i] > 0.8) {
	        stdev[4] += fimg[hpass][i] * fimg[hpass][i];
	        samples[4]++;
	      } else if (fimg[lpass][i] > 0.6) {
	        stdev[3] += fimg[hpass][i] * fimg[hpass][i];
	        samples[3]++;
	      }	else if (fimg[lpass][i] > 0.4) {
	        stdev[2] += fimg[hpass][i] * fimg[hpass][i];
	        samples[2]++;
	      }	else if (fimg[lpass][i] > 0.2) {
	        stdev[1] += fimg[hpass][i] * fimg[hpass][i];
	        samples[1]++;
	      } else {
	        stdev[0] += fimg[hpass][i] * fimg[hpass][i];
	        samples[0]++;
	      }
	    }
	}
      stdev[0] = sqrt (stdev[0] / (samples[0] + 1));
      stdev[1] = sqrt (stdev[1] / (samples[1] + 1));
      stdev[2] = sqrt (stdev[2] / (samples[2] + 1));
      stdev[3] = sqrt (stdev[3] / (samples[3] + 1));
      stdev[4] = sqrt (stdev[4] / (samples[4] + 1));

      if (b != 0)
	gimp_progress_update (a + b * (lev + 0.75) / 5.0);

      /* do thresholding */
      for (i = 0; i < size; i++)
	{
	  if (fimg[lpass][i] > 0.8) {
	    thold = threshold * stdev[4];
	  } else if (fimg[lpass][i] > 0.6) {
	    thold = threshold * stdev[3];
	  } else if (fimg[lpass][i] > 0.4) {
	    thold = threshold * stdev[2];
	  } else if (fimg[lpass][i] > 0.2) {
	    thold = threshold * stdev[1];
	  } else {
	    thold = threshold * stdev[0];
	  }

	  if (fimg[hpass][i] < -thold)
	    fimg[hpass][i] += thold - thold * low;
	  else if (fimg[hpass][i] > thold)
	    fimg[hpass][i] -= thold - thold * low;
	  else
	    fimg[hpass][i] *= low;

	  if (hpass)
	    fimg[0][i] += fimg[hpass][i];
	}
      hpass = lpass;
    }

  for (i = 0; i < size; i++)
    fimg[0][i] = fimg[0][i] + fimg[lpass][i];

  /* FIXME: replace by GIMP functions */
  free (temp);
}
