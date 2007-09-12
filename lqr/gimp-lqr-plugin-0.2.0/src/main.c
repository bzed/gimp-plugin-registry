/* GIMP LiquidRescaling Plug-in
 * Copyright (C) 2007 Carlo Baldassi (the "Author") <carlobaldassi@yahoo.it>.
 * (implementation based on the GIMP Plug-in Template by Michael Natterer)
 * All Rights Reserved.
 *
 * This plug-in implements the algorithm described in the paper
 * "Seam Carving for Content-Aware Image Resizing"
 * by Shai Avidan and Ariel Shamir
 * which can be found at http://www.faculty.idc.ac.il/arik/imret.pdf
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 dated June, 1991.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */


#include "config.h"

#include <string.h>

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "plugin-intl.h"

#include "main.h"
#include "interface.h"
#include "render.h"



/*  Constants  */

#define PROCEDURE_NAME   "lqr_plugin"

#define DATA_KEY_VALS    "lqr_plug_in"
#define DATA_KEY_UI_VALS "lqr_plug_in_ui"

#define PARASITE_KEY     "lqr_plug-in-options"


/*  Local function prototypes  */

static void query (void);
static void run (const gchar * name,
                 gint nparams,
                 const GimpParam * param,
                 gint * nreturn_vals, GimpParam ** return_vals);


/*  Local variables  */

const PlugInVals default_vals = {
  100,
  100,
  0,
  100,
  0,
  100,
  LQR_GF_SUMABS,
  FALSE,
  TRUE,
  GIMP_MASK_APPLY
};

const PlugInImageVals default_image_vals = {
  0
};

const PlugInDrawableVals default_drawable_vals = {
  0,
  FALSE,
  FALSE
};

const PlugInUIVals default_ui_vals = {
  FALSE
};

static PlugInVals vals;
static PlugInImageVals image_vals;
static PlugInDrawableVals drawable_vals;
static PlugInUIVals ui_vals;


GimpPlugInInfo PLUG_IN_INFO = {
  NULL,                         /* init_proc  */
  NULL,                         /* quit_proc  */
  query,                        /* query_proc */
  run,                          /* run_proc   */
};

