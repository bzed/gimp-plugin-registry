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
#include <fcntl.h>
#include <setjmp.h>
#include <glib/gstdio.h>

#include <libgimp/gimp.h>

#include <jpeglib.h>

#include "libgimp/stdplugins-intl.h"

#include "platform.h"

#include "separate.h"
#include "util.h"
#include "jpeg.h"


typedef struct _JpegWriteErrMgr
{
  struct jpeg_error_mgr jerr;
  jmp_buf buf;
} JpegWriteErrMgr;


void
jpeg_write_error_handler (j_common_ptr cinfo)
{
  JpegWriteErrMgr *mgr = (JpegWriteErrMgr *) (cinfo->err);

  longjmp (mgr->buf, 1);
}


#define MARKER_LENGTH 65533
#define ICC_MARKER_HEADER_LENGTH 14
#define ICC_MARKER_DATA_LENGTH (MARKER_LENGTH - ICC_MARKER_HEADER_LENGTH)
#define PSD_MARKER_HEADER_LENGTH 14
#define PSD_MARKER_DATA_LENGTH (MARKER_LENGTH - PSD_MARKER_HEADER_LENGTH)

static void
jpeg_write_profile_data (j_compress_ptr  cinfo,
                         const guchar   *profile_data,
                         gsize           profile_length)
{
  gint n_markers, current_marker;

  n_markers = profile_length / ICC_MARKER_DATA_LENGTH;

  if (profile_length % ICC_MARKER_DATA_LENGTH)
    n_markers++;

  current_marker = 1;

  while (profile_length > 0)
    {
      guint length;

      length = MIN (profile_length, ICC_MARKER_DATA_LENGTH);
      profile_length -= length;

      jpeg_write_m_header (cinfo, JPEG_APP0 + 2, length + ICC_MARKER_HEADER_LENGTH);

      jpeg_write_m_byte (cinfo, 'I');
      jpeg_write_m_byte (cinfo, 'C');
      jpeg_write_m_byte (cinfo, 'C');
      jpeg_write_m_byte (cinfo, '_');
      jpeg_write_m_byte (cinfo, 'P');
      jpeg_write_m_byte (cinfo, 'R');
      jpeg_write_m_byte (cinfo, 'O');
      jpeg_write_m_byte (cinfo, 'F');
      jpeg_write_m_byte (cinfo, 'I');
      jpeg_write_m_byte (cinfo, 'L');
      jpeg_write_m_byte (cinfo, 'E');
      jpeg_write_m_byte (cinfo, 0);

      jpeg_write_m_byte (cinfo, current_marker);
      jpeg_write_m_byte (cinfo, n_markers);

      while (length--)
        {
          jpeg_write_m_byte (cinfo, *profile_data);
          profile_data++;
        }

      current_marker++;
    }
}


static void
jpeg_write_path_data (j_compress_ptr  cinfo,
                      const guint8   *path_data,
                      gsize           path_length)
{
  /*gint n_markers, current_marker;

  n_markers = path_length / PSD_MARKER_DATA_LENGTH;

  if (path_length % PSD_MARKER_DATA_LENGTH)
    n_markers++;*/

  //current_marker = 1;

  while (path_length > 0)
    {
      guint length;

      length = MIN (path_length, PSD_MARKER_DATA_LENGTH);
      path_length -= length;

      jpeg_write_m_header (cinfo, JPEG_APP0 + 13, length + PSD_MARKER_HEADER_LENGTH);

      jpeg_write_m_byte (cinfo, 'P');
      jpeg_write_m_byte (cinfo, 'h');
      jpeg_write_m_byte (cinfo, 'o');
      jpeg_write_m_byte (cinfo, 't');
      jpeg_write_m_byte (cinfo, 'o');
      jpeg_write_m_byte (cinfo, 's');
      jpeg_write_m_byte (cinfo, 'h');
      jpeg_write_m_byte (cinfo, 'o');
      jpeg_write_m_byte (cinfo, 'p');
      jpeg_write_m_byte (cinfo, ' ');
      jpeg_write_m_byte (cinfo, '3');
      jpeg_write_m_byte (cinfo, '.');
      jpeg_write_m_byte (cinfo, '0');
      jpeg_write_m_byte (cinfo, 0);

      //jpeg_write_m_byte (cinfo, current_marker);
      //jpeg_write_m_byte (cinfo, n_markers);

      while (length--)
        {
          jpeg_write_m_byte (cinfo, *path_data);
          path_data++;
        }

      //current_marker++;
    }
}


