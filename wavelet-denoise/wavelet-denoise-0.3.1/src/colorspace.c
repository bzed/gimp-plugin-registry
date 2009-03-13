/* 
 * Wavelet denoise GIMP plugin
 * 
 * colorspace.c
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
srgb2ycbcr (float ** fimg, int size)
{
  /* using JPEG conversion here - expecting all channels to be
   * in [0:255] range */
  int i;
  float y, cb, cr;

  for (i = 0; i < size; i++) {
    y =   0.2990 * fimg[0][i] + 0.5870 * fimg[1][i] + 0.1140 * fimg[2][i];
    cb = -0.1687 * fimg[0][i] - 0.3313 * fimg[1][i] + 0.5000 * fimg[2][i]
         + 0.5;
    cr =  0.5000 * fimg[0][i] - 0.4187 * fimg[1][i] - 0.0813 * fimg[2][i]
         + 0.5;
    fimg[0][i] = y;
    fimg[1][i] = cb;
    fimg[2][i] = cr;
  }
}

void
ycbcr2srgb (float **fimg, int size, int pc)
{
  /* using JPEG conversion here - expecting all channels to be
   * in [0:255] range */
  int i;
  static float r, g, b;

  if (pc > 3) { /* single channel, colour */
    pc -= 4;
    for (i = 0; i < size; i++) {
      fimg[(pc + 1) % 3][i] = 0.5;
      fimg[(pc + 2) % 3][i] = 0.5;
    }
  } else if (pc > 0) { /* single channel, gray */
    pc -= 1;
    for (i = 0; i < size; i++) {
      fimg[(pc + 1) % 3][i] = fimg[pc][i];
      fimg[(pc + 2) % 3][i] = fimg[pc][i];
    }
    return;
  }

  for (i = 0; i < size; i++) {
    r = fimg[0][i] + 1.40200 * (fimg[2][i] - 0.5);
    g = fimg[0][i] - 0.34414 * (fimg[1][i] - 0.5)
        - 0.71414 * (fimg[2][i] - 0.5);
    b = fimg[0][i] + 1.77200 * (fimg[1][i] - 0.5);
    fimg[0][i] = r;
    fimg[1][i] = g;
    fimg[2][i] = b;
  }
}

void
srgb2xyz (float **fimg, int size)
{
  /* fimg in [0:1], sRGB */
  int i;
  float x, y, z;

  for (i = 0; i < size; i++) {
    /* scaling and gamma correction (approximate) */
    fimg[0][i] = pow(fimg[0][i], 2.2);
    fimg[1][i] = pow(fimg[1][i], 2.2);
    fimg[2][i] = pow(fimg[2][i], 2.2);
 

    /* matrix RGB -> XYZ, with D65 reference white (www.brucelindbloom.com) */
    x = 0.412424 * fimg[0][i] + 0.357579 * fimg[1][i] + 0.180464 * fimg[2][i];
    y = 0.212656 * fimg[0][i] + 0.715158 * fimg[1][i] + 0.0721856 * fimg[2][i];
    z = 0.0193324 * fimg[0][i] + 0.119193 * fimg[1][i] + 0.950444 * fimg[2][i];

    /*
    x = 0.412424 * fimg[0][i] + 0.212656 * fimg[1][i] + 0.0193324 * fimg[2][i];
    y = 0.357579 * fimg[0][i] + 0.715158 * fimg[1][i] + 0.119193  * fimg[2][i];
    z = 0.180464 * fimg[0][i] + 0.0721856 * fimg[1][i] + 0.950444 * fimg[2][i];
    */

    fimg[0][i] = x;
    fimg[1][i] = y;
    fimg[2][i] = z;
  }
}

void
xyz2srgb (float **fimg, int size, int pc)
{
  int i;
  float r, g, b;

  if (pc > 3) { /* single channel, colour */
    pc -= 4;
    for (i = 0; i < size; i++) {
      fimg[(pc + 1) % 3][i] = 0.0;
      fimg[(pc + 2) % 3][i] = 0.0;
    }
  } else if (pc > 0) { /* single channel, gray */
    pc -= 1;
    for (i = 0; i < size; i++) {
      fimg[pc][i] = pow(fimg[pc][i], 1 / 2.2);
      fimg[(pc + 1) % 3][i] = fimg[pc][i];
      fimg[(pc + 2) % 3][i] = fimg[pc][i];
    }
    return;
  }

  for (i = 0; i < size; i++) {
    /* matrix RGB -> XYZ, with D65 reference white (www.brucelindbloom.com) */
    r = 3.24071 * fimg[0][i] - 1.53726 * fimg[1][i] - 0.498571 * fimg[2][i];
    g = -0.969258 * fimg[0][i] + 1.87599 * fimg[1][i] + 0.0415557 * fimg[2][i];
    b = 0.0556352 * fimg[0][i] - 0.203996 * fimg[1][i] + 1.05707 * fimg[2][i];

    /*
    r =  3.24071  * fimg[0][i] - 0.969258  * fimg[1][i]
      + 0.0556352 * fimg[2][i];
    g = -1.53726  * fimg[0][i] + 1.87599   * fimg[1][i]
      - 0.203996  * fimg[2][i];
    b = -0.498571 * fimg[0][i] + 0.0415557 * fimg[1][i]
      + 1.05707   * fimg[2][i];
    */
  
    /* scaling and gamma correction (approximate) */
    r = r < 0 ? 0 : pow(r, 1.0 / 2.2);
    g = g < 0 ? 0 : pow(g, 1.0 / 2.2);
    b = b < 0 ? 0 : pow(b, 1.0 / 2.2);
  
    fimg[0][i] = r;
    fimg[1][i] = g;
    fimg[2][i] = b;
  }
}

