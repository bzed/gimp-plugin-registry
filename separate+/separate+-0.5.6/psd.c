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
#include <glib/gstdio.h>

#include <libgimp/gimp.h>

#include "libgimp/stdplugins-intl.h"

#include "platform.h"

#include "separate.h"
#include "util.h"
#include "psd.h"


typedef struct _PSDHeader
{
  guint32 signature; /* '8BPS' */
  guint16 version;   /* always 1 */
  gchar reserved[6];
  gint16 channels;
  gint32 height; /* npixels */
  gint32 width;
  gint16 depth;
  gint16 mode;
} __attribute__ ((packed)) PSDHeader;

typedef struct _PSDResResource
{
  guint32 type; /* '8BIM' */
  guint16 id;   /* 0x03ed */
  gchar name[2]; /* "\0\0" */
  gint32 size;
  gint32 hres;
  gint16 hres_unit;
  gint16 width_unit;
  gint32 vres;
  gint16 vres_unit;
  gint16 height_unit;
} __attribute__ ((packed)) PSDResResource;

typedef struct _PSDIccResource
{
  guint32 type; /* '8BIM' */
  guint16 id;   /* 0x040f */
  gchar name[2]; /* "\0\0" */
  gint32 size;
} __attribute__ ((packed)) PSDIccResource;


#define MAX_PIXELS 30000
#define MAX_BUFFER_SIZE 1048576

