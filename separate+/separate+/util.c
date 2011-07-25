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
#include <glib/gi18n.h>

#include <libgimp/gimp.h>

#include "platform.h"

#include "separate.h"
#include "util.h"

static char *separate_channelnames[] = {"C", "M", "Y", "K"};

static gboolean
check_layer_name (char *layer_name,
                  gint *mask)
{
  if ((((*mask) & 16) == 0) && (strcmp (layer_name, _("Background")) == 0))
    {
      *mask |= 16;
      return TRUE;
    }
  if ((((*mask) & 1) == 0) && (strcmp (layer_name, "C") == 0))
    {
      *mask |= 1;
      return TRUE;
    }
  if ((((*mask) & 2) == 0) && (strcmp (layer_name, "M") == 0))
    {
      *mask |= 2;
      return TRUE;
    }
  if ((((*mask) & 4) == 0) && (strcmp (layer_name, "Y") == 0))
    {
      *mask |= 4;
      return TRUE;
    }
  if ((((*mask) & 8) == 0) && (strcmp (layer_name, "K") == 0))
    {
      *mask |= 8;
      return TRUE;
    }
  return FALSE;
}


GimpDrawable *
separate_find_channel (gint32                image_id,
                       enum separate_channel channel)
{
  GimpDrawable *result=NULL;
  gint *layers, layercount;
  gint i;

  if ((channel < 0) || (channel > 3))
    return NULL;

  layers = gimp_image_get_layers (image_id, &layercount);
  for (i = 0; i < layercount; ++i)
    {
      char *layer_name = gimp_drawable_get_name (layers[i]);

      if (strcmp (layer_name, separate_channelnames[channel]) == 0)
        {
          result = gimp_drawable_get (layers[i]);
          if (gimp_drawable_is_rgb (result->drawable_id))
            result = gimp_drawable_get (gimp_layer_get_mask (layers[i]));

          return result;
        }
    }

  return result;
}

GimpDrawable *
separate_find_alpha (gint32 image_id)
{
  gint *channels, n_channels;
  gint i;

  channels = gimp_image_get_channels (image_id, &n_channels);

  if (n_channels && gimp_drawable_get_visible (channels[0]))
    return gimp_drawable_get (channels[0]);
  else
    return NULL;
}


gboolean
separate_is_CMYK (gint32 image_id)
{
  gint *layers, layercount;
  gint i;
  gint mask = 0;

  layers = gimp_image_get_layers (image_id, &layercount);

  if (layercount > 5)
    return FALSE;

  for (i = 0; i < layercount; ++i)
    {
      char *layer_name = gimp_drawable_get_name (layers[i]);

      if (check_layer_name (layer_name, &mask) == FALSE)
        return FALSE;
    }

  if (mask == 0 || mask == 16)
    return FALSE;

  return TRUE;
}


