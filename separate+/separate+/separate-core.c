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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <gtk/gtk.h>

#include <libgimp/gimp.h>

#include "libgimp/stdplugins-intl.h"

#include "platform.h"

#include "separate.h"
#include "separate-core.h"
#include "util.h"
#include "iccbutton.h"



#ifdef ENABLE_COLOR_MANAGEMENT
static void      embed_cmyk_profile (gint32  image_id,
                                     gchar  *filename);
#endif

static gboolean  setup_transform    (SeparateContext *sc);


#ifdef ENABLE_COLOR_MANAGEMENT
static void
embed_cmyk_profile (gint32  image_id,
                    gchar  *filename)
{
  gsize length;
  gchar *buf = NULL;

  g_return_if_fail (filename != NULL);

  if (g_file_get_contents (filename, &buf, &length, NULL))
    {
      GimpParasite *parasite;
    
      /* Profile is embedded, and cannot be used independently */
      buf[47] |= 2;

      parasite = gimp_parasite_new (CMYKPROFILE, GIMP_PARASITE_PERSISTENT, length, buf);
      gimp_image_parasite_attach (image_id, parasite);
      gimp_parasite_free (parasite);

      g_free (buf);
    }
}
#endif

static void
duplicate_paths (gint32 src_id,
                 gint32 dst_id)
{
  gint *src, n_src;

  src = gimp_image_get_vectors (src_id, &n_src);

  if (n_src)
    {
      gint i, j;
      gint src_active_vectors_id = gimp_image_get_active_vectors (src_id);
      gint dst_active_vectors_id;

      for (i = 0; i < n_src; i++)
        {
          gchar *name = gimp_vectors_get_name (src[i]);
          gint vectors_id = gimp_vectors_new (dst_id, name);
          gint *strokes, n_strokes;
          GimpVectorsStrokeType type;
          gdouble *points;
          gint n_points;
          gboolean closed;

          strokes = gimp_vectors_get_strokes (src[i], &n_strokes);

          for (j = 0; j < n_strokes; j++)
            {
              type = gimp_vectors_stroke_get_points (src[i], strokes[j], &n_points, &points, &closed);
              gimp_vectors_stroke_new_from_points (vectors_id, type, n_points, points, closed);

              g_free (points);
            }

          g_free (strokes);

          if (src[i] == src_active_vectors_id)
            dst_active_vectors_id = vectors_id;

          gimp_vectors_set_visible (vectors_id, gimp_vectors_get_visible (src[i]));
          gimp_vectors_set_linked (vectors_id, gimp_vectors_get_linked (src[i]));
          gimp_image_add_vectors (dst_id, vectors_id, i);
        }

      g_free (src);

      gimp_image_set_active_vectors (dst_id, dst_active_vectors_id);
    }

  return;
}