static void
jpeg_write_image_data (j_compress_ptr cinfo,
                       gint32         imageID)
{
  gint i, x, y;
  GimpDrawable *drw[4];
  GimpPixelRgn region[4];
  gchar *src_buf[4], *dst_buf[64];

  drw[0] = separate_find_channel (imageID,sep_C);
  drw[1] = separate_find_channel (imageID,sep_M);
  drw[2] = separate_find_channel (imageID,sep_Y);
  drw[3] = separate_find_channel (imageID,sep_K);

  gimp_progress_init (_("Exporting JPEG..."));

  for (i = 0; i < 64; i++)
    {
      dst_buf[i] = g_try_malloc (cinfo->image_width * 4);

      if (!dst_buf[i])
        goto CLEANUP;
    }

  for (i = 0; i < 4; i++)
    {
      src_buf[i] = g_try_malloc (cinfo->image_width * 64);

      if (!src_buf[i])
        goto CLEANUP;

      if (drw[i])
        {
          gimp_pixel_rgn_init (&region[i], drw[i],
                               0, 0, cinfo->image_width, cinfo->image_height,
                               FALSE, FALSE);
        }
      else
        memset (src_buf[i], 0, cinfo->image_width * 64);
    }

  for (y = 0; y < cinfo->image_height; y += 64)
    {
      gint n_lines, col;
      gint read_bytes, write_bytes;

      n_lines = MIN (cinfo->image_height - y, 64);

      gimp_progress_update ((gdouble)y / (gdouble)cinfo->image_height);

      for (i = 0; i < 4; i++)
        {
          if (drw[i])
            gimp_pixel_rgn_get_rect (&region[i], src_buf[i], 0, y, cinfo->image_width, n_lines);
        }

      read_bytes = 0;

      for (col = 0; col < n_lines; col++)
        {
          write_bytes = 0;

          for (x = 0; x < cinfo->image_width; x++)
            {
              dst_buf[col][write_bytes++] = 0xff - src_buf[0][read_bytes];
              dst_buf[col][write_bytes++] = 0xff - src_buf[1][read_bytes];
              dst_buf[col][write_bytes++] = 0xff - src_buf[2][read_bytes];
              dst_buf[col][write_bytes++] = 0xff - src_buf[3][read_bytes];

              read_bytes++;
            }
        }

      jpeg_write_scanlines (cinfo, dst_buf, n_lines);
    }

  gimp_progress_update (1.0);

CLEANUP:

  for (i = 0; i < 64; i++)
    {
      if (!dst_buf[i])
        break;

      g_free (dst_buf[i]);
    }

  g_free (src_buf[0]);
  g_free (src_buf[1]);
  g_free (src_buf[2]);
  g_free (src_buf[3]);
}


gboolean
separate_jpeg_export (gchar         *filename,
                      gint32         imageID,
                      gconstpointer  profile_data,
                      gsize          profile_length,
                      gconstpointer  path_data,
                      gsize          path_length,
                      gboolean       compression)
{
  FILE *stream;

  stream = g_fopen (filename, "wb");

  if (stream)
    {
      gdouble xres, yres;
      struct jpeg_compress_struct cinfo;
      JpegWriteErrMgr mgr;

      mgr.jerr.error_exit = jpeg_write_error_handler;
      cinfo.err = jpeg_std_error (&mgr.jerr);
      jpeg_create_compress (&cinfo);

      if (setjmp (mgr.buf) != 0)
        {
          gimp_message (_("Failed to exporting JPEG."));
          return FALSE;
        }

      jpeg_stdio_dest (&cinfo, stream);

      cinfo.image_width = gimp_image_width (imageID);
      cinfo.image_height = gimp_image_height (imageID);
      cinfo.input_components = 4;
      cinfo.in_color_space = JCS_CMYK;
      cinfo.jpeg_color_space = JCS_YCCK;

      jpeg_set_defaults (&cinfo);

      jpeg_set_colorspace (&cinfo, JCS_YCCK);

      jpeg_set_quality (&cinfo, compression ? 65 : 95, TRUE);

      gimp_image_get_resolution (imageID, &xres, &yres);
      cinfo.X_density = xres;
      cinfo.Y_density = yres;
      cinfo.density_unit = 1;
      cinfo.optimize_coding = TRUE;
      cinfo.dct_method = JDCT_FLOAT;
      cinfo.write_JFIF_header = TRUE;
      cinfo.write_Adobe_marker = TRUE;

      jpeg_start_compress (&cinfo, TRUE);

      if (profile_data)
        jpeg_write_profile_data (&cinfo, profile_data, profile_length);

      if (path_data)
        jpeg_write_path_data (&cinfo, path_data, path_length);

      jpeg_write_image_data (&cinfo, imageID);

      jpeg_finish_compress (&cinfo);

      jpeg_destroy_compress (&cinfo);
      fclose (stream);

      return TRUE;
    }

  return FALSE;
}