void
separate_init_settings (SeparateContext        *sc,
                        enum separate_function  func,
                        gboolean                get_last_values)
{
#ifdef ENABLE_COLOR_MANAGEMENT
  GimpColorConfig *config;
#endif

  memset (sc, '\0', sizeof (SeparateContext));

  /* set default values */
  switch (func)
    {
    case SEP_LIGHT:
    case SEP_FULL:
    case SEP_SEPARATE:
    case SEP_PROOF:
#ifdef ENABLE_COLOR_MANAGEMENT
      if ((config = gimp_get_color_configuration()))
        {
          g_object_get (config,
                        "display-profile", &(sc->alt_displayfilename),
                        "rgb-profile", &(sc->alt_rgbfilename),
                        "cmyk-profile", &(sc->alt_cmykfilename),
                        "printer-profile", &(sc->alt_prooffilename),
                        "display-rendering-intent", &(sc->ps.mode),
                        "simulation-rendering-intent", &(sc->ss.intent),
                        NULL);
          if (sc->ps.mode >= 2)
            sc->ps.mode--;
        }
      g_object_unref (G_OBJECT (config));
#endif

      if (!(sc->alt_displayfilename))
        sc->alt_displayfilename = DEFAULT_RGB_PROFILE;
      if (!(sc->alt_rgbfilename))
        sc->alt_rgbfilename = DEFAULT_RGB_PROFILE;
      if (!(sc->alt_cmykfilename))
        sc->alt_cmykfilename = DEFAULT_CMYK_PROFILE;
      if (!(sc->alt_prooffilename))
        sc->alt_prooffilename = DEFAULT_CMYK_PROFILE;

      sc->ss.profile = TRUE;
      sc->ps.profile = TRUE;

      break;
    case SEP_SAVE:
    case SEP_EXPORT:
#ifdef ENABLE_COLOR_MANAGEMENT
      if ((config = gimp_get_color_configuration()))
        {
          g_object_get (config,
                        "cmyk-profile", &(sc->cmykfilename),
                        "printer-profile", &(sc->prooffilename),
                        NULL);
        }
      g_object_unref (G_OBJECT (config));
#endif
      break;
    case SEP_DUOTONE:
    default:
      break;
    }

  /* get last values */
  if (get_last_values)
    {
      gint size;

      switch (func)
        {
          /* TODO : g_free ()やNULL値の設定が必要でないことの検証 */
        case SEP_LIGHT:
        case SEP_FULL:
        case SEP_SEPARATE:
          if ((size = gimp_get_data_size ("separate_rgbprofile")))
            {
              g_free (sc->rgbfilename);
              sc->rgbfilename = g_new (gchar, size);
              gimp_get_data ("separate_rgbprofile", sc->rgbfilename);
            }
          g_free (sc->cmykfilename);
          if ((size = gimp_get_data_size ("separate_cmykprofile")) > 1)
            {
              sc->cmykfilename = g_new (gchar, size);
              gimp_get_data ("separate_cmykprofile", sc->cmykfilename);
            }
          else
            sc->cmykfilename = NULL;
          gimp_get_data ("separate_settings", &(sc->ss));
          break;
        case SEP_PROOF:
          if ((size = gimp_get_data_size ("separate_displayprofile")))
            {
              g_free (sc->displayfilename );
              sc->displayfilename = g_new (gchar, size);
              gimp_get_data ("separate_displayprofile", sc->displayfilename);
            }
          if ((size = gimp_get_data_size ("separate_proofprofile")))
            {
              g_free (sc->prooffilename);
              sc->prooffilename = g_new (gchar, size);
              gimp_get_data ("separate_proofprofile", sc->prooffilename);
            }
          gimp_get_data( "separate_proofsettings", &( sc->ps ) );
          break;
        case SEP_EXPORT:
          gimp_get_data ("separate_exportsettings", &(sc->sas));
          break;
        default:
          break;
        }
    }
}

void
separate_store_settings (SeparateContext        *sc,
                         enum separate_function  func)
{
  switch( func ) {
  case SEP_SEPARATE:
  case SEP_FULL:
  case SEP_LIGHT:
    if( sc->rgbfilename )
      gimp_set_data( "separate_rgbprofile", sc->rgbfilename, strlen( sc->rgbfilename ) + 1 );
    if( sc->cmykfilename )
      gimp_set_data( "separate_cmykprofile", sc->cmykfilename, strlen( sc->cmykfilename ) + 1 );
    else
      gimp_set_data( "separate_cmykprofile", "", 1);
    gimp_set_data( "separate_settings", &( sc->ss ), sizeof( SeparateSettings ) );
    break;
  case SEP_PROOF:
    if( sc->displayfilename )
      gimp_set_data( "separate_displayprofile", sc->displayfilename, strlen( sc->displayfilename ) + 1 );
    if( sc->prooffilename )
      gimp_set_data( "separate_proofprofile", sc->prooffilename, strlen( sc->prooffilename ) + 1 );
    gimp_set_data( "separate_proofsettings", &( sc->ps ), sizeof( ProofSettings ) );
    break;
  case SEP_EXPORT:
    gimp_set_data ("separate_exportsettings", &(sc->sas), sizeof (SaveSettings));
    break;
  default:
    break;
  } 
}