static gboolean
setup_transform (SeparateContext *sc)
{
  cmsHPROFILE hInProfile = NULL, hOutProfile = NULL;
  DWORD src_format, dst_format;

  sc->drawable_has_alpha = gimp_drawable_has_alpha (sc->drawable->drawable_id);
  src_format = sc->drawable_has_alpha ? TYPE_RGBA_8 : TYPE_RGB_8;
  dst_format = sc->ss.dither ? TYPE_CMYK_8 | DITHER_SH (1) : TYPE_CMYK_8;

  if (!sc->rgbfilename)
    {
      sc->rgbfilename = sc->alt_rgbfilename;
      sc->alt_rgbfilename = NULL;
    }

  hInProfile = lcms_open_profile (sc->rgbfilename);

  if (hInProfile && cmsGetDeviceClass (hInProfile) == icSigLinkClass)
    sc->hTransform = cmsCreateTransform (hInProfile, src_format, NULL, dst_format, 0, 0);
  else
    {
      DWORD dwFlags = 0;
      SeparateRenderingIntent intent;

      if (sc->ss.intent < 0 || sc->ss.intent > INTENT_ABSOLUTE_COLORIMETRIC + 1)
        {
          gimp_message (_("Rendering intent is invalid."));
          return FALSE;
        }

      if (sc->ss.profile)
        {
          GimpParasite *parasite = gimp_image_parasite_find (sc->imageID, "icc-profile");

          if (parasite)
            {
              cmsHPROFILE tmp;

              tmp = cmsOpenProfileFromMem ((gpointer)gimp_parasite_data (parasite),
                                           gimp_parasite_data_size (parasite));
              gimp_parasite_free (parasite);

              if (tmp)
                {
                  if (hInProfile)
                    cmsCloseProfile (hInProfile);

                  hInProfile = tmp;
                }
              else
                {
                  gimp_message (_("Cannot read the embedded profile.\n"));

                  if (hInProfile)
                    cmsCloseProfile (hInProfile);

                  return FALSE;
                }
            }
        }

      if (!hInProfile)
        {
          gimp_message (_("Cannot open the source/devicelink profile.\n"));
          return FALSE;
        }

      if (!sc->cmykfilename)
        {
          sc->cmykfilename = sc->alt_cmykfilename;
          sc->alt_cmykfilename = NULL;
        }

      hOutProfile = lcms_open_profile (sc->cmykfilename);

      if (!hOutProfile)
        {
          gimp_message (_("Cannot open the destination profile.\n"));
          cmsCloseProfile (hInProfile);
          return FALSE;
        }

      if (sc->ss.bpc)
        dwFlags |= cmsFLAGS_BLACKPOINTCOMPENSATION;

      if (sc->ss.intent == INTENT_ABSOLUTE_COLORIMETRIC + 1)
        {
          dwFlags |= cmsFLAGS_NOWHITEONWHITEFIXUP;
          cmsSetAdaptationState (1.0);
        }
      else
        cmsSetAdaptationState (0);

      intent = sc->ss.intent > INTENT_ABSOLUTE_COLORIMETRIC ? INTENT_ABSOLUTE_COLORIMETRIC : sc->ss.intent;

      sc->hTransform = cmsCreateTransform (hInProfile, src_format, hOutProfile, dst_format, intent, dwFlags);

      cmsCloseProfile (hOutProfile);
    }

  if (!hInProfile)
    cmsCloseProfile (hInProfile);

  if (!sc->hTransform)
    {
      gimp_message (_("Cannot build transform.\nThere might be an error in the specification of the profile."));
      return FALSE;
    }

  return TRUE;
}

static void
separate_core (SeparateContext *sc,
               unsigned char   *src,
               gint             size)
{
  gint i, b1, b2, b3, b4, b5;
  guchar *dp1, *dp2, *dp3, *dp4, *dp5;
  static guchar richBlack[] = "\0\0\0\0";

  /* keep ink limit */
  if (*((gint32 *)richBlack) == 0 )
    {
      gdouble ratio;
      cmsDoTransform (sc->hTransform, "\0\0\0\0", richBlack, 1);
      ratio = (255.0 - richBlack[3]) / (richBlack[0] + richBlack[1] + richBlack[2]);
      richBlack[0] = CLAMP (richBlack[0] - richBlack[0] * ratio, 0, 255);
      richBlack[1] = CLAMP (richBlack[1] - richBlack[1] * ratio, 0, 255);
      richBlack[2] = CLAMP (richBlack[2] - richBlack[2] * ratio, 0, 255);
    }

  b1 = sc->bpp[0];
  b2 = sc->bpp[1];
  b3 = sc->bpp[2];
  b4 = sc->bpp[3];

  dp1 = sc->destptr[0];
  dp2 = sc->destptr[1];
  dp3 = sc->destptr[2];
  dp4 = sc->destptr[3];

  if (sc->drawable_has_alpha)
    {
      b5 = sc->bpp[4];
      dp5 = sc->destptr[4];
    }

  cmsDoTransform (sc->hTransform, src, sc->cmyktemp, size);

  if (sc->ss.preserveblack)
    {
      for (i = 0; i < size; ++i)
        {
          int r = *src++;
          int g = *src++;
          int b = *src++;

          if ((r | g | b) != 0)
            {
              dp1[i*b1] = sc->cmyktemp[i*4+3];
              dp2[i*b2] = sc->cmyktemp[i*4+2]; //ly
              dp3[i*b3] = sc->cmyktemp[i*4+1]; //lm
              dp4[i*b4] = sc->cmyktemp[i*4];   //lc
            }
          else
            {
              dp1[i*b1] = 255;
              if (!(sc->ss.overprintblack))
                {
                  dp2[i*b2] = 0;
                  dp3[i*b3] = 0;
                  dp4[i*b4] = 0;
                }
              else
                {
                  dp2[i*b2] = richBlack[2];
                  dp3[i*b3] = richBlack[1];
                  dp4[i*b4] = richBlack[0];
                }
            }

          if (sc->drawable_has_alpha)
            dp5[i*b5] = *(src++);
        }
    }
  else
    {
      for (i = 0; i < size; ++i)
        {
          dp1[i*b1] = sc->cmyktemp[i*4+3];
          dp2[i*b2] = sc->cmyktemp[i*4+2];
          dp3[i*b3] = sc->cmyktemp[i*4+1];
          dp4[i*b4] = sc->cmyktemp[i*4];

          if (sc->drawable_has_alpha)
            {
              src += 3;
              dp5[i*b5] = *(src++);
            }
        }
    }
}


