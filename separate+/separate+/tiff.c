/* separate+ 0.5 - image processing plug-in for the Gimp
 *
 * Copyright (C) 2002-2004 Alastair Robinson (blackfive@fakenhamweb.co.uk),
 * Based on code by Andrew Kieschnick and Peter Kirchgessner
 * 2007-2010 Modified by Yoshinori Yamakawa (yamma-ma@users.sourceforge.jp)
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libgimp/gimp.h>

#include "libgimp/stdplugins-intl.h"

#include "platform.h"

#include "separate.h"
#include "util.h"
#include "tiff.h"

#include <tiffio.h>

#define STRIPHEIGHT 64

static gboolean
separate_writetiffdata (TIFF    *out,
                        gint32  imageID,
                        gint32  width,
                        gint32  height)
{
  int result = TRUE;
  int32 BufSizeOut = TIFFStripSize (out);
  unsigned char *BufferOut;
  int i;
  int StripCount = (height + (STRIPHEIGHT - 1)) / STRIPHEIGHT;
  int32 sw = width;
  int32 sl = MIN (height, STRIPHEIGHT);

  GimpDrawable *drw[4];
  GimpPixelRgn pixrgn[4];
  gchar *chanbuf[4];

  drw[0] = separate_find_channel (imageID,sep_C);
  drw[1] = separate_find_channel (imageID,sep_M);
  drw[2] = separate_find_channel (imageID,sep_Y);
  drw[3] = separate_find_channel (imageID,sep_K);

  for (i=0; i < 4; ++i)
    {
      if (!(chanbuf[i] = malloc (sw * sl)))
        result = FALSE;
      if (drw[i])
        gimp_pixel_rgn_init (&pixrgn[i], drw[i], 0, 0, width, height, FALSE, FALSE);
    }
  if (!result)
    return FALSE;

  BufferOut = (unsigned char *)_TIFFmalloc (BufSizeOut);
  if (!BufferOut)
    return FALSE;

  gimp_progress_init (_("Exporting TIFF..."));

  for (i = 0; i < StripCount; i++)
    {
      int j;
      unsigned char *src[4] = {NULL, NULL, NULL, NULL};
      unsigned char *dest = BufferOut;
      int x, y;

      gimp_progress_update (((double)i) / ((double)StripCount));

      for (j = 0; j < 4; ++j)
        {
          if (drw[j])
            {
              int left, top, wd, ht;
              left = 0;
              top = i * STRIPHEIGHT;
              wd = width;
              ht = (top + STRIPHEIGHT > height) ? height - top : STRIPHEIGHT;
              src[j] = chanbuf[j];
              gimp_pixel_rgn_get_rect (&pixrgn[j], src[j], left, top, wd, ht);
            }
        }
      for (y = 0; y < sl; ++y)
        {
          for (x = 0; x < sw; ++x)
            {
              if (src[0])
                *dest++ = *src[0]++;
              else
                *dest++ = 0;
              if (src[1])
                *dest++ = *src[1]++;
              else
                *dest++ = 0;
              if (src[2])
                *dest++ = *src[2]++;
              else
                *dest++ =0;
              if (src[3])
                *dest++ = *src[3]++;
              else
                *dest++ = 0;
            }
        }
      TIFFWriteEncodedStrip (out, i, BufferOut, BufSizeOut);
    }

  gimp_progress_update (1.0);

  _TIFFfree (BufferOut);
  return TRUE;
}


gboolean
separate_tiff_export (gchar         *filename,
                      gint32         imageID,
                      gconstpointer  profile_data,
                      gsize          profile_length,
                      gconstpointer  path_data,
                      gsize          path_length,
                      gboolean       compression)

{
  gint32 width, height;
  gdouble xres, yres;
  TIFF *out;

#ifdef G_OS_WIN32
  {
    gchar *_filename; // win32 filename encoding(not UTF8)

    if ((_filename = g_win32_locale_filename_from_utf8 (filename)))
      {
        out = TIFFOpen (_filename, "w");
        g_free (_filename);
      }
    else
      out = TIFFOpen (filename, "w");
  }
#else
  out = TIFFOpen (filename, "w");
#endif

  if (out)
    {
      gimp_image_get_resolution (imageID, &xres, &yres);
      width = gimp_image_width (imageID);
      height = gimp_image_height (imageID);

      TIFFSetField (out, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_SEPARATED);
      TIFFSetField (out, TIFFTAG_SAMPLESPERPIXEL, 4);
      TIFFSetField (out, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
      TIFFSetField (out, TIFFTAG_INKSET, INKSET_CMYK);
      TIFFSetField (out, TIFFTAG_BITSPERSAMPLE, 8);
      TIFFSetField (out, TIFFTAG_IMAGEWIDTH, width);
      TIFFSetField (out, TIFFTAG_IMAGELENGTH, height);
      TIFFSetField (out, TIFFTAG_RESOLUTIONUNIT, RESUNIT_INCH);
      TIFFSetField (out, TIFFTAG_XRESOLUTION, xres);
      TIFFSetField (out, TIFFTAG_YRESOLUTION, yres);
      TIFFSetField (out, TIFFTAG_ROWSPERSTRIP, STRIPHEIGHT);
      TIFFSetField (out, TIFFTAG_COMPRESSION, compression ? COMPRESSION_LZW : COMPRESSION_NONE);

      if (profile_data)
        TIFFSetField (out, TIFFTAG_ICCPROFILE, profile_length, profile_data);

      if (path_data)
        TIFFSetField (out, TIFFTAG_PHOTOSHOP, path_length, path_data);

      separate_writetiffdata (out, imageID, width, height);

      TIFFWriteDirectory (out);
      TIFFClose (out);

      return TRUE;
    }

  return FALSE;
}