/* Create a normal RGB image for proof...*/
gint32
separate_create_RGB (gchar    *filename,
                     guint     width,
                     guint     height,
                     gboolean  has_alpha,
                     gint32   *layers)
{
  gint32 image_id;

  image_id = gimp_image_new (width, height, GIMP_RGB);
  gimp_image_undo_disable (image_id);
  gimp_image_set_filename (image_id, filename);

  if (has_alpha)
    layers[0] = gimp_layer_new (image_id, _("Layer 1"), width, height,
                                GIMP_RGBA_IMAGE, 100, GIMP_NORMAL_MODE);
  else
    layers[0] = gimp_layer_new (image_id, _("Background"), width, height,
                                GIMP_RGB_IMAGE, 100, GIMP_NORMAL_MODE);

  gimp_image_add_layer (image_id, layers[0], -1);

  return image_id;
}


/* Create an image with four greyscale layers, to be used as CMYK channels...*/
gint32
separate_create_planes_grey (gchar  *filename,
                             guint   width,
                             guint   height,
                             gint32 *layers)
{
  gint32 image_id;

  image_id = gimp_image_new (width, height, GIMP_GRAY);
  gimp_image_undo_disable (image_id);
  gimp_image_set_filename (image_id, filename);

  layers[0] = gimp_layer_new (image_id, "K", width, height,
                              GIMP_GRAY_IMAGE, 100, GIMP_NORMAL_MODE);
  gimp_image_add_layer (image_id, layers[0], -1);
  layers[1] = gimp_layer_new (image_id, "Y", width, height,
                              GIMP_GRAY_IMAGE, 100, GIMP_NORMAL_MODE);
  gimp_image_add_layer (image_id, layers[1], -1);
  layers[2] = gimp_layer_new (image_id, "M", width, height,
                              GIMP_GRAY_IMAGE, 100, GIMP_NORMAL_MODE);
  gimp_image_add_layer (image_id, layers[2], -1);
  layers[3] = gimp_layer_new (image_id, "C", width, height,
                              GIMP_GRAY_IMAGE, 100, GIMP_NORMAL_MODE);
  gimp_image_add_layer (image_id, layers[3], -1);

  return image_id;
}


/* Create an image with four colour layers with masks, to be used as CMYK channels...*/
gint32
separate_create_planes_CMYK (gchar  *filename,
                             guint   width,
                             guint   height,
                             gint32 *layers,
                             guchar *primaries)
{
  gint32 image_id;
  gint32 background_id;
  gint counter;
  GimpRGB rgb;

  image_id = gimp_image_new (width, height, GIMP_RGB);
  gimp_image_undo_disable (image_id);
  gimp_image_set_filename (image_id, filename);

  background_id = gimp_layer_new (image_id, _("Background"), width, height,
                                  GIMP_RGB_IMAGE, 100, GIMP_NORMAL_MODE);
  gimp_image_add_layer (image_id, background_id, -1);
  layers[0] = gimp_layer_new (image_id, "K", width, height,
                              GIMP_RGBA_IMAGE, 100, GIMP_DARKEN_ONLY_MODE);
  gimp_image_add_layer (image_id, layers[0], -1);
  layers[1] = gimp_layer_new (image_id, "Y", width, height,
                              GIMP_RGBA_IMAGE, 100, GIMP_DARKEN_ONLY_MODE);
  gimp_image_add_layer (image_id, layers[1], -1);
  layers[2] = gimp_layer_new (image_id, "M", width, height,
                              GIMP_RGBA_IMAGE, 100, GIMP_DARKEN_ONLY_MODE);
  gimp_image_add_layer (image_id, layers[2], -1);
  layers[3] = gimp_layer_new (image_id, "C", width, height,
                              GIMP_RGBA_IMAGE, 100, GIMP_DARKEN_ONLY_MODE);
  gimp_image_add_layer (image_id, layers[3], -1);

  gimp_context_push ();

  for (counter = 0; counter < 4; counter++)
    {
      gimp_rgb_set_uchar (&rgb, primaries[0], primaries[1], primaries[2]);
      gimp_context_set_foreground (&rgb);
      primaries += 3;

      gimp_drawable_fill (layers[counter], GIMP_FOREGROUND_FILL);
    }

  rgb.r = rgb.g = rgb.b = 1.0;
  gimp_context_set_foreground (&rgb);
  gimp_drawable_fill (background_id, GIMP_FOREGROUND_FILL);

  gimp_context_pop ();

  return image_id;
}