static void
psd_write_image_data_packbits (int    fd,
                               gint32 imageID,
                               gint32 width,
                               gint32 height)
{
  gint i, x, y, col;
  gint32 current, total;
  gint16 compression;
  GimpDrawable *drw[4];
  GimpPixelRgn region;
  gchar *src_buf, *dst_buf;
  guint16 line_bytes[MAX_PIXELS];
  gint buf_width, buf_height;
  off_t data_head, data_end;
  gchar channel_name[4][2] = { "C", "M", "Y", "K" };

  drw[0] = separate_find_channel (imageID,sep_C);
  drw[1] = separate_find_channel (imageID,sep_M);
  drw[2] = separate_find_channel (imageID,sep_Y);
  drw[3] = separate_find_channel (imageID,sep_K);

  gimp_progress_init ("");

  current = 0;
  total = height * 4;

  buf_width = ceil (width * 1.334);
  buf_height = MIN (MAX_BUFFER_SIZE / buf_width, height);

  if (!(src_buf = g_try_malloc (buf_width * buf_height)))
    return;
  if (!(dst_buf = g_try_malloc (buf_width * buf_height)))
    {
      g_free (src_buf);
      return;
    }

  compression = GINT16_TO_BE (1);
  write (fd, &compression, sizeof (gint16));

  data_head = lseek (fd, 0, SEEK_CUR);
  data_end = data_head + height * 4 * 2;

  for (i = 0; i < 4; i++)
    {
      gint n_lines;
      gint result;

      gimp_progress_set_text_printf (_("Exporting Photoshop PSD (%s channel)..."),
                                     channel_name[i]);

      if (drw[i])
        {
          gimp_pixel_rgn_init (&region, drw[i], 0, 0, width, height, FALSE, FALSE);

          for (y = 0; y < height; y += buf_height, current += buf_height)
            {
              gint read_bytes, write_bytes;

              gimp_progress_update ((gdouble)current / (gdouble)total);

              n_lines = MIN (height - y, buf_height);

              gimp_pixel_rgn_get_rect (&region, src_buf, 0, y, width, n_lines);

              read_bytes = 0;
              write_bytes = 0;

              for (col = 0; col < n_lines; col++)
                {
                  gint _write_bytes = write_bytes;
                  gchar *count = dst_buf + write_bytes++;

                  *count = 0;

                  dst_buf[write_bytes++] = 0xff - src_buf[read_bytes++];

                  for (x = 1; x < width; x++)
                    {
                      gchar tmp = 0xff - src_buf[read_bytes++];

                      if (dst_buf[write_bytes - 1] == tmp) /* 連続 */
                        {
                          if (*count == -127)
                            {
                              /* 128バイトを超えた場合 */
                              count = dst_buf + write_bytes++;
                              *count = 0;
                              dst_buf[write_bytes++] = tmp;
                            }
                          else if (*count <= 0)
                            {
                              /* 通常のカウント */
                              (*count)--;
                            }
                          else
                            {
                              /* 非連続からの切り替え */
                              (*count)--; /* 非連続としてカウントされた1バイト目の分を減らす */
                              count = dst_buf + write_bytes - 1;
                              *count = -1;
                              dst_buf[write_bytes++] = tmp;
                            }
                        }
                      else /* 非連続 */
                        {
                          if (*count >= 0 && *count != 127)
                            {
                              (*count)++;
                              dst_buf[write_bytes++] = tmp;
                            }
                          else
                            {
                              count = dst_buf + write_bytes++;
                              *count = 0;
                              dst_buf[write_bytes++] = tmp;
                            }
                        }
                    }
                  line_bytes[y + col] = GINT16_TO_BE (write_bytes - _write_bytes);
                }

              /* seek to end of pixel data */
              lseek (fd, data_end, SEEK_SET);
              result = write (fd, dst_buf, write_bytes);
              data_end += write_bytes;
            }

          /* seek to bytes per line data */
          lseek (fd, data_head + height * i * 2, SEEK_SET);
          result = write (fd, line_bytes, height * 2);
        }
      else
        {
          gint n_repeat = width / 128;
          gint bytes = 0;

          for (x = 0; x < n_repeat; x++)
            {
              dst_buf[bytes++] = -127;
              dst_buf[bytes++] = 0xff;
            }
          if (width % 128)
            {
              dst_buf[bytes++] = ((width % 128) - 1) * -1;
              dst_buf[bytes++] = 0xff;
            }

          for (col = 1; col < buf_height; col++)
            memcpy (dst_buf + bytes * col, dst_buf, bytes);

          /* seek to end of pixel data */
          lseek (fd, data_end, SEEK_SET);

          for (y = 0; y < height; y += buf_height, current += buf_height)
            {
              gimp_progress_update ((gdouble)current / (gdouble)total);

              n_lines = MIN (height - y, buf_height);

              result = write (fd, dst_buf, bytes * n_lines);

              for (col = 0; col < n_lines; col++)
                line_bytes[y + col] = GINT16_TO_BE (bytes);
            }
          data_end += bytes * height;

          /* seek to bytes per line data */
          lseek (fd, data_head + height * i * 2, SEEK_SET);
          result = write (fd, line_bytes, height * 2);
        }
    }

  gimp_progress_update (1.0);

  g_free (src_buf);
  g_free (dst_buf);
}


static void
psd_write_image_data_raw (int    fd,
                          gint32 imageID,
                          gint32 width,
                          gint32 height)
{
  gint i, x, y;
  gint32 current, total;
  gint16 compression;
  GimpDrawable *drw[4];
  GimpPixelRgn region;
  gchar *buf;
  gint buf_height;
  gchar channel_name[4][2] = { "C", "M", "Y", "K" };

  drw[0] = separate_find_channel (imageID,sep_C);
  drw[1] = separate_find_channel (imageID,sep_M);
  drw[2] = separate_find_channel (imageID,sep_Y);
  drw[3] = separate_find_channel (imageID,sep_K);

  gimp_progress_init ("");

  current = 0;
  total = height * 4;

  buf_height = MIN (MAX_BUFFER_SIZE / width, height);

  if (!(buf = g_try_malloc (width * buf_height)))
    return;

  compression = 0;
  write (fd, &compression, sizeof (gint16));

  for (i = 0; i < 4; i++)
    {
      gint n_lines;
      gint result;

      gimp_progress_set_text_printf (_("Exporting Photoshop PSD (%s channel)..."),
                                     channel_name[i]);

      if (drw[i])
        {
          gimp_pixel_rgn_init (&region, drw[i], 0, 0, width, height, FALSE, FALSE);

          for (y = 0; y < height; y += buf_height, current += buf_height)
            {
              gimp_progress_update ((gdouble)current / (gdouble)total);

              n_lines = MIN (height - y, buf_height);

              gimp_pixel_rgn_get_rect (&region, buf, 0, y, width, n_lines);

              for (x = 0; x < width * n_lines; x++)
                buf[x] = 0xff - buf[x];

              result = write (fd, buf, width * n_lines);
            }
        }
      else
        {
          memset (buf, 0xff, width * buf_height);

          for (y = 0; y < height; y += buf_height, current += buf_height)
            {
              gimp_progress_update ((gdouble)current / (gdouble)total);

              n_lines = MIN (height - y, buf_height);

              result = write (fd, buf, width * n_lines);
            }
        }
    }

  gimp_progress_update (1.0);

  g_free (buf);
}