void
separate_full (GimpDrawable    *drawable,
               GimpParam       *values,
               SeparateContext *sc)
{
  GimpPixelRgn srcPR;
  gpointer tileiterator;
  gint width, height;
  guchar *src;
  gint32 rgbimage = sc->imageID;

  guchar rgbprimaries[] =
  {
      0,   0,   0,
    237, 220,  33,
    236,  38,  99,
     46, 138, 222
  };

#if 0
  guchar cmykprimaries[] =
  {
      0,   0,   0, 255,
      0,   0, 255,   0,
      0, 255,   0,   0,
    255,   0,   0,   0
  };

  sc->hTransform = cmsCreateTransform (hOutProfile, TYPE_CMYK_8,
                                       hInProfile, TYPE_RGB_8,
                                       INTENT_RELATIVE_COLORIMETRIC,
                                       cmsFLAGS_BLACKPOINTCOMPENSATION | cmsFLAGS_NOTPRECALC);

  cmsDoTransform (sc->hTransform,
                  cmykprimaries,
                  rgbprimaries,
                  4);

  cmsDeleteTransform (sc->hTransform);
#endif


  if (!setup_transform (sc))
  {
    values[0].data.d_image = -1;
    return;
  }

  width  = drawable->width;
  height = drawable->height;

  {
    gint32 new_image_id, counter;
    gdouble xres, yres;
    gint n_drawables = 4;
    GimpDrawable *drawables[5];
    GimpPixelRgn pixrgn[5];
    gint32 layers[4];
    gint32 mask[4];
    gint32 ntiles = 0, tilecounter = 0;

    gchar *filename = separate_filename_add_suffix (gimp_image_get_filename (gimp_drawable_get_image (drawable->drawable_id)), "CMYK");

    values[0].data.d_image = new_image_id =
      separate_create_planes_CMYK (filename,
                                   drawable->width, drawable->height,
                                   layers, rgbprimaries);
    g_free (filename);

    gimp_image_get_resolution (rgbimage, &xres, &yres);
    gimp_image_set_resolution (new_image_id, xres, yres);

    for (counter = 0; counter < 4; ++counter)
      {
        mask[counter] = gimp_layer_create_mask (layers[counter], GIMP_ADD_WHITE_MASK);
        gimp_layer_add_mask (layers[counter], mask[counter]);
        drawables[counter] = gimp_drawable_get (mask[counter]);
      }

    if (sc->drawable_has_alpha)
      {
        const GimpRGB color = {1.0, 1.0, 1.0};
        gint32 channel;

        channel = gimp_channel_new (new_image_id, _("Alpha of source image"),
                                    width, height, 100.0, &color);
        gimp_channel_set_show_masked (channel, TRUE);
        gimp_drawable_set_visible (channel, FALSE);
        gimp_image_add_channel (new_image_id, channel, 0);

        drawables[4] = gimp_drawable_get (channel);

        n_drawables++;
      }

    gimp_pixel_rgn_init (&srcPR, drawable, 0, 0, width, height, FALSE, FALSE);

    for (counter = 0; counter < n_drawables; ++counter)
      gimp_pixel_rgn_init (&pixrgn[counter], drawables[counter], 0, 0, width, height, TRUE, FALSE);

    sc->cmyktemp = g_new (guchar, 64 * 64 * 4);

    gimp_progress_init (_("Separating..."));
    ntiles = drawable->ntile_rows * drawable->ntile_cols;
    tileiterator = gimp_pixel_rgns_register (n_drawables + 1, &srcPR,
                                             &pixrgn[0],
                                             &pixrgn[1],
                                             &pixrgn[2],
                                             &pixrgn[3],
                                             &pixrgn[4]);
    while (tileiterator)
      {
        src = srcPR.data;

        for (counter = 0; counter < n_drawables; ++counter)
          {
            sc->destptr[counter] = pixrgn[counter].data;
            sc->bpp[counter] = pixrgn[counter].bpp;
          }

        separate_core (sc, src, srcPR.w * srcPR.h);

        gimp_progress_update (((double)tilecounter) / ((double)ntiles));

        ++tilecounter;
        tileiterator = gimp_pixel_rgns_process (tileiterator);
      }

    g_free (sc->cmyktemp);
    cmsDeleteTransform (sc->hTransform);

#ifdef ENABLE_COLOR_MANAGEMENT
    embed_cmyk_profile (new_image_id, sc->cmykfilename);
#endif

    duplicate_paths (sc->imageID, new_image_id);

    for (counter = 0; counter < n_drawables; ++counter)
      {
        gimp_drawable_flush (drawables[counter]);
        gimp_drawable_update (drawables[counter]->drawable_id, 0, 0, width, height);
        gimp_drawable_detach (drawables[counter]);
      }
  }
}