/* Create an image with two colour layers with masks, to be used as MK duotone channels...*/
gint32
separate_create_planes_Duotone (gchar  *filename,
                                guint   width,
                                guint   height,
                                gint32 *layers)
{
  gint32 image_id;
  gint32 background_id;
  GimpRGB rgb = {0};

  image_id = gimp_image_new (width, height, GIMP_RGB);
  gimp_image_undo_disable (image_id);
  gimp_image_set_filename (image_id, filename);

  background_id = gimp_layer_new (image_id, _("Background"), width, height,
                                  GIMP_RGB_IMAGE, 100, GIMP_NORMAL_MODE);
  gimp_image_add_layer (image_id, background_id, -1);

  layers[0] = gimp_layer_new (image_id, "K", width, height,
                              GIMP_RGBA_IMAGE, 100, GIMP_DARKEN_ONLY_MODE);
  gimp_image_add_layer (image_id, layers[0], -1);

  layers[1] = gimp_layer_new (image_id, "M", width, height,
                              GIMP_RGBA_IMAGE, 100, GIMP_DARKEN_ONLY_MODE);
  gimp_image_add_layer (image_id, layers[1], -1);

  gimp_context_push ();

  gimp_context_set_foreground (&rgb);
  gimp_drawable_fill (layers[0], GIMP_FOREGROUND_FILL);

  rgb.r = 1.0;
  gimp_context_set_foreground (&rgb);
  gimp_drawable_fill (layers[1], GIMP_FOREGROUND_FILL);

  rgb.r = rgb.g = rgb.b = 1.0;
  gimp_context_set_foreground (&rgb);
  gimp_drawable_fill (background_id, GIMP_FOREGROUND_FILL);

  gimp_context_pop ();

  return image_id;
}


char *
separate_build_filename (char *root,
                         char *suffix)
{
  /* Build a filename like <imagename>-<channel>.<extension> */
  char *filename;
  char *extension;
  root = g_strdup (root);
  extension = root + strlen (root) - 1;

  while (extension >= root)
    {
      if (*extension == '.')
        break;

      extension--;
    }

  if (extension >= root)
    {
      *(extension++) = '\0';
      filename = g_strdup_printf ("%s-%s.%s", root, suffix, extension);
    }
  else
    filename = g_strdup_printf ("%s-%s", root, suffix);

  g_free (root);

  return filename;
}


char *
separate_filename_add_suffix (char *root,
                              char *suffix)
{
  /* Build a filename like <imagename>-<channel>.<extension> */
  char *filename;
  char *extension;

  if (root == NULL)
    return g_strdup_printf (_("Untitled-%s.tif"), suffix);

  root = g_strdup (root);
  extension = root + strlen (root) - 1;

  while (extension >= root)
    {
      if (*extension == '.') break;
      extension--;
    }
  if (extension >= root)
    {
      *(extension++) = '\0';
    }

  filename = g_strdup_printf ("%s-%s.tif", root, suffix);
  g_free (root);

  return filename;
}


char *
separate_filename_change_extension (char *root,
                                    char *newext)
{
  /* Change <imagename>.<extension> to <imagename>.<tif> */
  char *filename;
  char *extension;

  root =g_strdup (root);
  extension = root + strlen (root) - 1;

  while (extension >= root)
    {
      if (*extension == '.')
        break;

      extension--;
    }

  if (extension >= root)
    {
      *extension++ = 0;
      filename = g_strdup_printf ("%s.%s", root, newext);
    }
  else
    filename = g_strdup (root);

  g_free (root);

  return filename;
}


gint
separate_path_get_extention_offset (gchar *filename)
{
  gint length;
  gint offset;

  g_return_val_if_fail (filename != NULL, 0);

  length = strlen (filename);
  offset = length;

  while (offset > 0)
    {
      if (filename[offset] == '.')
        {
          if (!(offset == length - 1) && !G_IS_DIR_SEPARATOR (filename[offset - 1]))
            return offset;
          else
            return 0;
        }

      offset--;
    }

  return 0;
}
