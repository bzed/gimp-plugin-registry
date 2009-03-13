/* 
 * Wavelet denoise GIMP plugin
 * 
 * plugin.c
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

GimpPlugInInfo PLUG_IN_INFO = { NULL, NULL, query, run };

wavelet_settings settings = {
  {0, 0},			/* gray_thresholds */
  {0.0, 0.0},			/* gray_low */
  {0, 0, 0, 0},			/* colour_thresholds */
  {0.0, 0.0, 0.0, 0.0},		/* colour_low */
  MODE_YCBCR,			/* colour_mode */
  0,				/* preview_channel */
  1,				/* preview_mode */
  TRUE,				/* preview */
  {1.1, 4.5, 1.4},		/* times */
  0, 0				/* winxsize, winysize */
};

char *names_ycbcr[] = { "Y", "Cb", "Cr", N_("Alpha") };
char *names_rgb[] = { "R", "G", "B", N_("Alpha") };
char *names_gray[] = { "Gray", N_("Alpha") };
char *names_lab[] = { "L*", "a*", "b*", N_("Alpha") };

MAIN ()
     void query (void)
{
  static GimpParamDef args[] = {
    /* TRANSLATORS: This is a plugin argument for scripts */
    {GIMP_PDB_INT32, "run-mode", "Run mode"},
    /* TRANSLATORS: This is a plugin argument for scripts */
    {GIMP_PDB_IMAGE, "image", "Input image"},
    /* TRANSLATORS: This is a plugin argument for scripts */
    {GIMP_PDB_DRAWABLE, "drawable", "Input drawable"}
  };

  gimp_install_procedure ("plug-in-wavelet-denoise",
			  _("Removes noise in the image using wavelets."),
			  PLUGIN_HELP,
			  _("Marco Rossini"),
			  _("Copyright 2008 Marco Rossini"),
			  "2008",
			  /* TRANSLATORS: Menu entry of the plugin. Use under-
			     score for identifying hotkey */
			  N_("_Wavelet denoise ..."),
			  "RGB*, GRAY*",
			  GIMP_PLUGIN, G_N_ELEMENTS (args), 0, args, NULL);

  gimp_plugin_domain_register("gimp20-wavelet-denoise-plug-in", LOCALEDIR);
  
  gimp_plugin_menu_register ("plug-in-wavelet-denoise",
			     "<Image>/Filters/Enhance");
}

void
run (const gchar * name, gint nparams, const GimpParam * param,
     gint * nreturn_vals, GimpParam ** return_vals)
{
  static GimpParam values[1];
  GimpRunMode run_mode;
  GimpDrawable *drawable;
  gint i;

  bindtextdomain("gimp20-wavelet-denoise-plug-in", LOCALEDIR);
  textdomain("gimp20-wavelet-denoise-plug-in");
  bind_textdomain_codeset("gimp20-wavelet-denoise-plug-in", "UTF-8");

  timer = g_timer_new ();

  /* Setting mandatory output values */
  *nreturn_vals = 1;
  *return_vals = values;
  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = GIMP_PDB_SUCCESS;

  /* restore settings saved in GIMP core */
  gimp_get_data ("plug-in-wavelet-denoise", &settings);

  drawable = gimp_drawable_get (param[2].data.d_drawable);
  channels = gimp_drawable_bpp (drawable->drawable_id);

  if (settings.preview_channel > channels - 1)
    settings.preview_channel = 0;

  /* allocate buffers */
  /* FIXME: replace by GIMP funcitons */
  for (i = 0; i < channels; i++)
    {
      fimg[i] = (float *) malloc (drawable->width * drawable->height
				  * sizeof (float));
    }
  buffer[1] = (float *) malloc (drawable->width * drawable->height
				* sizeof (float));
  buffer[2] = (float *) malloc (drawable->width * drawable->height
				* sizeof (float));

  /* run GUI if in interactiv mode */
  run_mode = param[0].data.d_int32;
  if (run_mode == GIMP_RUN_INTERACTIVE)
    {
      if (!user_interface (drawable))
	{
	  gimp_drawable_detach (drawable);
	  /* FIXME: should return error status here */
	  return;
	}
    }

  denoise (drawable, NULL);

  /* free buffers */
  /* FIXME: replace by GIMP functions */
  for (i = 0; i < channels; i++)
    {
      free (fimg[i]);
    }
  free (buffer[1]);
  free (buffer[2]);

  gimp_displays_flush ();
  gimp_drawable_detach (drawable);

  /* save settings in the GIMP core */
  gimp_set_data ("plug-in-wavelet-denoise", &settings,
		 sizeof (wavelet_settings));
}