void
separate_light (GimpDrawable    *drawable,
                GimpParam       *values,
                SeparateContext *sc)
{
  GimpPixelRgn srcPR;
  gpointer tileiterator;
  gint width, height;
  gint bytes;
  guchar *src;
  gint ntiles = 0, tilecounter = 0;
  gint32 rgbimage = sc->imageID;

  if (!setup_transform (sc))
    {
      values[0].data.d_image = -1;
      return;
    }

  /* Get the size of the input image. (This will/must be the same *
   * as the size of the output image.)                            */
  width  = drawable->width;
  height = drawable->height;
  bytes  = drawable->bpp;

  /*  initialize the pixel regions  */
  gimp_pixel_rgn_init (&srcPR, drawable, 0, 0, width, height, FALSE, FALSE);
  {
    gint32 new_image_id, counter;
    gdouble xres, yres;
    gint n_drawables = 4;
    GimpDrawable *drawables[5];
    GimpPixelRgn pixrgn[5];
    gint32 layers[4];

    enum layerid { LAYER_K, LAYER_Y, LAYER_M, LAYER_C };

    gchar *filename = separate_filename_add_suffix (gimp_image_get_filename (gimp_drawable_get_image (drawable->drawable_id)), "CMYK");

    values[0].data.d_image = new_image_id =
      separate_create_planes_grey (filename, drawable->width, drawable->height, layers);
    g_free (filename);

    gimp_image_get_resolution (rgbimage, &xres, &yres);
    gimp_image_set_resolution (new_image_id, xres, yres);

    gimp_pixel_rgn_init (&srcPR, drawable, 0, 0, width, height, FALSE, FALSE);

    for (counter = 0; counter < 4; ++counter)
      {
        drawables[counter] = gimp_drawable_get (layers[counter]);
        gimp_pixel_rgn_init (&pixrgn[counter], drawables[counter], 0, 0, width, height, TRUE, FALSE);
      }

    if (sc->drawable_has_alpha)
      {
        const GimpRGB color = {1.0, 1.0, 1.0};
        gint32 channel;

        channel = gimp_channel_new (new_image_id, _("Alpha of source image"),
                                    width, height, 100.0, &color);
        gimp_channel_set_show_masked (channel, TRUE);
        gimp_drawable_set_visible (channel, FALSE);
        gimp_image_add_channel (new_image_id, channel, 0);

        drawables[4] = gimp_drawable_get (channel);
        gimp_pixel_rgn_init (&pixrgn[4], drawables[4], 0, 0, width, height, TRUE, FALSE);

        n_drawables++;
      }

    sc->cmyktemp = g_new (guchar, 64 * 64 * 4);

    gimp_progress_init (_("Separating..."));
    ntiles = drawable->ntile_cols * drawable->ntile_rows;
    tileiterator = gimp_pixel_rgns_register (n_drawables + 1, &srcPR,
                                             &pixrgn[0],
                                             &pixrgn[1],
                                             &pixrgn[2],
                                             &pixrgn[3],
                                             &pixrgn[4]);
    while (tileiterator)
      {
        src = srcPR.data;

        for (counter=0; counter < n_drawables; ++counter)
          {
            sc->destptr[counter] = pixrgn[counter].data;
            sc->bpp[counter] = pixrgn[counter].bpp;
          }

        separate_core (sc, src, srcPR.w * srcPR.h);

        gimp_progress_update (((double)tilecounter ) / ((double)ntiles));
        ++tilecounter;
        tileiterator = gimp_pixel_rgns_process (tileiterator);
      }

    g_free (sc->cmyktemp);
    cmsDeleteTransform (sc->hTransform);

#ifdef ENABLE_COLOR_MANAGEMENT
    embed_cmyk_profile (new_image_id, sc->cmykfilename);
#endif

    duplicate_paths (sc->imageID, new_image_id);

    for (counter = 0; counter < n_drawables; ++counter)
      {
        gimp_drawable_flush (drawables[counter]);
        gimp_drawable_update (drawables[counter]->drawable_id, 0, 0, width, height);
        gimp_drawable_detach (drawables[counter]);
      }
  }
}


