/* CMYK TIFF importer : a part of the Separate+ 
 *
 * Copyright (C) 2007-2010 Yoshinori Yamakawa (yam@yellowmagic.info)
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
#include <fcntl.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include <tiffio.h>

#include "libgimp/stdplugins-intl.h"

#include "separate.h"
#include "platform.h"

/* Declare local functions.
 */
static void query (void);
static void run   (const gchar      *name,
                   gint              nparams,
                   const GimpParam  *param,
                   gint             *nreturn_vals,
                   GimpParam       **return_vals);

static gint        separate_import_dialog (SeparateContext *sc);

GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,  /* init_proc  */
  NULL,  /* quit_proc  */
  query, /* query_proc */
  run,   /* run_proc   */
};

MAIN ()


static void
query (void)
{
  /* setup for localization */
  INIT_I18N ();

  /* Arguments for CMYK Separation routines */

  static GimpParamDef args[] =
  {
    { GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive" },
    { GIMP_PDB_STRING, "filename", "Filename" }
  };
  static gint nargs = sizeof (args) / sizeof (args[0]);
  static GimpParamDef rargs[] =
  {
    { GIMP_PDB_IMAGE, "new_image", "Separated image" }
  };
  static gint nrargs = sizeof (rargs) / sizeof (rargs[0]);

  gimp_install_procedure ("plug_in_separate_import",
                          _("Import CMYK TIFF image"),
                          _("Load the CMYK TIFF image, and generate four layers "
                            "as pseudo C/M/Y/K channels.\nTo convert to single-"
                            "layered RGB image, try to use the \"plug-in-separate"
                            "-proof\" procedure."),
                          "Yoshinori Yamakawa",
                          "Yoshinori Yamakawa",
                          "2007-2010",
                          N_("From CMYK TIFF"),
                          NULL,
                          GIMP_PLUGIN,
                          nargs, nrargs,
                          args, rargs );

  gimp_plugin_menu_register ("plug_in_separate_import",
                             gimp_major_version > 2 ||
                             (gimp_major_version == 2 && gimp_minor_version >= 6) ?
                             "<Image>/File/Create/" : "<Toolbox>/File/Acquire");

  gimp_plugin_domain_register (GETTEXT_PACKAGE, NULL);
}


static void run ( const gchar *name, gint nparams, const GimpParam *param,
                  gint *nreturn_vals, GimpParam **return_vals)
{
  static GimpParam values[3];
  GimpRunMode run_mode;
  GimpPDBStatusType status = GIMP_PDB_SUCCESS;
  SeparateContext mysc;
  //enum separate_function func = SEP_NONE;
  run_mode = param[0].data.d_int32;


  /* setup for localization */
  INIT_I18N ();

  mysc.filename = NULL;
  if (nparams != (run_mode == GIMP_RUN_NONINTERACTIVE ? 2 : 1))
    status = GIMP_PDB_CALLING_ERROR;
  else if (run_mode == GIMP_RUN_NONINTERACTIVE)
    {
      if (param[1].type != GIMP_PDB_STRING || strlen (param[1].data.d_string) == 0)
        status = GIMP_PDB_CALLING_ERROR;
      else
        mysc.filename = g_strdup (param[1].data.d_string);
    }
  else
    {
      gint size = gimp_get_data_size ("plug-in-separate-import/lastpath");

      if (size)
        {
          mysc.filename = g_malloc (size);
          gimp_get_data ("plug-in-separate-import/lastpath", mysc.filename);
        }
    }

  if (status == GIMP_PDB_SUCCESS && (run_mode == GIMP_RUN_NONINTERACTIVE || separate_import_dialog (&mysc)))
    {
      gint i, j, x, y;
      TIFF *in;
      guint32 width, height, stripSize, stripCount, stripHeight;
      gint16 bps, spp, step, planerConfig, photometric, inkset, resolutionUnit;
      gint n_extra_samples = 0;
      guint16 *extra_samples = NULL;
      gboolean has_alpha = FALSE;
      float xres, yres;
      const gchar *layerNames[] = { "C", "M", "Y", "K" };
      guchar *buf, *maskbuf[5], *srcbuf, *destbuf[5], *iccProfile;
      gint32 layers[5], masks[4];
      gint n_drawables;
      GimpDrawable *drw[5];
      GimpPixelRgn rgn[5];
      GimpRGB primaries[4] = { { .180, .541, .870, 1.0 },
                               { .925, .149, .388, 1.0 },
                               { .929, .862, .129, 1.0 },
                               { 0, 0, 0, 1.0 }  };

      gchar *str = NULL;
      gchar *baseName = g_path_get_basename (gimp_filename_to_utf8 (mysc.filename));

#ifdef G_OS_WIN32
      {
        gchar *_filename = NULL; // win32 filename encoding(not UTF-8)
        _filename = g_win32_locale_filename_from_utf8 (mysc.filename);
        in = TIFFOpen (_filename ? _filename : mysc.filename, "r");
        g_free (_filename);
      }
#else
      in = TIFFOpen (mysc.filename, "r");
#endif

    if (!in)
      {
        str = g_strdup_printf (_("Cannot open : \"%s\""), baseName);
        gimp_message (str);
        g_free (str);
        status = GIMP_PDB_EXECUTION_ERROR;
      }
    else
      {
        TIFFGetField (in, TIFFTAG_EXTRASAMPLES, &n_extra_samples, &extra_samples);

        if ((TIFFGetField (in, TIFFTAG_BITSPERSAMPLE, &bps) == FALSE || (bps != 8 && bps != 16)) ||
            (TIFFGetField (in, TIFFTAG_SAMPLESPERPIXEL, &spp) == FALSE || spp - n_extra_samples != 4) ||
            (n_extra_samples > 1 || (n_extra_samples == 1 && extra_samples[0] != EXTRASAMPLE_ASSOCALPHA)) ||
            (TIFFGetField (in, TIFFTAG_PHOTOMETRIC, &photometric) == FALSE || photometric != PHOTOMETRIC_SEPARATED) ||
            (TIFFGetField (in, TIFFTAG_PLANARCONFIG, &planerConfig) == FALSE || planerConfig != PLANARCONFIG_CONTIG) ||
            (TIFFGetField (in, TIFFTAG_INKSET, &inkset) == TRUE && inkset != INKSET_CMYK))
          {
            str = g_strdup_printf (_("\"%s\" is unsupported."), baseName);
            gimp_message (str);
            g_free (str);
            status = GIMP_PDB_EXECUTION_ERROR;
          }
        else
          {
            has_alpha = n_extra_samples && extra_samples[0] == EXTRASAMPLE_ASSOCALPHA;
            n_drawables = 4 + n_extra_samples;
            stripCount = TIFFNumberOfStrips (in);
            stripSize = TIFFStripSize (in);
            TIFFGetField (in, TIFFTAG_IMAGEWIDTH, &width);
            TIFFGetField (in, TIFFTAG_IMAGELENGTH, &height);
            TIFFGetField (in, TIFFTAG_ROWSPERSTRIP, &stripHeight);
            TIFFGetField (in, TIFFTAG_RESOLUTIONUNIT, &resolutionUnit);
            TIFFGetField (in, TIFFTAG_XRESOLUTION, &xres);
            TIFFGetField (in, TIFFTAG_YRESOLUTION, &yres);

#if 0
            str = g_strdup_printf ("Photometric : %d  BPS : %d  SPP : %d\nInkset : %d  StripCount : %d  drw : %d",
                                   photometric, bps, spp, inkset, stripCount, n_drawables);
            gimp_message (str);
            g_free (str);
#endif

            step = (bps == 16) ? 2 : 1;

            buf = g_malloc (stripSize);

            values[1].data.d_image = gimp_image_new (width, height, GIMP_RGB);
            gimp_image_set_resolution (values[1].data.d_image, xres, yres);
            gimp_context_push ();

            /* setup for layers */
            for (i = 0; i < 4; i++)
              {
                layers[i] = gimp_layer_new (values[1].data.d_image, layerNames[i], width, height, GIMP_RGBA_IMAGE, 100.0, GIMP_DARKEN_ONLY_MODE);
                gimp_context_set_foreground (&primaries[i]);
                gimp_drawable_fill (layers[i], GIMP_FOREGROUND_FILL);
                gimp_image_add_layer (values[1].data.d_image, layers[i], i);
                masks[i] = gimp_layer_create_mask (layers[i], GIMP_ADD_BLACK_MASK);
                gimp_layer_add_mask (layers[i], masks[i]);
                drw[i] = gimp_drawable_get (masks[i]);
                maskbuf[i] = g_malloc (width * stripHeight);
              }

            gimp_context_pop ();
            layers[4] = gimp_layer_new (values[1].data.d_image, _("Background"), width, height, GIMP_RGB_IMAGE, 100.0, GIMP_NORMAL_MODE);
            gimp_drawable_fill (layers[4], GIMP_WHITE_FILL);
            gimp_image_add_layer (values[1].data.d_image, layers[4], 4);

            /* setup for (alpha) channels */
            for (i = 0; i < n_extra_samples; i++)
              {
                const GimpRGB color = {1.0, 1.0, 1.0};
                gint32 channel;

                channel = gimp_channel_new (values[1].data.d_image, _("Alpha of source image"),
                                                   width, height, 100.0, &color);
                gimp_channel_set_show_masked (channel, TRUE);
                gimp_drawable_set_visible (channel, TRUE);
                gimp_image_add_channel (values[1].data.d_image, channel, 0);

                drw[4 + i] = gimp_drawable_get (channel);
                maskbuf[4 + i] = g_malloc (width * stripHeight);
              }

            str = g_strdup_printf (_("Reading \"%s\"..."), baseName);
            gimp_progress_init (str);
            g_free (str);

            for (i = 0; i < stripCount; i++)
              {
                guint32 size = TIFFReadEncodedStrip (in, i, buf, stripSize);
                guint32 rowCount = (size < stripSize ? height % stripHeight : stripHeight);
                srcbuf = buf;

                if (bps == 16)
                  srcbuf++;

                for (j = 0; j < n_drawables; j++)
                  {
                    gimp_pixel_rgn_init (&rgn[j], drw[j],
                                         0, stripHeight * i, width, rowCount,
                                         FALSE, FALSE);
                    destbuf[j] = maskbuf[j];
                  }

                for (y = 0; y < rowCount; y++)
                  {
                    for (x = 0; x < width; x++)
                      {
                        if (has_alpha)
                          {
                            guint a;

                            for (j = 0; j < n_drawables; j++)
                              {
                                *destbuf[j] = *srcbuf;
                                srcbuf += step;
                              }

                            if ((a = *destbuf[4]))
                              {
                                for (j = 0; j < 4; j++)
                                  *destbuf[j] = (guint)*destbuf[j] * 255 / a;
                              }

                            for (j = 0; j < n_drawables; j++)
                              destbuf[j]++;
                          }
                        else
                          for (j = 0; j < n_drawables; j++)
                            {
                              *destbuf[j]++ = *srcbuf;
                              srcbuf += step;
                            }
                      }
                  }

                for (j = 0; j < n_drawables; j++)
                  gimp_pixel_rgn_set_rect (&rgn[j], maskbuf[j], 0, stripHeight * i, width, rowCount);

                gimp_progress_update ((gdouble)i / stripCount);
              }

            g_free (buf);

            for (i = 0; i < n_drawables; i++)
              {
                g_free (maskbuf[i]);
                gimp_drawable_detach (drw[i]);
              }

#ifdef ENABLE_COLOR_MANAGEMENT
            if (TIFFGetField (in, TIFFTAG_ICCPROFILE, &width, &iccProfile))
              {
                GimpParasite *parasite;

                parasite = gimp_parasite_new (CMYKPROFILE, 0, width, iccProfile);
                gimp_image_parasite_attach (values[1].data.d_image, parasite);
                gimp_parasite_free (parasite);

                //g_free( iccProfile ); // This causes clash on TIFFClose( in ).
              }
#endif
          }

        TIFFClose (in);
      }

      g_free (baseName);
    }
  else
    status = GIMP_PDB_CANCEL;

  *return_vals = values;
  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = status;

  if (status == GIMP_PDB_SUCCESS)
    {
      *nreturn_vals = 2;
      values[1].type = GIMP_PDB_IMAGE;

      if (run_mode != GIMP_RUN_NONINTERACTIVE)
        {
          gimp_image_undo_enable (values[1].data.d_image);
          gimp_display_new (values[1].data.d_image);
          gimp_displays_flush ();
        }

      gimp_set_data ("plug-in-separate-import/lastpath", mysc.filename, strlen (mysc.filename) + 1);
    }
  else
    *nreturn_vals = 1;

  g_free (mysc.filename);
}


static gint
separate_import_dialog (SeparateContext *sc)
{
  GtkFileFilter *filter;

  sc->dialogresult = FALSE;
  gimp_ui_init ("separate_import", FALSE);

  sc->filenamefileselector = gtk_file_chooser_dialog_new (_("Import separated TIFF..."),
                                                          NULL,
                                                          GTK_FILE_CHOOSER_ACTION_OPEN,
                                                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                                          GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                                          NULL);
  gimp_window_set_transient (GTK_WINDOW (sc->filenamefileselector));
  gtk_dialog_set_alternative_button_order (GTK_DIALOG (sc->filenamefileselector),
                                           GTK_RESPONSE_ACCEPT,
                                           GTK_RESPONSE_CANCEL,
                                           -1);
  gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (sc->filenamefileselector), sc->filename);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("All files"));
  gtk_file_filter_add_pattern (filter, "*");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (sc->filenamefileselector), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("CMYK TIFF image"));
  gtk_file_filter_add_pattern (filter, "*.tif");
  gtk_file_filter_add_pattern (filter, "*.tiff");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (sc->filenamefileselector), filter);
  gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (sc->filenamefileselector), filter);

  sc->dialogresult = gtk_dialog_run (GTK_DIALOG (sc->filenamefileselector));

  if (sc->dialogresult == GTK_RESPONSE_ACCEPT)
    {
      g_free (sc->filename);
      sc->filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (sc->filenamefileselector));
      sc->dialogresult = TRUE;
    }
  else
    {
      g_free (sc->filename);
      sc->filename = NULL;
      sc->dialogresult = FALSE;
    }

  gtk_widget_destroy (sc->filenamefileselector);
  return sc->dialogresult;
}
