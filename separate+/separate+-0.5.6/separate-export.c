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

#include <gtk/gtk.h>

#include <libgimp/gimp.h>

#include "libgimp/stdplugins-intl.h"

#include "platform.h"

#include "separate.h"
#include "separate-export.h"
#include "tiff.h"
#include "psd.h"
#include "jpeg.h"


gchar *file_extention_table[] =
{
  "tif",
  "tiff",
  "jpg",
  "jpeg",
  "jpe",
  "psd",
  NULL
};

SeparateExportFunc extention_to_func_table[] =
{
  separate_tiff_export,
  separate_tiff_export,
  separate_jpeg_export,
  separate_jpeg_export,
  separate_jpeg_export,
  separate_psd_export,
  NULL
};

SeparateExportFunc export_func_table[] =
{
  separate_tiff_export,
  separate_jpeg_export,
  separate_psd_export
};

static gint32
double_to_fixed (gdouble d)
{
  gint32 i;

  d = CLAMP (d, -16.0, 16.0);

  i = d * (d > 0 ? 16777215.9375 : 16777216);

  return i;
}

typedef union
{
  gint8    i8;
  guint8   ui8;
  gint16   i16;
  guint16  ui16;
  gint32   i32;
  guint32  ui32;
  guint8   bytes[26];
  struct {
    guint16 selector;
    guint16 length;
  } __attribute__ ((packed)) rec_length;
  struct {
    guint16 selector;
    gint32 pos[6];
  } __attribute__ ((packed)) rec_knot;
  guint8  *ptr;
} PathResourceValType;