void
separate_proof (GimpDrawable    *drawable,
                GimpParam       *values,
                SeparateContext *sc)
{
  gpointer tileiterator;
  gint width, height;
  gint bytes;
  gint ntiles=0, tilecounter=0;
  gint32 cmykimage = sc->imageID;

  gint n_drawables = 4;
  GimpDrawable *drawables[6] = {0};

  cmsHPROFILE hInProfile = NULL, hOutProfile;
  cmsHTRANSFORM hTransform;
  gint intent ;
  DWORD dwFLAGS;

  values[0].data.d_image = -1; /* error? */

  if (sc->ps.mode < 0 || sc->ps.mode > 2)
    {
      gimp_message (_("Proofing mode is invalid."));
      return;
    }

  if (!(separate_is_CMYK (cmykimage)))
    {
      gimp_message (_("Image is not separated..."));
      return;
    }

  drawables[1] = separate_find_channel (cmykimage, sep_C);
  drawables[2] = separate_find_channel (cmykimage, sep_M);
  drawables[3] = separate_find_channel (cmykimage, sep_Y);
  drawables[4] = separate_find_channel (cmykimage, sep_K);

  if ((drawables[5] = separate_find_alpha (cmykimage)))
    {
      n_drawables++;
      sc->drawable_has_alpha = TRUE;
    }
  else
    sc->drawable_has_alpha = FALSE;

  {
    int i, n = 0;
    gchar* channel_names[4] = { _( "C" ), _( "M" ), _( "Y" ), _( "K" ) };
    gchar* missing_channels[7];
    for (i = 0; i < 4; i++)
      {
        if (drawables[i + 1] == 0)
          missing_channels[n++] = channel_names[i];
      }
    if (n)
      {
        missing_channels[n] = NULL;
        missing_channels[5] = g_strjoinv (", ", missing_channels);
        missing_channels[6] = g_strdup_printf (_("Couldn't get channel(s) : %s"), missing_channels[5]);
        gimp_message (missing_channels[6]);
        g_free (missing_channels[5]);
        g_free (missing_channels[6]);
        return;
      }
  }

  if (!sc->prooffilename)
    {
      sc->prooffilename = sc->alt_prooffilename;
      sc->alt_prooffilename = NULL;
    }

#ifdef ENABLE_COLOR_MANAGEMENT
  if (sc->ps.profile)
    {
      GimpParasite *parasite = gimp_image_parasite_find (cmykimage, CMYKPROFILE);

      if (parasite)
        {
          hInProfile = cmsOpenProfileFromMem ((gpointer)gimp_parasite_data (parasite),
                                              gimp_parasite_data_size (parasite));
          gimp_parasite_free (parasite);
        }
    }
  if (!hInProfile)
#endif
    if (!(hInProfile = lcms_open_profile (sc->prooffilename)))
      {
        gimp_message (_("Cannot open the CMYK profile."));
        return;
      }

  if (!sc->displayfilename)
    {
      sc->displayfilename = sc->alt_displayfilename;
      sc->alt_displayfilename = NULL;
    }

  hOutProfile  = lcms_open_profile (sc->displayfilename);

  if (!hOutProfile)
    {
      gimp_message (_("Cannot open the display profile."));

      if (hInProfile)
        cmsCloseProfile (hInProfile);
      //values[0].data.d_image = -1;
      return;
    }
  if (sc->ps.mode == 2)
    { /* Simulate media white */
      cmsCIEXYZ whitePoint;
      cmsCIExyY wp_xyY;
      LPcmsCIExyY D50_xyY = cmsD50_xyY();

      intent = INTENT_ABSOLUTE_COLORIMETRIC;
      dwFLAGS = cmsFLAGS_NOWHITEONWHITEFIXUP;

      whitePoint = lcms_get_whitepoint (hOutProfile);
      cmsXYZ2xyY (&wp_xyY, &whitePoint);
      cmsSetAdaptationState ((pow (fabs (wp_xyY.x - D50_xyY->x), 2) +
                             pow (fabs (wp_xyY.y - D50_xyY->y), 2) > 0.000005) ? 1.0 : 0);
      //cmsSetAdaptationState ((cmsIsTag (hOutProfile, 0x63686164L/*'chad'*/) == FALSE) ? 1.0 : 0);
    }
  else
    { /* Others */
      intent = sc->ps.mode;
      dwFLAGS = 0;

      cmsSetAdaptationState (0);
    }
  hTransform = cmsCreateTransform (hInProfile,  sc->drawable_has_alpha ? TYPE_CMYKA_8 : TYPE_CMYK_8,
                                   hOutProfile, sc->drawable_has_alpha ? TYPE_RGBA_8 : TYPE_RGB_8,
                                   intent,
                                   dwFLAGS);
  if (!hTransform)
    {
      gimp_message (_("Cannot build transform.\nThere might be an error in the specification of the profile."));
      if (hInProfile)
        cmsCloseProfile (hInProfile);
      if (hOutProfile)
        cmsCloseProfile (hOutProfile);
      //values[0].data.d_image = -1;
      return;
    }

  /* Get the size of the input image. (This will/must be the same
   *  as the size of the output image.
   */
  width  = drawable->width;
  height = drawable->height;
  bytes  = 1;

  {
    gint32 new_image_id, counter;
    gdouble xres, yres;
    GimpPixelRgn pixrgn[6] = { {0}, {0}, {0}, {0}, {0}, {0} };
    gint32 layers[1];
    gint srcbpp = n_drawables;
    guchar *cmyktemp;

    char *filename = separate_filename_add_suffix (gimp_image_get_filename (cmykimage), "Proof"); 
    values[0].data.d_image = new_image_id =
      separate_create_RGB (filename, drawable->width, drawable->height, sc->drawable_has_alpha, layers);
    g_free (filename);

    gimp_image_get_resolution (cmykimage, &xres, &yres);
    gimp_image_set_resolution (new_image_id, xres, yres);

    drawables[0] = gimp_drawable_get (layers[0]);

    for (counter = 1; counter <= n_drawables; counter++)
      gimp_pixel_rgn_init (&pixrgn[counter], drawables[counter], 0, 0, width, height, FALSE, FALSE);

    gimp_pixel_rgn_init (&pixrgn[0], drawables[0], 0, 0, width, height, TRUE, FALSE);

    cmyktemp = g_new (guchar, pixrgn[0].w * pixrgn[0].h * srcbpp);

    gimp_progress_init (_("Proofing..."));
    ntiles = drawables[0]->ntile_cols * drawables[0]->ntile_rows;
    tileiterator = gimp_pixel_rgns_register (n_drawables + 1, &pixrgn[0],
                                             &pixrgn[1],   /* C */
                                             &pixrgn[2],   /* M */
                                             &pixrgn[3],   /* Y */
                                             &pixrgn[4],   /* K */
                                             &pixrgn[5]);  /* A */

    while (tileiterator)
      {
        long i;
        guchar *ptr[6];

        for (counter = 0; counter <= n_drawables; ++counter)
          ptr[counter] = pixrgn[counter].data;

        for (i = 0; i < (pixrgn[0].w * pixrgn[0].h); i++)
          {
            cmyktemp[i * srcbpp]     = (ptr[1])[i * pixrgn[1].bpp];
            cmyktemp[i * srcbpp + 1] = (ptr[2])[i * pixrgn[2].bpp];
            cmyktemp[i * srcbpp + 2] = (ptr[3])[i * pixrgn[3].bpp];
            cmyktemp[i * srcbpp + 3] = (ptr[4])[i * pixrgn[4].bpp];

            if (sc->drawable_has_alpha)
              (ptr[0])[i * pixrgn[0].bpp + 3] = (ptr[5])[i * pixrgn[5].bpp];
          }

        cmsDoTransform (hTransform,
                        cmyktemp,
                        ptr[0],
                        pixrgn[0].w * pixrgn[0].h);

        gimp_progress_update (((double)tilecounter) / ((double)ntiles));
        ++tilecounter;
        tileiterator = gimp_pixel_rgns_process (tileiterator);
      }

    g_free (cmyktemp);
    cmsDeleteTransform (hTransform);
    cmsCloseProfile (hInProfile);
    cmsCloseProfile( hOutProfile );

#ifdef ENABLE_COLOR_MANAGEMENT
    {
      /* embed destination profile for correct preview */
      gint num_matches = 0;
      gchar **proc_names;
      gimp_procedural_db_query ("^plug-in-icc-profile-set$",
                                ".*", ".*", ".*", ".*", ".*", ".*",
                                &num_matches,
                                &proc_names);
      if (num_matches)
        {
          GimpParam *return_vals;
          gint i, nreturn_vals;

          for (i = 0; i < num_matches; i++)
            g_free (proc_names[i]);
          g_free (proc_names );

          return_vals = gimp_run_procedure ("plug-in-icc-profile-set",
                                            &nreturn_vals,
                                            GIMP_PDB_INT32,
                                            GIMP_RUN_NONINTERACTIVE,
                                            GIMP_PDB_IMAGE,
                                            new_image_id,
                                            GIMP_PDB_STRING,
                                            sc->displayfilename,
                                            GIMP_PDB_END);

          gimp_destroy_params (return_vals, nreturn_vals);
        }
      else
        {
          gsize length;
          gchar *buf = NULL;
          if (g_file_get_contents (sc->displayfilename, &buf, &length, NULL))
            {
              GimpParasite *parasite;

              /* Profile is embedded, and cannot be used independently */
              buf[47] |= 2;

              parasite = gimp_parasite_new ("icc-profile", 0, length, buf);
              gimp_image_parasite_attach (new_image_id, parasite);
              gimp_parasite_free (parasite);

              g_free (buf);
            }
        }
    }
#endif

    gimp_drawable_flush (drawables[4]);
    gimp_drawable_update (drawables[4]->drawable_id, 0, 0, width, height);
    gimp_drawable_detach (drawables[4]);
  }

}