gboolean
separate_psd_export (gchar         *filename,
                     gint32         imageID,
                     gconstpointer  profile_data,
                     gsize          profile_length,
                     gconstpointer  path_data,
                     gsize          path_length,
                     gboolean       compression)
{
  int fd;
  PSDHeader header;
  PSDResResource res;
  PSDIccResource icc;

  fd = g_open (filename, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 00644);

  if (fd != -1)
    {
      gint32 length;
      gint32 width, height;
      gdouble xres, yres;

      width = gimp_image_width (imageID);
      height = gimp_image_height (imageID);

      /* header */
      header.signature = GUINT32_TO_BE (0x38425053);
      header.version = GUINT16_TO_BE (1);
      memset (header.reserved, 0, sizeof (header.reserved));
      header.channels = GINT16_TO_BE (4); /* C, M, Y, K, and no alpha channels */
      header.height = GINT32_TO_BE (height);
      header.width = GINT32_TO_BE (width);
      header.depth = GINT16_TO_BE (8); /* 8bit per channels */
      header.mode = GINT16_TO_BE (4); /* CMYK Mode */

      write (fd, &header, sizeof (header));

      /***** color mode data *****/
      length = 0;
      write (fd, &length, sizeof (gint32));

      /***** image resources *****/
      length = 0;

      /* resolution info */
      gimp_image_get_resolution (imageID, &xres, &yres);
      res.type = GUINT32_TO_BE (0x3842494d);
      res.id = GUINT16_TO_BE (0x03ed);
      memset (res.name, 0, 2);
      res.size = GINT32_TO_BE (16);
      res.hres = GINT32_TO_BE (xres * 65536.0);
      res.hres_unit = GINT16_TO_BE (1);
      res.width_unit = GINT16_TO_BE (1);
      res.vres = GINT32_TO_BE (yres * 65536.0);
      res.vres_unit = GINT16_TO_BE (1);
      res.height_unit = GINT16_TO_BE (1);

      length += sizeof (PSDResResource);

      /* ICC profile */
      if (profile_data)
        {
          icc.type = res.type;
          icc.id = GUINT16_TO_BE (0x040f);
          memset (icc.name, 0, 2);
          icc.size = GINT32_TO_BE (profile_length);

          length += sizeof (PSDIccResource) + profile_length;
        }

      /* path */
      if (path_data)
        length += path_length;

      /* write data... */
      length = GINT32_TO_BE (length);
      write (fd, &length, sizeof (gint32));

      write (fd, &res, sizeof (PSDResResource));

      if (profile_data)
        {
          write (fd, &icc, sizeof (PSDIccResource));
          write (fd, profile_data, profile_length);
        }

      if (path_data)
        write (fd, path_data, path_length);

      /***** layer and mask info *****/
      length = 0;
      write (fd, &length, sizeof (gint32));

      /***** image data *****/
      if (compression)
        psd_write_image_data_packbits (fd, imageID, width, height);
      else
        psd_write_image_data_raw (fd, imageID, width, height);

      close (fd);

      return TRUE;
    }

  return FALSE;
}