static guint8 *
create_psd_path_resource (gint32  imageID,
                          gint32  clipping_path_id,
                          gsize  *size)
{
  GByteArray *resources;
  guint16 res_id, max_res_id;
  guint32 *res_size;
  guint res_size_index;
  gint32 n_vectors, *vector_id;
  gint32 n_strokes, *stroke_id;
  gchar *clipping_path_name = NULL;
  guint clipping_path_name_length;
  gint width, height;
  gint i, j, k;
  PathResourceValType temp;

  *size = 0;

  vector_id = gimp_image_get_vectors (imageID, &n_vectors);

  if (!n_vectors)
    return NULL;
  n_vectors = MIN (n_vectors, 999);

  width = gimp_image_width (imageID);
  height = gimp_image_height (imageID);

  resources = g_byte_array_new ();

  for (res_id = 2000, i = 0; i < n_vectors; res_id++, i++)
    {
      gchar *name, *_name;
      gsize name_len;
      GError *err = NULL;

      /* signature */
      g_byte_array_append (resources, (guint8 *)"8BIM", 4);

      /* resource id */
      temp.ui16 = GUINT16_TO_BE (res_id);
      g_byte_array_append (resources, temp.bytes, 2);

      /* name */
      name = gimp_vectors_get_name (vector_id[i]);
      _name = g_convert (name, -1, "iso8859-1", "utf-8", NULL, &name_len, &err);
      if (_name && !err)
        {
//g_printerr ("iso-8859-1\n");
          temp.ui8 = MIN (name_len, 255);
        }
      else
        {
          const gchar **language_names = g_get_language_names ();
          gchar *codeset = NULL;
          gint _len, _offset;

          if (*language_names && g_ascii_strncasecmp (*language_names, "ja", 2) == 0)
            codeset = "sjis";

          _len = MIN (255, g_utf8_strlen (name, -1));
          do
            {
              g_free (_name);
              g_error_free (err);
              err = NULL;

              _offset = g_utf8_offset_to_pointer (name, _len) - name;

              if (codeset)
                _name = g_convert (name, _offset, codeset, "utf-8", NULL, &name_len, &err);
              else
                _name = g_locale_from_utf8 (name, _offset, NULL, &name_len, &err);
              _len--;
            } while (name_len > 255 && _name && !err);

          if (!_name || err)
            {
//g_printerr ("utf-8\n");
              /* iso8859-1へもシステムロケールのコードへも変換できなければutf-8のままにする */
              g_error_free (err);
              g_free (_name);

              _len = MIN (255 - 3, g_utf8_strlen (name, -1));

              while ((_offset = g_utf8_offset_to_pointer (name, _len) - name + 3) > 255)
                name_len--;

              temp.ui8 = _offset;
              _name = g_strdup_printf ("\xEF\xBB\xBF%s", name);
            }
          else
            {
//g_printerr ("system locale encoding\n");
              temp.ui8 = name_len;
            }
        }
      g_byte_array_append (resources, temp.bytes, 1);
      g_byte_array_append (resources, _name, temp.ui8);
      if (vector_id[i] == clipping_path_id)
        {
          clipping_path_name = _name;
          clipping_path_name_length = temp.ui8;
          _name = NULL;
        }
      else
        g_free (_name);
      g_free (name);
      if ((temp.ui8 + 1) % 2)
        g_byte_array_append (resources, (guint8 *)"\0", 1); /* add a padding byte */

      /* size of resource(dummy) */
      res_size_index = resources->len; /* 後で値をセットするので位置を記憶しておく */
      g_byte_array_append (resources, temp.bytes, 4);

      /* strokes */
      /* (Path fill rule record) */
      memset (temp.bytes, 0, 26);
      temp.ui16 = GUINT16_TO_BE (6);
      g_byte_array_append (resources, temp.bytes, 26);

      stroke_id = gimp_vectors_get_strokes (vector_id[i], &n_strokes);

      for (j = 0; j < n_strokes; j++)
        {
          GimpVectorsStrokeType type;
          gboolean closed;
          gint n_points;
          gdouble *points;

          type = gimp_vectors_stroke_get_points (vector_id[i], stroke_id[j],
                                                 &n_points, &points,
                                                 &closed);

          if (type != GIMP_VECTORS_STROKE_TYPE_BEZIER || n_points > 65535 || n_points % 6)
            {
              g_free (points);
              continue;
            }

          memset (temp.bytes, 0, 26);
          temp.rec_length.selector = GUINT16_TO_BE (closed ? 0 : 3);
          temp.rec_length.length = GUINT16_TO_BE (n_points / 6);
          g_byte_array_append (resources, temp.bytes, 26);

          for (k = 0; k < n_points; k += 6)
            {
              gint32 fixed;

              /* TODO : check link/unlink state */
              temp.rec_knot.selector = GUINT16_TO_BE (closed ? 2 : 5);

              fixed = double_to_fixed (points[k + 0] / width);
//g_printerr ("double : %f, fixed : %x\n", points[k + 0] / width, fixed);
              temp.rec_knot.pos[1] = GINT32_TO_BE (fixed);
//g_printerr ("BE-fixed : %x\n", temp.rec_knot.pos[1]);
              fixed = double_to_fixed (points[k + 1] / height);
//g_printerr ("double : %f, fixed : %x\n", points[k + 1] / height, fixed);
              temp.rec_knot.pos[0] = GINT32_TO_BE (fixed);
//g_printerr ("BE-fixed : %x\n\n", temp.rec_knot.pos[0]);
              fixed = double_to_fixed (points[k + 2] / width);
              temp.rec_knot.pos[3] = GINT32_TO_BE (fixed);
              fixed = double_to_fixed (points[k + 3] / height);
              temp.rec_knot.pos[2] = GINT32_TO_BE (fixed);
              fixed = double_to_fixed (points[k + 4] / width);
              temp.rec_knot.pos[5] = GINT32_TO_BE (fixed);
              fixed = double_to_fixed (points[k + 5] / height);
              temp.rec_knot.pos[4] = GINT32_TO_BE (fixed);
              g_byte_array_append (resources, temp.bytes, 26);
            }

          g_free (points);
        }

      g_free (stroke_id);

      /* update size of resource */
      res_size = (guint32 *)(resources->data + res_size_index);
      *res_size = resources->len - res_size_index - sizeof (*res_size);
      *res_size = GUINT32_TO_BE (*res_size);
    }

  if (clipping_path_name)
    {
      /* signature */
      g_byte_array_append (resources, (guint8 *)"8BIM", 4);

      /* resource id */
      temp.ui16 = GUINT16_TO_BE (2999);
      g_byte_array_append (resources, temp.bytes, 2);

      /* name */
      temp.ui16 = 0;
      g_byte_array_append (resources, temp.bytes, 2);

      /* resource size */
      res_size_index = resources->len;
      g_byte_array_append (resources, temp.bytes, 4);

      /* name of clipping path */
      temp.ui8 = clipping_path_name_length;
      g_byte_array_append (resources, temp.bytes, 1);
      g_byte_array_append (resources, clipping_path_name, temp.ui8);
      if ((temp.ui8 + 1) % 2)
        g_byte_array_append (resources, (guint8 *)"\0", 1); /* add a padding byte */
      g_free (clipping_path_name);

      /* update size of resource */
      res_size = (guint32 *)(resources->data + res_size_index);
      *res_size = resources->len - res_size_index - sizeof (*res_size);
      *res_size = GUINT32_TO_BE (*res_size);
    }

//g_file_set_contents ("z:\\res.bin", resources->data, resources->len, NULL);
  *size = resources->len;
  return g_byte_array_free (resources, FALSE);
}