MAIN ()

     static void query (void)
{
  gchar *help_path;
  gchar *help_uri;

  static GimpParamDef args[] = {
    {GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive"},
    {GIMP_PDB_IMAGE, "image", "Input image"},
    {GIMP_PDB_DRAWABLE, "drawable", "Input drawable"},
    {GIMP_PDB_INT32, "width", "Final width"},
    {GIMP_PDB_INT32, "height", "Final height"},
    {GIMP_PDB_INT32, "pres_layer", "Layer that marks preserved areas"},
    {GIMP_PDB_INT32, "pres_coeff", "Preservation coefficient"},
    {GIMP_PDB_INT32, "disc_layer", "Layer that marks areas to discard"},
    {GIMP_PDB_INT32, "disc_coeff", "Discard coefficient"},
    {GIMP_PDB_INT32, "grad_func", "Gradient function to use"},
    {GIMP_PDB_INT32, "update_en", "Wether to update energy map"},
    {GIMP_PDB_INT32, "resize_canvas", "Wether to resize canvas"},
    {GIMP_PDB_INT32, "mask_behavior", "What to do with masks"}
  };

  gimp_plugin_domain_register (PLUGIN_NAME, LOCALEDIR);

  help_path = g_build_filename (DATADIR, "help", NULL);
  help_uri = g_filename_to_uri (help_path, NULL, NULL);
  g_free (help_path);

  gimp_plugin_help_register ("http://developer.gimp.org/lqr-plug-in/help",
                             help_uri);

  gimp_install_procedure (PROCEDURE_NAME, _("LiquidRescaling (content-aware rescaling)"), "Set width and height and you're done", "Carlo Baldassi <carlobaldassi@yahoo.it>", "Carlo Baldassi <carlobaldassi@yahoo.it>", "2007", N_("Liquid rescale ..."), "RGB*, GRAY*",        /* what about INDEXED* images ? */
                          GIMP_PLUGIN, G_N_ELEMENTS (args), 0, args, NULL);

  gimp_plugin_menu_register (PROCEDURE_NAME, "<Image>/Layer/");
}

static void
run (const gchar * name,
     gint n_params,
     const GimpParam * param, gint * nreturn_vals, GimpParam ** return_vals)
{
  static GimpParam values[1];
  GimpDrawable *drawable;
  gint32 image_ID;
  GimpRunMode run_mode;
  GimpPDBStatusType status = GIMP_PDB_SUCCESS;
  gint x1, y1, x2, y2;

  *nreturn_vals = 1;
  *return_vals = values;

  /*  Initialize i18n support  */
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif
  textdomain (GETTEXT_PACKAGE);

  run_mode = param[0].data.d_int32;
  image_ID = param[1].data.d_int32;
  drawable = gimp_drawable_get (param[2].data.d_drawable);

  /*  Initialize with default values  */
  vals = default_vals;
  image_vals = default_image_vals;
  drawable_vals = default_drawable_vals;
  ui_vals = default_ui_vals;

  vals.new_width = gimp_image_width (image_ID);
  vals.new_height = gimp_image_height (image_ID);

  gimp_drawable_mask_bounds (drawable->drawable_id, &x1, &y1, &x2, &y2);

  gimp_image_undo_group_start (image_ID);
  if (strcmp (name, PROCEDURE_NAME) == 0)
    {
      switch (run_mode)
        {
        case GIMP_RUN_NONINTERACTIVE:
          if (n_params != 9)
            {
              status = GIMP_PDB_CALLING_ERROR;
            }
          else
            {
              vals.new_width = param[3].data.d_int32;
              vals.new_height = param[4].data.d_int32;
              vals.grad_func = param[5].data.d_int32;
              vals.update_en = param[6].data.d_int32;
              vals.resize_canvas = param[7].data.d_int32;
              vals.mask_behavior = param[8].data.d_int32;
            }
          break;

        case GIMP_RUN_INTERACTIVE:
          /*  Possibly retrieve data  */
          gimp_get_data (DATA_KEY_VALS, &vals);
          gimp_get_data (DATA_KEY_UI_VALS, &ui_vals);

          if (!dialog (image_ID, drawable,
                       &vals, &image_vals, &drawable_vals, &ui_vals))
            {
              status = GIMP_PDB_CANCEL;
            }
          break;

        case GIMP_RUN_WITH_LAST_VALS:
          /*  Possibly retrieve data  */
          gimp_get_data (DATA_KEY_VALS, &vals);

          break;

        default:
          break;
        }
    }
  else
    {
      status = GIMP_PDB_CALLING_ERROR;
    }

  if (status == GIMP_PDB_SUCCESS)
    {
      if ((vals.pres_layer_ID == -1) || (drawable_vals.pres_status == FALSE)) {
	      vals.pres_layer_ID = 0;
      }
      if ((vals.disc_layer_ID == -1) || (drawable_vals.disc_status == FALSE)) {
	      vals.disc_layer_ID = 0;
      }
      render (image_ID, drawable, &vals, &image_vals, &drawable_vals);

      if (run_mode != GIMP_RUN_NONINTERACTIVE)
        gimp_displays_flush ();

      if (run_mode == GIMP_RUN_INTERACTIVE)
        {
          gimp_set_data (DATA_KEY_VALS, &vals, sizeof (vals));
          gimp_set_data (DATA_KEY_UI_VALS, &ui_vals, sizeof (ui_vals));
        }

      drawable = gimp_drawable_get (gimp_image_get_active_drawable (image_ID));
      gimp_drawable_detach (drawable);
    }

  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = status;

  gimp_image_undo_group_end (image_ID);
}
