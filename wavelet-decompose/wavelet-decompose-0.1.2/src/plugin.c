/* 
 * Wavelet decompose GIMP plugin
 * 
 * plugin.c
 * Copyright 2008 by Marco Rossini
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2+
 * as published by the Free Software Foundation.
 */

#include "plugin.h"

GimpPlugInInfo PLUG_IN_INFO = { NULL, NULL, query, run };

MAIN ()
     void query (void)
{
  static GimpParamDef args[] = {
    {GIMP_PDB_INT32, "run-mode", "Run mode"},
    {GIMP_PDB_IMAGE, "image", "Input image"},
    {GIMP_PDB_DRAWABLE, "drawable", "Input drawable"},
    {GIMP_PDB_INT8, "scale-num", "Number of wavelet scales to decompose"},
    {GIMP_PDB_INT8, "layer-modes", "Set layer mode to grain-merge (boolean)"},
    {GIMP_PDB_INT8, "alpha", "Force alpha channel (boolean)"},
    {GIMP_PDB_INT8, "new-image", "Create new image (boolean)"}
  };

  gimp_plugin_domain_register (GETTEXT_PKG, LOCALEDIR);

  gimp_install_procedure ("plug-in-wavelet-decompose",
			  N_("Decomposes the image into wavelet scales."),
			  "Decomposes the image into wavelet scales.",
			  "Marco Rossini",
			  "Copyright 2008 Marco Rossini",
			  "2008",
			  N_("_Wavelet decompose ..."),
			  "RGB*, GRAY*",
			  GIMP_PLUGIN, G_N_ELEMENTS (args), 0, args, NULL);

  gimp_plugin_menu_register ("plug-in-wavelet-decompose",
			     "<Image>/Filters/Generic");
}

wavelet_settings settings = {
  5,				/* scales */
  0,				/* new_image */
  0,				/* add_alpha */
  GIMP_GRAIN_MERGE_MODE		/* layer_modes */
};

void
run (const gchar * name, gint nparams, const GimpParam * param,
     gint * nreturn_vals, GimpParam ** return_vals)
{
  static GimpParam values[1];
  GimpRunMode run_mode;
  GimpDrawable *drawable;
  gint32 image;
  unsigned int maxscale, size;

  bindtextdomain (GETTEXT_PKG, LOCALEDIR);
  textdomain (GETTEXT_PKG);
  bind_textdomain_codeset (GETTEXT_PKG, "UTF-8");

  /* Setting mandatory output values */
  *nreturn_vals = 1;
  *return_vals = values;
  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = GIMP_PDB_SUCCESS;

  image = param[1].data.d_image;
  drawable = gimp_drawable_get (param[2].data.d_drawable);

  /* verify things */
  if (!gimp_drawable_is_layer (drawable->drawable_id))
    {
      g_message (_
		 ("Error: The wavelet decompose filter works only on layers!"));
      values[0].data.d_status = GIMP_PDB_CALLING_ERROR;
      return;
    }

  /* smallest edge must be higher than or equal to 2^scales */
  size =
    MIN2 (gimp_drawable_width (drawable->drawable_id),
	  gimp_drawable_height (drawable->drawable_id));
  maxscale = 0;
  while (size >>= 1)
    {
      maxscale++;
    }

  /* run GUI if in interactiv mode */
  run_mode = param[0].data.d_int32;
  if (run_mode == GIMP_RUN_INTERACTIVE)
    {
      /* restore settings saved in GIMP core, only when interactive */
      gimp_get_data ("plug-in-wavelet-decompose", &settings);

      if (settings.scales > maxscale)
	settings.scales = maxscale;

      if (!user_interface (drawable, maxscale))
	{
	  gimp_drawable_detach (drawable);
	  values[0].data.d_status = GIMP_PDB_EXECUTION_ERROR;
	  return;
	}
    }
  else
    {
      settings.scales = param[3].data.d_int8;
      settings.layer_modes =
	(param[4].data.d_int8) ? GIMP_GRAIN_MERGE_MODE : GIMP_NORMAL_MODE;
      settings.add_alpha = !!(param[5].data.d_int8);
      settings.new_image = !!(param[6].data.d_int8);
      if (settings.scales < 1 || settings.scales > maxscale)
	{
	  values[0].data.d_status = GIMP_PDB_CALLING_ERROR;
	  return;
	}
    }

  decompose (image, drawable, settings.scales);

  gimp_displays_flush ();
  gimp_drawable_detach (drawable);

  if (run_mode == GIMP_RUN_INTERACTIVE)
    {
      /* save settings in the GIMP core, only when interactive and on success */
      gimp_set_data ("plug-in-wavelet-decompose", &settings,
		     sizeof (wavelet_settings));
    }
}