void
separate_export (GimpDrawable    *drawable,
                 SeparateContext *sc)
{
  gint32 imageID = sc->imageID;//gimp_drawable_get_image( drawable->drawable_id );
  gchar *filename, *extention;
  gchar *profile_data = NULL;
  guint8 *path_data = NULL;
  gsize profile_length, path_length;
  gint32 filetype = sc->sas.filetype;
  gboolean compression = sc->sas.compression;

#ifdef ENABLE_COLOR_MANAGEMENT
  {
    cmsHPROFILE hProfile = NULL;

    if (sc->sas.embedprofile == 3)
      {
        GimpParasite *parasite = gimp_image_parasite_find (imageID, CMYKPROFILE);

        if (parasite)
          {
            profile_length = gimp_parasite_data_size (parasite);
            profile_data = g_memdup (gimp_parasite_data (parasite),
                                     profile_length);
            gimp_parasite_free (parasite);
          }
      }
    else
      {
        gchar *profilefilename;
        switch (sc->sas.embedprofile)
          {
          case 1:
            profilefilename = sc->cmykfilename;
            break;
          case 2:
            profilefilename = sc->prooffilename;
            break;
          default:
            profilefilename = "";
          }
        g_file_get_contents (profilefilename, &profile_data, &profile_length, NULL);
      }
    if (profile_data)
      {
        if ((hProfile = cmsOpenProfileFromMem (profile_data, profile_length)) &&
            cmsGetColorSpace (hProfile) == icSigCmykData)
          { /* profile is OK? */
            /* Profile is embedded, and cannot be used independently */
            profile_data[47] |= 2;
            cmsCloseProfile (hProfile);
          }
        else
          {
            if (hProfile)
              cmsCloseProfile (hProfile);

            g_free (profile_data);
            profile_data = NULL;
            profile_length = 0;
          }
      }
  }
#endif

  path_data = create_psd_path_resource (imageID, sc->sas.clipping_path_id, &path_length);

  filename = gimp_image_get_filename (imageID);

  {
    gint offset = separate_path_get_extention_offset (filename);

    if (offset)
      extention = &filename[offset + 1];
    else
      extention = "";
  }

  if (filetype == -1)
    {
      gint i = 0;

      while (file_extention_table[i])
        {
          if (g_ascii_strcasecmp (file_extention_table[i], extention) == 0)
            {
              (extention_to_func_table[i]) (filename,     imageID,
                                            profile_data, profile_length,
                                            path_data,    path_length,
                                            compression);
              break;
            }
          i++;
        }

      if (!file_extention_table[i])
        (extention_to_func_table[0]) (filename,     imageID,
                                      profile_data, profile_length,
                                      path_data,    path_length,
                                      compression);
    }
  else
    {
      if (filetype >= 0 && filetype < (sizeof (export_func_table) / sizeof (SeparateExportFunc)))
        (export_func_table[filetype]) (filename,     imageID,
                                       profile_data, profile_length,
                                       path_data,    path_length,
                                       compression);
    }

  g_free (filename);
  g_free (profile_data);
  g_free (path_data);
}