void
separate_duotone (GimpDrawable    *drawable,
                  GimpParam       *values,
                  SeparateContext *sc)
{
  GimpPixelRgn srcPR;
  gpointer tileiterator;
  gint width, height;
  guchar *src;

  width  = drawable->width;
  height = drawable->height;

  {
    gint32 new_image_id, counter;
    gint n_drawables = 2;
    GimpDrawable *drawables[3];
    GimpPixelRgn pixrgn[3];
    gint32 layers[2];
    gint32 mask[2];
    gint32 ntiles = 0, tilecounter = 0;

    gchar *filename = separate_filename_add_suffix (gimp_image_get_filename (sc->imageID), "MK"); 

    values[0].data.d_image = new_image_id =
      separate_create_planes_Duotone (filename, drawable->width, drawable->height, layers);
    g_free (filename);

    for (counter = 0; counter < 2; ++counter)
      {
        mask[counter] = gimp_layer_create_mask (layers[counter], GIMP_ADD_WHITE_MASK);
        gimp_layer_add_mask (layers[counter], mask[counter]);
        drawables[counter] = gimp_drawable_get (mask[counter]);
      }

    if (gimp_drawable_has_alpha (drawable->drawable_id))
      {
        const GimpRGB color = {1.0, 1.0, 1.0};
        gint32 channel;

        channel = gimp_channel_new (new_image_id, _("Alpha of source image"),
                                    width, height, 100.0, &color);
        gimp_channel_set_show_masked (channel, TRUE);
        gimp_drawable_set_visible (channel, TRUE);
        gimp_image_add_channel (new_image_id, channel, 0);

        drawables[2] = gimp_drawable_get (channel);

        n_drawables++;
      }

    gimp_pixel_rgn_init (&srcPR, drawable, 0, 0, width, height, FALSE, FALSE);

    for (counter = 0; counter < n_drawables; ++counter)
      gimp_pixel_rgn_init (&pixrgn[counter], drawables[counter], 0, 0, width, height, TRUE, FALSE);

    gimp_progress_init (_("Separating..."));
    ntiles = drawable->ntile_rows * drawable->ntile_cols;
    tileiterator = gimp_pixel_rgns_register (n_drawables + 1, &srcPR,
                                             &pixrgn[0],
                                             &pixrgn[1],
                                             &pixrgn[2]);
    while (tileiterator)
      {
        long i;
        guchar *destptr[3];
        src = srcPR.data;

        for (counter = 0; counter < n_drawables; ++counter)
          destptr[counter] = pixrgn[counter].data;

        for (i = 0; i < (srcPR.w * srcPR.h); ++i)
          {
            int r, g, b, t;
            r = *src++;
            g = *src++;
            b = *src++;
            t = (g + b) / 2;

            if (r > t)
              g = b = t;
            else
              r = g = (r + g + b) / 3;

            (destptr[0])[i * pixrgn[0].bpp] = 255 - r;
            (destptr[1])[i * pixrgn[1].bpp] = r - g;
            if (n_drawables > 2)
              (destptr[2])[i * pixrgn[2].bpp] = *src++;
          }

        gimp_progress_update (((double) tilecounter) / ((double) ntiles));

        ++tilecounter;
        tileiterator = gimp_pixel_rgns_process (tileiterator);
      }

    duplicate_paths (sc->imageID, new_image_id);

    for (counter=0; counter < n_drawables; ++counter)
      {
        gimp_drawable_flush (drawables[counter]);
        gimp_drawable_update (drawables[counter]->drawable_id, 0, 0, width, height);
        gimp_drawable_detach (drawables[counter]);
      }
  }
}