void lab2srgb (float **fimg, int size, int pc)
{
  int i;
  float x, y, z;
  float neutral [] = {0.5, 0.5, 0.5};

  if (pc > 3) { /* single channel, colour */
    pc -= 4;
    for (i = 0; i < size; i++) {
      fimg[(pc + 1) % 3][i] = neutral[(pc + 1) % 3];
      fimg[(pc + 2) % 3][i] = neutral[(pc + 2) % 3];
    }
  } else if (pc > 0) { /* single channel, gray */
    pc -= 1;
    for (i = 0; i < size; i++) {
      fimg[pc][i] = pow(fimg[pc][i], 1 / 2.2);
      fimg[(pc + 1) % 3][i] = fimg[pc][i];
      fimg[(pc + 2) % 3][i] = fimg[pc][i];
    }
    return;
  }

  for (i = 0; i < size; i++) {
    /* convert back to normal LAB */
    fimg[0][i] = (fimg[0][i] - 0 * 16 * 27 / 24389.0) * 116;
    fimg[1][i] = (fimg[1][i] - 0.5) * 500 * 2;
    fimg[2][i] = (fimg[2][i] - 0.5) * 200 * 2.2;

    /* matrix */
    y = (fimg[0][i] + 16) / 116;
    z = y - fimg[2][i] / 200.0;
    x = fimg[1][i] / 500.0 + y;

    /* scale */
    if (x * x * x > 216 / 24389.0)
      x = x * x * x;
    else
      x = (116 * x - 16) * 27 / 24389.0;
    if (fimg[0][i] > 216 / 27.0)
      y = y * y * y;
    else
      //y = fimg[0][i] * 27 / 24389.0;
      y = (116 * y - 16) * 27 / 24389.0;
    if (z * z * z > 216 / 24389.0)
      z = z * z * z;
    else
      z = (116 * z - 16) * 27 / 24389.0;

    /* white reference */
    fimg[0][i] = x * 0.95047;
    fimg[1][i] = y;
    fimg[2][i] = z * 1.08883;
  }
  xyz2srgb(fimg, size, 0);
}

void srgb2lab (float **fimg, int size)
{
  int i;
  float l, a, b;
  srgb2xyz(fimg, size);
  for (i = 0; i < size; i++) {
    /* reference white */
    fimg[0][i] /= 0.95047;
    /* (just for completeness)
    fimg[1][i] /= 1.00000; */
    fimg[2][i] /= 1.08883;

    /* scale */
    if (fimg[0][i] > 216 / 24389.0) {
      fimg[0][i] = pow(fimg[0][i], 1 / 3.0);
    } else {
      fimg[0][i] = (24389 * fimg[0][i] / 27.0 + 16) / 116.0;
    }
    if (fimg[1][i] > 216 / 24389.0) {
      fimg[1][i] = pow(fimg[1][i], 1 / 3.0);
    } else {
      fimg[1][i] = (24389 * fimg[1][i] / 27.0 + 16) / 116.0;
    }
    if (fimg[2][i] > 216 / 24389.0) {
      fimg[2][i] = pow(fimg[2][i], 1 / 3.0);
    } else {
      fimg[2][i] = (24389 * fimg[2][i] / 27.0 + 16) / 116.0;
    }

    l = 116 * fimg[1][i] - 16;
    a = 500 * (fimg[0][i] - fimg[1][i]);
    b = 200 * (fimg[1][i] - fimg[2][i]);
    fimg[0][i] = l / 116.0; // + 16 * 27 / 24389.0;
    fimg[1][i] = a / 500.0 / 2.0 + 0.5;
    fimg[2][i] = b / 200.0 / 2.2 + 0.5;
    if (fimg[0][i] < 0)
      fimg[0][i] = 0;
  }
}

void srgb2rgb(float **fimg, int size)
{
  /*int i;
  for (i = 0; i < size; i++)
    {
      fimg[0][i] = pow(fimg[0][i], 1.1);
      fimg[0][i] = pow(fimg[1][i], 1.1);
      fimg[0][i] = pow(fimg[2][i], 1.1);
    }*/
}

void
rgb2srgb (float **fimg, int size, int pc)
{
  int i;

  if (pc > 3) { /* single channel, colour */
    pc -= 4;
    for (i = 0; i < size; i++) {
      fimg[(pc + 1) % 3][i] = 0.0;
      fimg[(pc + 2) % 3][i] = 0.0;
    }
  } else if (pc > 0) { /* single channel, gray */
    pc -= 1;
    for (i = 0; i < size; i++) {
      /* fimg[pc][i] = pow(fimg[pc][i], 1 / 1.1); */
      fimg[(pc + 1) % 3][i] = fimg[pc][i];
      fimg[(pc + 2) % 3][i] = fimg[pc][i];
    }
    return;
  }

  /*for (i = 0; i < size; i++) {
    fimg[0][i] = pow(fimg[0][i], 1 / 1.1);
    fimg[1][i] = pow(fimg[1][i], 1 / 1.1);
    fimg[2][i] = pow(fimg[2][i], 1 / 1.1);
  }*/
}
