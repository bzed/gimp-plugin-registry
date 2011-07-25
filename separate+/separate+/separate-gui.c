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

#include <gtk/gtk.h>

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "libgimp/stdplugins-intl.h"

#include "platform.h"

#include "separate.h"
#include "separate-core.h"
#include "separate-export.h"
#include "icon.h"
#include "util.h"

#include "iccbutton.h"



/* Declare local functions.
 */
static void      query              (void);
static void      run                (const gchar      *name,
                                     gint              nparams,
                                     const GimpParam  *param,
                                     gint             *nreturn_vals,
                                     GimpParam       **return_vals);

static void      callback_preserve_black_toggled (GtkWidget *toggleButton,
                                                  gpointer   data);

static gint      separate_dialog      (SeparateContext *sc);
static gint      proof_dialog         (SeparateContext *sc);
static gint      separate_save_dialog (SeparateContext *sc);


GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,  /* init_proc  */
  NULL,  /* quit_proc  */
  query, /* query_proc */
  run,   /* run_proc   */
};

/* Arguments */
static const GimpParamDef separate_args[] =
{
  { GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive" },
  { GIMP_PDB_IMAGE, "image", "Input image" },
  { GIMP_PDB_DRAWABLE, "drawable", "Input drawable" },
  { GIMP_PDB_STRING, "input_profile", "Input ICC profile" },
  { GIMP_PDB_INT32, "use_embedded_profile", "Use embedded source profile if possible (TRUE, FALSE)" },
  { GIMP_PDB_STRING, "output_profile", "Output ICC profile" },
  { GIMP_PDB_INT32, "rendering_intent", "Rendering intent (0-3)" },
  { GIMP_PDB_INT32, "use_bpc", "Use BPC algorithm (TRUE, FALSE)" },
  { GIMP_PDB_INT32, "preserve_black", "Preserve pure black (TRUE, FALSE)" },
  { GIMP_PDB_INT32, "overprint_black", "Overprint pure black (TRUE, FALSE)" },
  { GIMP_PDB_INT32, "use_dither", "Use dither (TRUE, FALSE)" },
  { GIMP_PDB_INT32, "pseudo_composite", "Make CMYK pseudo-composite (TRUE, FALSE)" }
};

static const GimpParamDef proof_args[] =
{
  { GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive" },
  { GIMP_PDB_IMAGE, "image", "Input image" },
  { GIMP_PDB_DRAWABLE, "drawable", "Input drawable" },
  { GIMP_PDB_STRING, "display_profile", "Monitor profile" },
  { GIMP_PDB_STRING, "proofing_profile", "Proofing profile" },
#ifdef ENABLE_COLOR_MANAGEMENT
  { GIMP_PDB_INT32, "use_attached_profile", "Use attached proofing profile if possible (TRUE, FALSE)" },
#endif
  { GIMP_PDB_INT32, "mode", "0:Normal, 1:Black ink simulation, 2:Media white simulation" }
};

static const GimpParamDef exportargs[] =
{
  { GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive" },
  { GIMP_PDB_IMAGE, "image", "Input image" },
  { GIMP_PDB_DRAWABLE, "drawable", "Input drawable" },
  { GIMP_PDB_STRING, "filename", "Filename" },
#ifdef ENABLE_COLOR_MANAGEMENT
  { GIMP_PDB_INT32, "embed_profile", "0:None, 1:CMYK profile, 2:Print simulation profile, 3:Own profile" },
#endif
  { GIMP_PDB_INT32, "filetype", "-1:Auto, 1:TIFF" },
  { GIMP_PDB_INT32, "compression", "Compress pixel data if available (TRUE, FALSE)" },
  { GIMP_PDB_VECTORS, "vectors", "Clipping path or -1" }
};

static GimpParamDef duotone_args[] =
{
  { GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive" },
  { GIMP_PDB_IMAGE, "image", "Input image" },
  { GIMP_PDB_DRAWABLE, "drawable", "Input drawable" },
};

static gint n_separate_args;
static gint n_proof_args;
static gint n_export_args;
static gint n_duotone_args;


MAIN ()


static void
query (void)
{
  /* setup for localization */
  INIT_I18N ();

  n_separate_args = sizeof (separate_args) / sizeof (separate_args[0]);
  n_proof_args = sizeof (proof_args) / sizeof (proof_args[0]);
  n_export_args = sizeof(exportargs) / sizeof (exportargs[0]);
  n_duotone_args = sizeof (duotone_args) / sizeof (duotone_args[0]);

  static GimpParamDef return_vals[] =
  {
    { GIMP_PDB_IMAGE, "new_image", "Separated image" }
  };
  static gint n_return_vals = sizeof (return_vals) / sizeof (return_vals[0]);

  static GimpParamDef proof_return_vals[] =
  {
    { GIMP_PDB_IMAGE, "new_image", "Proof image" }
  };
  static gint n_proof_return_vals = sizeof (proof_return_vals) / sizeof (proof_return_vals[0]);

  gimp_install_procedure ("plug_in_separate_separate",
                          _("Generate CMYK separations"),
                          _("Separate performs CMYK colour separation of an image, into "
                            "the four layers."),
                          "Alastair Robinson, Yoshinori Yamakawa",
                          "Alastair Robinson",
                          "2002-2010",
                          N_("<Image>/Image/Separate/Separate"),
                          "RGB*",
                          GIMP_PLUGIN,
                          n_separate_args, n_return_vals,
                          separate_args, return_vals);

  gimp_install_procedure ("plug_in_separate_full",
                          _("Generate CMYK separations"),
                          _("Separate performs CMYK colour separation of an RGB image, into "
                            "the alpha-channels of four coloured layers."),
                          "Alastair Robinson, Yoshinori Yamakawa",
                          "Alastair Robinson",
                          "2002-2010",
                          "",
                          "RGB*",
                          GIMP_PLUGIN,
                          n_separate_args - 1, n_return_vals,
                          separate_args, return_vals);

  gimp_install_procedure ("plug_in_separate_light",
                          _("Generate CMYK separations"),
                          _("Separate performs CMYK colour separation of an RGB image, into "
                            "four greyscale layers."),
                          "Alastair Robinson, Yoshinori Yamakawa",
                          "Alastair Robinson",
                          "2002-2010",
                          "",
                          "RGB*",
                          GIMP_PLUGIN,
                          n_separate_args - 1, n_return_vals,
                          separate_args, return_vals);

  gimp_install_procedure ("plug_in_separate_proof",
                          _("Softproofing CMYK colour"),
                          _("Separate proofs a CMYK colour separation, by transforming back "
                            "into RGB, with media-white simulation."),
                          "Alastair Robinson, Yoshinori Yamakawa",
                          "Alastair Robinson",
                          "2002-2010",
                          N_("<Image>/Image/Separate/Proof"),
                          "RGB*,GRAY*",
                          GIMP_PLUGIN,
                          n_proof_args, n_proof_return_vals,
                          proof_args, proof_return_vals);

  gimp_install_procedure ("plug_in_separate_duotone",
                          _("Generate duotone separations"),
                          "Splits an image into Red and Black plates, mapped into a CMYK image.  "
                          "HACK Alert:  The Red plate occupies the Magenta channel of the CMYK image, "
                          "allowing extraction of spot colour with standard CMYK separation code...",
                          "Alastair Robinson",
                          "Alastair Robinson",
                          "2002",
                          N_("<Image>/Image/Separate/Duotone"),
                          "RGB*",
                          GIMP_PLUGIN,
                          n_duotone_args, n_return_vals,
                          duotone_args, return_vals);

  gimp_install_procedure ("plug_in_separate_save",
                          _("Save separated image"),
                          _("Save separated image in TIFF format.\n"
                            "Note that this procedure is provided for backward compatibility."),
                          "Alastair Robinson, Yoshinori Yamakawa",
                          "Alastair Robinson",
                          "2002-2010",
                          NULL,
                          "RGB*,GRAY*",
                          GIMP_PLUGIN,
                          n_export_args - 3, 0,
                          exportargs, NULL);

  gimp_install_procedure ("plug_in_separate_export",
                          _("Export separated image"),
                          _("Export separated image.\nAvailavle formats are listed in README file."),
                          "Alastair Robinson, Yoshinori Yamakawa",
                          "Alastair Robinson",
                          "2002-2010",
                          N_("<Image>/Image/Separate/Export..."),
                          "RGB*,GRAY*",
                          GIMP_PLUGIN,
                          n_export_args, 0,
                          exportargs, NULL);

  gimp_plugin_icon_register ("plug_in_separate_separate" ,GIMP_ICON_TYPE_INLINE_PIXBUF, separate_icon_cmyk);
  gimp_plugin_icon_register ("plug_in_separate_duotone",GIMP_ICON_TYPE_INLINE_PIXBUF, separate_icon_duotone);
#ifdef GIMP_STOCK_DISPLAY_FILTER_PROOF
  gimp_plugin_icon_register ("plug_in_separate_proof" ,GIMP_ICON_TYPE_STOCK_ID, GIMP_STOCK_DISPLAY_FILTER_PROOF);
#endif
  gimp_plugin_icon_register ("plug_in_separate_export" ,GIMP_ICON_TYPE_STOCK_ID, GTK_STOCK_SAVE_AS);

  gimp_plugin_domain_register (GETTEXT_PACKAGE, NULL);
}


static void
run (const gchar      *name,
     gint              nparams,
     const GimpParam  *param,
     gint             *nreturn_vals,
     GimpParam       **return_vals)
{
  static GimpParam values[3];
  GimpDrawable *drawable;
  GimpRunMode run_mode;
  GimpPDBStatusType status = GIMP_PDB_SUCCESS;
  SeparateContext mysc;
  enum separate_function func = SEP_NONE;
  gint index = 0;

  n_separate_args = sizeof (separate_args) / sizeof (separate_args[0]);
  n_proof_args = sizeof (proof_args) / sizeof (proof_args[0]);
  n_export_args = sizeof(exportargs) / sizeof (exportargs[0]);
  n_duotone_args = sizeof (duotone_args) / sizeof (duotone_args[0]);

  run_mode = param[index++].data.d_int32;
  index++; /* skip the image parameter */

  if (strcmp (name, "plug_in_separate_separate") == 0)
    func = SEP_SEPARATE;
  if (strcmp (name, "plug_in_separate_full") == 0)
    func = SEP_FULL;
  else if (strcmp (name, "plug_in_separate_light") == 0)
    func = SEP_LIGHT;
  else if (strcmp (name, "plug_in_separate_proof") == 0)
    func = SEP_PROOF;
  else if (strcmp (name, "plug_in_separate_save") == 0)
    func = SEP_SAVE;
  else if (strcmp (name, "plug_in_separate_export") == 0)
    func = SEP_EXPORT;
  else if (strcmp( name, "plug_in_separate_duotone") == 0)
    func = SEP_DUOTONE;

  /* setup for localization */
  INIT_I18N ();

  lcms_error_setup ();

  values[1].data.d_image = -1;

  separate_init_settings (&mysc, func, (run_mode != GIMP_RUN_NONINTERACTIVE));

  /*  Get the specified drawable  */
  drawable = gimp_drawable_get (param[index].data.d_drawable);
  mysc.imageID = gimp_drawable_get_image (param[index++].data.d_drawable);//param[1].data.d_image;


  switch (func)
    {
    case SEP_SEPARATE:
    case SEP_FULL:
    case SEP_LIGHT:
    case SEP_PROOF:
      switch (run_mode)
        {
        case GIMP_RUN_NONINTERACTIVE:
          if (func == SEP_SEPARATE)
            {
              if (nparams != n_separate_args)
                status = GIMP_PDB_CALLING_ERROR;
            }
          else
            {
              if (nparams != (func == SEP_PROOF ? n_proof_args : n_separate_args - 1))
                status = GIMP_PDB_CALLING_ERROR;
            }

          if (status == GIMP_PDB_SUCCESS)
            {
              /* Collect the profile filenames */
              gchar *rgbprofile, *cmykprofile;

              rgbprofile = param[index++].data.d_string;

              if (func == SEP_PROOF)
                {
                  cmykprofile = param[index++].data.d_string;
#ifdef ENABLE_COLOR_MANAGEMENT
                  mysc.ps.profile = param[index++].data.d_int32;
#endif

                  if (rgbprofile && strlen (rgbprofile))
                    {
                      g_free (mysc.displayfilename);
                      mysc.displayfilename = g_strdup (rgbprofile);
                    }
                  if (cmykprofile && strlen (cmykprofile))
                    {
                      g_free (mysc.prooffilename);
                      mysc.prooffilename = g_strdup (cmykprofile);
                    }

                  mysc.ps.mode = param[index++].data.d_int32 == -1 ? mysc.ps.mode : param[5].data.d_int32;
                }
              else
                {
                  mysc.ss.profile = param[index++].data.d_int32;
                  cmykprofile = param[index++].data.d_string;

                  if (rgbprofile && strlen (rgbprofile))
                    {
                      g_free (mysc.rgbfilename);
                      mysc.rgbfilename = g_strdup (rgbprofile);
                    }
                  if (cmykprofile && strlen (cmykprofile))
                    {
                      g_free (mysc.cmykfilename);
                      mysc.cmykfilename = g_strdup (cmykprofile);
                    }

                  mysc.ss.intent = param[index].data.d_int32 == -1 ? mysc.ss.intent : param[index].data.d_int32;
                  index++;
                  mysc.ss.bpc = param[index++].data.d_int32;
                  mysc.ss.preserveblack = param[index++].data.d_int32;
                  mysc.ss.overprintblack = param[index++].data.d_int32;
                  mysc.ss.dither = param[index++].data.d_int32;
                }
            }
          break;
        case GIMP_RUN_INTERACTIVE:
          mysc.integrated = (func == SEP_SEPARATE);

          if (!(func == SEP_PROOF ? proof_dialog (&mysc) : separate_dialog (&mysc)))
            status = GIMP_PDB_EXECUTION_ERROR;
          break;
        case GIMP_RUN_WITH_LAST_VALS:
          break;
        default:
          break;
        }

      if (status == GIMP_PDB_SUCCESS)
        {
          /*  Make sure that the drawable is RGB color  */
          mysc.drawable = drawable;

          switch (func)
            {
            case SEP_SEPARATE:
              if ((run_mode == GIMP_RUN_NONINTERACTIVE) ? param[index].data.d_int32 : mysc.ss.composite)
                separate_full (drawable, &values[1], &mysc);
              else
                separate_light (drawable, &values[1], &mysc);
              break;
            case SEP_FULL:
              separate_full (drawable, &values[1], &mysc);
              break;
            case SEP_LIGHT:
              separate_light (drawable, &values[1], &mysc);
              break;
            case SEP_PROOF:
              separate_proof (drawable, &values[1], &mysc);
              break;
            default:
              gimp_message (_("Separate: Internal calling error!"));
            }

          if (run_mode != GIMP_RUN_NONINTERACTIVE)
            {
              gimp_displays_flush();

              if (values[1].data.d_image != -1)
                separate_store_settings (&mysc, func);
            }
        }
      break;
    case SEP_SAVE:
    case SEP_EXPORT:
      if (!(separate_is_CMYK (mysc.imageID)))
        {
          gimp_message (_("This is not a CMYK separated image!"));
          status = GIMP_PDB_EXECUTION_ERROR;
        }
      else
        {
          switch (run_mode)
            {
            case GIMP_RUN_NONINTERACTIVE:
              if (nparams != (func == SEP_SAVE ? n_export_args - 3 : n_export_args))
                status = GIMP_PDB_CALLING_ERROR;

              if (status == GIMP_PDB_SUCCESS)
                {
                  /* Collect the filenames */
                  gchar *filename;

                  filename= param[index++].data.d_string;
                  gimp_image_set_filename (mysc.imageID, filename);
#ifdef ENABLE_COLOR_MANAGEMENT
                  mysc.sas.embedprofile = param[index++].data.d_int32;
#endif
                  if (func == SEP_EXPORT)
                    {
                      mysc.sas.filetype = param[index++].data.d_int32;
                      mysc.sas.compression = param[index++].data.d_int32;
                      mysc.sas.clipping_path_id = param[index++].data.d_vectors;
                    }
                }
              break;
            case GIMP_RUN_INTERACTIVE:
              if (!separate_save_dialog (&mysc))
                status = GIMP_PDB_EXECUTION_ERROR;
              break;
            case GIMP_RUN_WITH_LAST_VALS:
              break;
            default:
              break;
            }
        }

      if (status == GIMP_PDB_SUCCESS)
        {
          separate_export (drawable, &mysc);
          separate_store_settings (&mysc, func);
        }
      break;
    case SEP_DUOTONE:
      separate_duotone (drawable, &values[1], &mysc);
      break;
    default:
      gimp_message (_("Separate: Internal calling error!"));
      break;
    }

  if (func != SEP_DUOTONE)
    {
      g_free (mysc.displayfilename);
      g_free (mysc.cmykfilename);
      g_free (mysc.rgbfilename);
      g_free (mysc.prooffilename);
      g_free (mysc.alt_displayfilename);
      g_free (mysc.alt_cmykfilename);
      g_free (mysc.alt_rgbfilename);
      g_free (mysc.alt_prooffilename);
    }

  *return_vals = values;
  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = status;

  if (func != SEP_SAVE && func != SEP_EXPORT)
    {
      *nreturn_vals = 2;
      values[1].type = GIMP_PDB_IMAGE;

      if (values[1].data.d_image != -1)
        {
          if (run_mode != GIMP_RUN_NONINTERACTIVE)
            {
              gimp_display_new (values[1].data.d_image);
              gimp_displays_flush ();
            }
          gimp_image_undo_enable (values[1].data.d_image);
        }
    }
  else
    *nreturn_vals = 1;

  gimp_drawable_detach (drawable);
}


/* Dialogs */

#define ICC_BUTTON_SET_RGB_MASK(b) \
  (icc_button_set_mask (ICC_BUTTON (b), \
                        ICC_BUTTON_CLASS_INPUT | ICC_BUTTON_CLASS_OUTPUT | ICC_BUTTON_CLASS_DISPLAY, \
                        ICC_BUTTON_COLORSPACE_ALL, ICC_BUTTON_COLORSPACE_RGB))

#define ICC_BUTTON_SET_RGB_PROOF_MASK(b) \
  (icc_button_set_mask (ICC_BUTTON (b), \
                        ICC_BUTTON_CLASS_OUTPUT | ICC_BUTTON_CLASS_DISPLAY, \
                        ICC_BUTTON_COLORSPACE_ALL, ICC_BUTTON_COLORSPACE_RGB))

#define ICC_BUTTON_SET_DEVLINK_MASK(b) \
  (icc_button_set_mask (ICC_BUTTON (b), \
                        ICC_BUTTON_CLASS_LINK, \
                        ICC_BUTTON_COLORSPACE_CMYK, ICC_BUTTON_COLORSPACE_RGB))

#define ICC_BUTTON_SET_CMYK_MASK(b) \
  (icc_button_set_mask (ICC_BUTTON (b), \
                        ICC_BUTTON_CLASS_OUTPUT, \
                        ICC_BUTTON_COLORSPACE_ALL, ICC_BUTTON_COLORSPACE_CMYK))

gboolean
separate_is_ready (SeparateContext *sc)
{
  gboolean ready = TRUE;
  guint16 mask = 0;

  IccButton *src = ICC_BUTTON (sc->rgbfileselector);
  IccButton *dst = ICC_BUTTON (sc->cmykfileselector);

#if 0
  gchar *str = g_strdup_printf ("checking...\nsrc-path : %s\ndst-path : %s\ncheckbutton : %s\n",
                                src->path ? src->path : "NULL",
                                dst->path ? dst->path : "NULL",
                                gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (sc->profileselector)) ? "TRUE" : "FALSE");
  gimp_message (str);
  g_free (str);
#endif

  icc_button_get_mask (src, &mask, NULL, NULL);

  if (mask & ICC_BUTTON_CLASS_LINK)
    {
      if (icc_button_is_empty (src))
        ready = FALSE;
    }
  else
    {
      if ((!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (sc->profileselector)) || !sc->has_embedded_profile) && icc_button_is_empty (src))
        ready = FALSE;

      if (icc_button_is_empty (dst))
        ready = FALSE;
    }

  gtk_dialog_set_response_sensitive (GTK_DIALOG (sc->dialog), GTK_RESPONSE_OK, ready);

  return ready;
}

void
callback_devicelink_toggle_button_destroy (gchar *filename)
{
  g_free (filename);

  return;
}

void
setup_widgets (gboolean         is_devicelink,
               SeparateContext *sc)
{
  IccButton *button = ICC_BUTTON (sc->rgbfileselector);

  if (is_devicelink)
    {
      gtk_widget_set_sensitive (GTK_WIDGET (sc->intentlabel), FALSE);
      gtk_widget_set_sensitive (GTK_WIDGET (sc->intentselector), FALSE);
      gtk_widget_set_sensitive (GTK_WIDGET (sc->bpcselector), FALSE);
      gtk_widget_set_sensitive (GTK_WIDGET (sc->profileselector), FALSE);
      gtk_widget_set_sensitive (GTK_WIDGET (sc->profilelabel), FALSE);
      gtk_label_set_text_with_mnemonic (GTK_LABEL (sc->srclabel), _("Device_link profile:"));
      icc_button_set_title (button, _("Choose devicelink profile..."));
      ICC_BUTTON_SET_DEVLINK_MASK (button);
      icc_button_set_enable_empty (ICC_BUTTON (sc->rgbfileselector), FALSE);
      icc_button_set_enable_empty (ICC_BUTTON (sc->cmykfileselector), TRUE);
      separate_is_ready (sc);
    }
  else
    {
      gtk_widget_set_sensitive (GTK_WIDGET (sc->intentlabel), TRUE);
      gtk_widget_set_sensitive (GTK_WIDGET (sc->intentselector), TRUE);
      gtk_widget_set_sensitive (GTK_WIDGET (sc->bpcselector), TRUE);
      gtk_widget_set_sensitive (GTK_WIDGET (sc->profileselector), TRUE);
      gtk_widget_set_sensitive (GTK_WIDGET (sc->profilelabel), TRUE);
      gtk_label_set_text_with_mnemonic (GTK_LABEL (sc->srclabel), _("_Source color space:"));
      icc_button_set_title (button, _("Choose source profile (RGB)..."));
      ICC_BUTTON_SET_RGB_MASK (button);
      icc_button_set_enable_empty (ICC_BUTTON (sc->rgbfileselector), TRUE);
      icc_button_set_enable_empty (ICC_BUTTON (sc->cmykfileselector), FALSE);
      separate_is_ready (sc);
    }
}

void
callback_devicelink_toggled (GtkWidget       *toggleButton,
                             SeparateContext *sc)
{
  IccButton *button = ICC_BUTTON (sc->rgbfileselector);

  gchar *filename = g_object_steal_data (G_OBJECT (toggleButton), "filename");

  g_object_set_data_full (G_OBJECT (toggleButton), "filename",
                          icc_button_get_filename (button),
                          (GDestroyNotify)callback_devicelink_toggle_button_destroy);

  setup_widgets (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggleButton)), sc);

  if (filename)
    icc_button_set_filename (button, filename, FALSE);
}

void
callback_preserve_black_toggled (GtkWidget *toggleButton,
                                 gpointer   data)
{
  gtk_widget_set_sensitive (GTK_WIDGET (data),
                            gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggleButton)));
}


static gint
separate_dialog (SeparateContext *sc)
{
  GtkWidget *vbox;
  GtkTable  *table;
  GtkWidget *temp;
  GtkWidget *devicelinkselector;
  GtkWidget *pureblackselector;
  GtkWidget *overprintselector;
  GtkWidget *ditherselector;
  GtkWidget *compositeselector;
  gboolean   run;
  gboolean   is_devicelink;

  gimp_ui_init ("separate", FALSE);

  sc->dialogresult = FALSE;
  sc->dialog = gimp_dialog_new (_("Separate"), "separate",
                                NULL, 0,
                                gimp_standard_help_func, "gimp-filter-separate",
                                GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                GTK_STOCK_OK, GTK_RESPONSE_OK,
                                NULL);
  gimp_window_set_transient (GTK_WINDOW (sc->dialog));
  gtk_dialog_set_alternative_button_order (GTK_DIALOG (sc->dialog),
                                           GTK_RESPONSE_OK,
                                           GTK_RESPONSE_CANCEL,
                                           -1);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (sc->dialog)->vbox), vbox, TRUE, TRUE, 0);

  table = GTK_TABLE (gtk_table_new (2, 11, FALSE));
  gtk_table_set_col_spacing (table, 0, 8);
  gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (table), TRUE, TRUE, 0);

  /* Profile file selectors */

  sc->srclabel = gtk_label_new_with_mnemonic (_("_Source color space:"));
  gtk_misc_set_alignment (GTK_MISC (sc->srclabel), 1, 0.5);
  gtk_table_attach (table, sc->srclabel, 0, 1, 0, 1, GTK_FILL, 0, 0, 0);

  sc->rgbfileselector = icc_button_new ();
  gtk_label_set_mnemonic_widget (GTK_LABEL (sc->srclabel), sc->rgbfileselector);
  g_signal_connect_swapped (G_OBJECT (sc->rgbfileselector), "changed",
                            G_CALLBACK (separate_is_ready), (gpointer)sc);
  icc_button_set_max_entries (ICC_BUTTON (sc->rgbfileselector), 10);
  icc_button_dialog_set_show_detail (ICC_BUTTON (sc->rgbfileselector), TRUE);
  icc_button_dialog_set_list_columns (ICC_BUTTON (sc->rgbfileselector), ICC_BUTTON_COLUMN_ICON | ICC_BUTTON_COLUMN_PATH);
  icc_button_set_filename (ICC_BUTTON (sc->rgbfileselector),
                           sc->rgbfilename ? sc->rgbfilename : sc->alt_rgbfilename,
                           FALSE);
  is_devicelink = icc_button_get_class (ICC_BUTTON (sc->rgbfileselector)) == icSigLinkClass;

  gtk_table_attach (table, sc->rgbfileselector, 1, 2, 0, 1, GTK_FILL | GTK_EXPAND, 0, 0, 0);

  sc->profileselector = gtk_check_button_new_with_mnemonic (_("_Give priority to embedded profile"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sc->profileselector), sc->ss.profile);
  g_signal_connect_swapped (G_OBJECT (sc->profileselector), "toggled",
                            G_CALLBACK (separate_is_ready), (gpointer)sc);
  gtk_table_attach (table, sc->profileselector, 1, 2, 1, 2, GTK_FILL, 0, 0, 0);

  {
    GimpParasite *parasite;
    cmsHPROFILE hProfile;
    gchar *labelStr = NULL;
    gint indicator_size, indicator_spacing;

    if ((parasite = gimp_image_parasite_find (sc->imageID, "icc-profile")) != NULL)
      {
        if ((hProfile = cmsOpenProfileFromMem ((gpointer)gimp_parasite_data (parasite),
                                               gimp_parasite_data_size (parasite))) != NULL)
          {
            gchar *desc = lcms_get_profile_desc (hProfile);
            labelStr = g_strdup_printf ("%s", desc);
            g_free (desc);
            cmsCloseProfile (hProfile);
          }
        gimp_parasite_free (parasite);
      }

    if (labelStr)
      {
        sc->profilelabel = gtk_label_new (labelStr);
        g_free( labelStr );

        sc->has_embedded_profile = TRUE;
      }
    else
      sc->profilelabel = gtk_label_new (_("(no profiles embedded)"));

    gtk_label_set_ellipsize (GTK_LABEL (sc->profilelabel), PANGO_ELLIPSIZE_MIDDLE);
    gtk_misc_set_alignment (GTK_MISC (sc->profilelabel), 0, 0.5);

    gtk_widget_style_get (GTK_WIDGET (sc->profileselector),
                          "indicator-size", &indicator_size,
                          "indicator-spacing", &indicator_spacing,
                          NULL);
    temp = gtk_alignment_new (0, 0.5, 1, 0);
    gtk_alignment_set_padding (GTK_ALIGNMENT (temp),
                               0, 0,
                               indicator_size + indicator_spacing * 4, 0);
    gtk_container_add (GTK_CONTAINER (temp), sc->profilelabel);
  }
  gtk_table_attach (table, temp, 1, 2, 2, 3, GTK_FILL, 0, 0, 0);
  gtk_table_set_row_spacing (table, 2, 8);

  temp = gtk_label_new_with_mnemonic (_("D_estination color space:"));
  gtk_misc_set_alignment (GTK_MISC (temp), 1, 0.5);
  gtk_table_attach (table, temp, 0, 1, 3, 4, GTK_FILL, 0, 0, 0);

  sc->cmykfileselector = icc_button_new ();
  gtk_label_set_mnemonic_widget (GTK_LABEL (temp), sc->cmykfileselector);
  g_signal_connect_swapped (G_OBJECT (sc->cmykfileselector), "changed",
                            G_CALLBACK (separate_is_ready), (gpointer)sc);
  icc_button_set_max_entries (ICC_BUTTON (sc->cmykfileselector), 10);
  icc_button_set_title (ICC_BUTTON (sc->cmykfileselector), _("Choose output profile (CMYK)..."));
  /* ボタンのラベルが（未選択）になってしまうことへの回避策 */
  icc_button_set_enable_empty (ICC_BUTTON (sc->cmykfileselector), is_devicelink);
  icc_button_dialog_set_show_detail (ICC_BUTTON (sc->cmykfileselector), TRUE);
  icc_button_dialog_set_list_columns (ICC_BUTTON (sc->cmykfileselector), ICC_BUTTON_COLUMN_ICON | ICC_BUTTON_COLUMN_PATH);
  icc_button_set_filename (ICC_BUTTON (sc->cmykfileselector),
                           (!is_devicelink && !sc->cmykfilename) ? sc->alt_cmykfilename : sc->cmykfilename,
                           FALSE);
  ICC_BUTTON_SET_CMYK_MASK (sc->cmykfileselector);

  gtk_table_attach (table, sc->cmykfileselector, 1, 2, 3, 4, GTK_FILL | GTK_EXPAND, 0, 0, 0);
  gtk_table_set_row_spacing (table, 3, 12 );

  sc->intentlabel = gtk_label_new_with_mnemonic (_("_Rendering intent:"));
  gtk_misc_set_alignment (GTK_MISC (sc->intentlabel), 1, 0.5);
  gtk_table_attach (table, sc->intentlabel, 0, 1, 4, 5, GTK_FILL, 0, 0, 0);

  sc->intentselector = gtk_combo_box_new_text ();
  gtk_label_set_mnemonic_widget (GTK_LABEL (sc->intentlabel), sc->intentselector);
  gtk_combo_box_append_text (GTK_COMBO_BOX (sc->intentselector), _("Perceptual"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (sc->intentselector), _("Relative colorimetric"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (sc->intentselector), _("Saturation"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (sc->intentselector), _("Absolute colorimetric"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (sc->intentselector), _("Absolute colorimetric(2)"));
  gtk_combo_box_set_active (GTK_COMBO_BOX (sc->intentselector),
                            sc->ss.intent < 0 ? 0 : ( sc->ss.intent > 4 ? 4 : sc->ss.intent));
  gtk_table_attach (table, sc->intentselector, 1, 2, 4, 5, GTK_FILL, 0, 0, 0);

  sc->bpcselector = gtk_check_button_new_with_mnemonic (_("Use _BPC algorithm"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sc->bpcselector), sc->ss.bpc);
  gtk_table_attach (table, sc->bpcselector, 1, 2, 5, 6, GTK_FILL, 0, 0, 0);
  gtk_table_set_row_spacing (table, 5, 8);

  temp=gtk_label_new (_("Options:"));
  gtk_misc_set_alignment (GTK_MISC (temp), 1, 0.5);
  gtk_table_attach (table, temp, 0, 1, 6, 7, GTK_FILL, 0, 0, 0);

  devicelinkselector = gtk_check_button_new_with_mnemonic (_("_Use devicelink profile"));
  gtk_table_attach (table, devicelinkselector, 1, 2, 6, 7, GTK_FILL, 0, 0, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (devicelinkselector), is_devicelink);

  g_signal_connect (G_OBJECT( devicelinkselector), "toggled",
                    G_CALLBACK (callback_devicelink_toggled), (gpointer)sc);

  pureblackselector = gtk_check_button_new_with_mnemonic (_("_Preserve pure black"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pureblackselector), sc->ss.preserveblack);
  gtk_table_attach (table, pureblackselector, 1, 2, 7, 8, GTK_FILL, 0, 0, 0);

  overprintselector = gtk_check_button_new_with_mnemonic (_("O_verprint pure black"));
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON (overprintselector), sc->ss.overprintblack);
  gtk_widget_set_sensitive (GTK_WIDGET (overprintselector), sc->ss.preserveblack);
  gtk_table_attach (table, overprintselector, 1, 2, 8, 9, GTK_FILL, 0, 0, 0);

  g_signal_connect (G_OBJECT( pureblackselector), "toggled",
                    G_CALLBACK (callback_preserve_black_toggled), (gpointer)overprintselector);

  ditherselector = gtk_check_button_new_with_mnemonic (_("Use _dither"));
#ifdef ENABLE_DITHER
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ditherselector), sc->ss.dither);
#else
  gtk_widget_set_sensitive (ditherselector, FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ditherselector), FALSE);
#endif
  gtk_table_attach (table, ditherselector, 1, 2, 9, 10, GTK_FILL, 0, 0, 0);

  if (sc->integrated)
    {
      compositeselector = gtk_check_button_new_with_mnemonic (_("_Make CMYK pseudo-composite"));
      gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON (compositeselector), sc->ss.composite);
      gtk_table_attach (table, compositeselector, 1, 2, 11, 12, GTK_FILL, 0, 0, 0);
    }

  setup_widgets (is_devicelink, sc);

  separate_is_ready (sc);

  gtk_widget_show_all (sc->dialog);

  /* Show the widgets */

  run = (gimp_dialog_run (GIMP_DIALOG (sc->dialog)) == GTK_RESPONSE_OK);

  if (run)
    {
      /* Update the source and destination profile names... */
      gchar *tmp;

      tmp = icc_button_get_filename (ICC_BUTTON (sc->rgbfileselector));
      if (tmp != NULL && strlen (tmp))
        {
          g_free (sc->rgbfilename);
          sc->rgbfilename = tmp;
        }
      else
        g_free(tmp);

      tmp = icc_button_get_filename (ICC_BUTTON (sc->cmykfileselector));
      if (is_devicelink || (tmp != NULL && strlen (tmp)))
        {
          g_free (sc->cmykfilename);
          sc->cmykfilename = tmp;
        }
      else
        g_free (tmp);

      sc->ss.preserveblack = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (pureblackselector));
      sc->ss.overprintblack = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (overprintselector));
      sc->ss.intent = gtk_combo_box_get_active (GTK_COMBO_BOX (sc->intentselector));
      sc->ss.bpc = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (sc->bpcselector));
      sc->ss.profile = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (sc->profileselector));
      sc->ss.dither = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ditherselector));

      if (sc->integrated)
        sc->ss.composite = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (compositeselector));
    }

  gtk_widget_destroy (sc->dialog);

  return run;
}


gboolean
proof_is_ready (SeparateContext *sc)
{
  gboolean ready = TRUE;

  IccButton *src = ICC_BUTTON (sc->cmykfileselector);
  IccButton *dst = ICC_BUTTON (sc->rgbfileselector);

#if 0
  gchar *str = g_strdup_printf ("checking...\nsrc-path : %s\ndst-path : %s\ncheckbutton : %s\n",
                                src->path ? src->path : "NULL",
                                dst->path ? dst->path : "NULL",
                                gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (sc->profileselector)) ? "TRUE" : "FALSE");
  gimp_message (str);
  g_free (str);
#endif

#ifdef ENABLE_COLOR_MANAGEMENT
  if ((!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (sc->profileselector)) || !sc->has_embedded_profile) && icc_button_is_empty (src))
#else
  if (icc_button_is_empty (src))
#endif
    ready = FALSE;

  if (icc_button_is_empty (dst))
    ready = FALSE;

  gtk_dialog_set_response_sensitive (GTK_DIALOG (sc->dialog), GTK_RESPONSE_OK, ready);

  return ready;
}

static gint
proof_dialog (SeparateContext *sc)
{
  GtkWidget *vbox;
  GtkTable  *table;
  guint attach = 0;
  GtkWidget *temp;
  GtkWidget *modeselector;
  gboolean   run;

  gimp_ui_init ("separate", FALSE);

  sc->dialogresult = FALSE;
  sc->dialog = gimp_dialog_new (_("Proof"), "proof",
                                NULL, 0,
                                gimp_standard_help_func, "gimp-filter-proof",
                                GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                GTK_STOCK_OK, GTK_RESPONSE_OK,
                                NULL);
  gimp_window_set_transient (GTK_WINDOW (sc->dialog));
  gtk_dialog_set_alternative_button_order (GTK_DIALOG (sc->dialog),
                                           GTK_RESPONSE_OK,
                                           GTK_RESPONSE_CANCEL,
                                           -1);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 12 );
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (sc->dialog)->vbox), vbox, TRUE, TRUE, 0);
  gtk_widget_show (vbox);

#ifdef ENABLE_COLOR_MANAGEMENT
  table = GTK_TABLE (gtk_table_new (2, 6, FALSE));
#else
  table = GTK_TABLE (gtk_table_new (2, 4, FALSE));
#endif
  gtk_table_set_col_spacing (table, 0, 8);
  gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (table), TRUE, TRUE, 0);
  gtk_widget_show (GTK_WIDGET (table));

  /* Profile file selectors */

  temp = gtk_label_new_with_mnemonic (_("_Monitor color space:"));
  gtk_misc_set_alignment (GTK_MISC (temp), 1, 0.5);
  gtk_table_attach (table, temp, 0, 1, attach, attach + 1, GTK_FILL, 0, 0, 0);
  gtk_widget_show (temp);

  sc->rgbfileselector = icc_button_new ();
  gtk_label_set_mnemonic_widget (GTK_LABEL (temp), sc->rgbfileselector);
  g_signal_connect_swapped (G_OBJECT (sc->rgbfileselector), "changed",
                            G_CALLBACK (proof_is_ready), (gpointer)sc);
  icc_button_set_max_entries (ICC_BUTTON (sc->rgbfileselector), 10);
  icc_button_set_title (ICC_BUTTON( sc->rgbfileselector), _("Choose RGB profile..."));
  icc_button_dialog_set_show_detail (ICC_BUTTON (sc->rgbfileselector), TRUE);
  icc_button_dialog_set_list_columns (ICC_BUTTON (sc->rgbfileselector), ICC_BUTTON_COLUMN_ICON | ICC_BUTTON_COLUMN_PATH);
  icc_button_set_filename (ICC_BUTTON (sc->rgbfileselector),
                           !sc->displayfilename ? sc->alt_displayfilename : sc->displayfilename,
                           FALSE);
  ICC_BUTTON_SET_RGB_PROOF_MASK (sc->rgbfileselector);

  gtk_table_attach (table,sc->rgbfileselector, 1, 2, attach , attach + 1, GTK_FILL|GTK_EXPAND, 0, 0, 0);
  attach++;
  gtk_widget_show (sc->rgbfileselector);
  gtk_table_set_row_spacing (table, 0, 8);
  
  temp = gtk_label_new_with_mnemonic (_("_Separated image's color space:"));
  gtk_misc_set_alignment (GTK_MISC( temp ), 1, 0.5 );
  gtk_table_attach (table, temp, 0, 1, attach, attach + 1, GTK_FILL, 0, 0, 0);
  gtk_widget_show (temp);

  sc->cmykfileselector = icc_button_new ();
  gtk_label_set_mnemonic_widget (GTK_LABEL (temp), sc->cmykfileselector);
  g_signal_connect_swapped (G_OBJECT (sc->cmykfileselector), "changed",
                            G_CALLBACK (proof_is_ready), (gpointer)sc);
  icc_button_set_max_entries (ICC_BUTTON (sc->cmykfileselector), 10);
  icc_button_set_title (ICC_BUTTON (sc->cmykfileselector), _("Choose CMYK profile..."));
  icc_button_dialog_set_show_detail (ICC_BUTTON (sc->cmykfileselector), TRUE);
  icc_button_dialog_set_list_columns (ICC_BUTTON (sc->cmykfileselector), ICC_BUTTON_COLUMN_ICON | ICC_BUTTON_COLUMN_PATH);
  icc_button_set_filename (ICC_BUTTON (sc->cmykfileselector),
                           !sc->prooffilename ? sc->alt_prooffilename : sc->prooffilename,
                           FALSE);
  ICC_BUTTON_SET_CMYK_MASK (sc->cmykfileselector);

  gtk_table_attach (table, sc->cmykfileselector, 1, 2, attach, attach + 1, GTK_FILL|GTK_EXPAND, 0, 0, 0);
  gtk_widget_show (sc->cmykfileselector);
  attach++;

#ifdef ENABLE_COLOR_MANAGEMENT
  sc->profileselector = gtk_check_button_new_with_mnemonic (_("_Give priority to attached profile"));
  g_signal_connect_swapped (G_OBJECT (sc->profileselector), "toggled",
                            G_CALLBACK (proof_is_ready), (gpointer)sc);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sc->profileselector), sc->ps.profile);
  gtk_table_attach (table, sc->profileselector, 1, 2, attach, attach + 1, GTK_FILL, 0, 0, 0);
  attach++;
  gtk_widget_show (sc->profileselector);

  {
    GimpParasite *parasite;
    cmsHPROFILE hProfile;
    gchar *labelStr = NULL;
    GtkWidget *label;
    gint indicator_size, indicator_spacing;

    if ((parasite = gimp_image_parasite_find (sc->imageID, CMYKPROFILE)) != NULL)
      {
        if ((hProfile = cmsOpenProfileFromMem ((gpointer)gimp_parasite_data (parasite),
                                               gimp_parasite_data_size (parasite))) != NULL)
          {
            gchar *desc = lcms_get_profile_desc (hProfile);
            labelStr = g_strdup_printf ("%s", desc);
            g_free (desc);
            cmsCloseProfile (hProfile);

            sc->has_embedded_profile = TRUE;
          }
        gimp_parasite_free (parasite);
      }

    if (labelStr)
      {
        label = gtk_label_new (labelStr);
        g_free (labelStr);
      }
    else
      label = gtk_label_new (_("(no profiles attached)"));

    gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_MIDDLE);
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

    gtk_widget_style_get (GTK_WIDGET (sc->profileselector),
                          "indicator-size", &indicator_size,
                          "indicator-spacing", &indicator_spacing,
                          NULL);
    temp = gtk_alignment_new (0, 0.5, 1, 0);
    gtk_alignment_set_padding (GTK_ALIGNMENT (temp),
                               0, 0,
                               indicator_size + indicator_spacing * 4, 0);
    gtk_container_add (GTK_CONTAINER (temp), label);
  }
  gtk_table_attach (table, temp, 1, 2, attach, attach + 1, GTK_FILL, 0, 0, 0);
  attach++;
  gtk_widget_show_all (temp);
#endif

  gtk_table_set_row_spacing( table, attach - 1, 12 );

  temp = gtk_label_new_with_mnemonic (_("M_ode:"));
  gtk_misc_set_alignment (GTK_MISC (temp), 1, 0.5);
  gtk_table_attach (table, temp, 0, 1, attach, attach + 1, GTK_FILL, 0, 0, 0);
  gtk_widget_show (temp);

  modeselector = gtk_combo_box_new_text ();
  gtk_label_set_mnemonic_widget (GTK_LABEL (temp), modeselector);
  gtk_combo_box_append_text (GTK_COMBO_BOX (modeselector), _("Normal"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (modeselector), _("Simulate black ink"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (modeselector), _("Simulate media white"));
  gtk_combo_box_set_active (GTK_COMBO_BOX (modeselector),
                            sc->ps.mode < 0 ? 0 : (sc->ps.mode > 2 ? 2 : sc->ps.mode));
  gtk_table_attach (table, modeselector, 1, 2, attach, attach + 1, GTK_FILL, 0, 0, 0);
  gtk_widget_show (modeselector);

  proof_is_ready (sc);

  /* Show the widgets */

  run = (gimp_dialog_run (GIMP_DIALOG (sc->dialog)) == GTK_RESPONSE_OK);

  if (run)
    {
      /* Update the source and destination profile names... */
      gchar *tmp;

      tmp = icc_button_get_filename (ICC_BUTTON (sc->rgbfileselector));
      if (tmp != NULL && strlen (tmp))
        {
          g_free (sc->displayfilename);
          sc->displayfilename = tmp;
        }
      else
        g_free (tmp);

      tmp = icc_button_get_filename (ICC_BUTTON (sc->cmykfileselector));
      if (tmp != NULL && strlen (tmp))
        {
          g_free (sc->prooffilename);
          sc->prooffilename = tmp;
        }
      else
        g_free (tmp);

      sc->ps.mode = gtk_combo_box_get_active (GTK_COMBO_BOX (modeselector));
#ifdef ENABLE_COLOR_MANAGEMENT
      sc->ps.profile = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (sc->profileselector));
#endif
    }

  gtk_widget_destroy (sc->dialog);

  return run;
}

static gint
separate_save_dialog (SeparateContext *sc)
{
  gchar *filename = gimp_image_get_filename (sc->imageID);
#ifdef G_OS_WIN32
  gchar *dirname = g_path_get_dirname (gimp_filename_to_utf8 (filename));
  gchar *basename = g_path_get_basename (gimp_filename_to_utf8 (filename));
#else
  gchar *dirname = g_path_get_dirname (filename);
  gchar *basename = g_path_get_basename (filename);
#endif
  GtkWidget *hbox, *table, *label, *combo1, *combo2 = NULL, *checkbox;
  gint row = 0;
#ifdef ENABLE_COLOR_MANAGEMENT
  GtkWidget *combo3;
#endif

  sc->filename=NULL;
  sc->dialogresult=FALSE;
  gimp_ui_init ("separate", FALSE);

  sc->filenamefileselector = gtk_file_chooser_dialog_new (_("Export CMYK image..."),
                                                          NULL,
                                                          GTK_FILE_CHOOSER_ACTION_SAVE,
                                                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                                          GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                                          NULL);
  gimp_window_set_transient (GTK_WINDOW (sc->filenamefileselector));
  gtk_dialog_set_alternative_button_order (GTK_DIALOG (sc->filenamefileselector),
                                           GTK_RESPONSE_ACCEPT,
                                           GTK_RESPONSE_CANCEL,
                                           -1);
  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (sc->filenamefileselector), dirname);
  gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (sc->filenamefileselector), basename);
  gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (sc->filenamefileselector), TRUE);

  g_free (filename);
  g_free (dirname);
  g_free (basename);

#ifdef ENABLE_COLOR_MANAGEMENT
  table = gtk_table_new (2, 3, FALSE);
#else
  table = gtk_table_new (2, 2, FALSE);
#endif
  gtk_table_set_col_spacing (GTK_TABLE (table), 0, 8);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (sc->filenamefileselector)->vbox),
                      table, FALSE, TRUE, 0);

  /* file type selector */
  label = gtk_label_new_with_mnemonic (_("_Format:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row + 1, GTK_FILL, 0, 0, 4);
  hbox = gtk_hbox_new (FALSE, 6);
  gtk_table_attach (GTK_TABLE (table), hbox, 1, 2, row, row + 1, GTK_FILL | GTK_EXPAND, 0, 0, 4);
  row++;

  combo1 = gtk_combo_box_new_text ();
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo1);
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo1), _("Auto"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo1), _("TIFF"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo1), _("JPEG"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo1), _("Photoshop PSD"));
  gtk_combo_box_set_active (GTK_COMBO_BOX (combo1), sc->sas.filetype + 1);
  gtk_box_pack_start (GTK_BOX (hbox), combo1, TRUE, TRUE, 0);

  /* Image data compression */
  checkbox = gtk_check_button_new_with_mnemonic (_("Co_mpress pixel data"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbox), sc->sas.compression);
  gtk_box_pack_start (GTK_BOX (hbox), checkbox, FALSE, FALSE, 0);

  {
    gint *vector_id, n_vectors;
    gchar *vector_name;
    gint i;

    vector_id = gimp_image_get_vectors (sc->imageID, &n_vectors);

    if (n_vectors)
      {
        label = gtk_label_new_with_mnemonic (_("Clippi_ng path:"));
        gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
        gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row + 1, GTK_FILL, 0, 0, 4);
        combo2 = g_object_new (GIMP_TYPE_INT_COMBO_BOX, NULL);
        gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo2);

        gimp_int_combo_box_append (GIMP_INT_COMBO_BOX (combo2),
                                   GIMP_INT_STORE_VALUE, -1,
                                   GIMP_INT_STORE_LABEL, _("Don't specify"),
                                   -1);
        gimp_int_combo_box_set_active (GIMP_INT_COMBO_BOX (combo2), -1);

        for (i = 0; i < n_vectors; i++)
          {
            vector_name = gimp_vectors_get_name (vector_id[i]);
            gimp_int_combo_box_append (GIMP_INT_COMBO_BOX (combo2),
                                       GIMP_INT_STORE_VALUE, vector_id[i],
                                       GIMP_INT_STORE_LABEL, vector_name,
                                       -1);
            g_free (vector_name);
          }
        gtk_table_attach (GTK_TABLE (table), combo2, 1, 2, row, row + 1, GTK_FILL | GTK_EXPAND, 0, 0, 4);
        row++;
      }
  }
#ifdef ENABLE_COLOR_MANAGEMENT
  /* What profile embed? */
  {
    GimpParasite *parasite;
    gint lastItemIndex = 2;

    //hbox = gtk_hbox_new (FALSE, 8);
    label = gtk_label_new_with_mnemonic (_("_Embed color profile:"));
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
    gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row + 1, GTK_FILL, 0, 0, 4);
    combo3 = gtk_combo_box_new_text ();
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo3);
    gtk_combo_box_append_text (GTK_COMBO_BOX (combo3), _("None"));
    gtk_combo_box_append_text (GTK_COMBO_BOX (combo3), _("CMYK default profile"));
    gtk_combo_box_append_text (GTK_COMBO_BOX (combo3), _("Print simulation profile"));
    if ((parasite = gimp_image_parasite_find (sc->imageID, CMYKPROFILE)) != NULL)
      {
        cmsHPROFILE hProfile = cmsOpenProfileFromMem ((gpointer)gimp_parasite_data (parasite),
                                                      gimp_parasite_data_size (parasite));
      if (hProfile)
        {
          gchar *desc = lcms_get_profile_desc (hProfile);
          gchar *text = g_strdup_printf (_("Own profile : %s"), desc);

          gtk_combo_box_append_text (GTK_COMBO_BOX (combo3), text);
          lastItemIndex++;

          g_free (desc);
          g_free (text);
          cmsCloseProfile (hProfile);
        }
        gimp_parasite_free (parasite);
      }
    gtk_combo_box_set_active (GTK_COMBO_BOX (combo3),
                              (sc->sas.embedprofile < 0 || sc->sas.embedprofile > lastItemIndex) ? 0 : sc->sas.embedprofile);
    gtk_table_attach (GTK_TABLE (table), combo3, 1, 2, row, row + 1, GTK_FILL | GTK_EXPAND, 0, 0, 4);
  }
#endif

  gtk_widget_show_all (table);

  sc->dialogresult = gtk_dialog_run (GTK_DIALOG (sc->filenamefileselector));
  if (sc->dialogresult == GTK_RESPONSE_ACCEPT)
    {
      sc->sas.filetype = gtk_combo_box_get_active (GTK_COMBO_BOX (combo1)) - 1;
      if (!combo2 || !gimp_int_combo_box_get_active (GIMP_INT_COMBO_BOX (combo2), &sc->sas.clipping_path_id))
        sc->sas.clipping_path_id = -1;
#ifdef ENABLE_COLOR_MANAGEMENT
      sc->sas.embedprofile = gtk_combo_box_get_active (GTK_COMBO_BOX (combo3));
#endif
      sc->filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (sc->filenamefileselector));
#ifdef G_OS_WIN32
      /* GIMP 2.2.x : ANSI
         GIMP 2.3.x : UTF-8 */
      if (GIMP_MAJOR_VERSION == 2 && GIMP_MINOR_VERSION < 3)
        {
          gchar *_filename = g_win32_locale_filename_from_utf8 (sc->filename);
          gimp_image_set_filename (sc->imageID, _filename != NULL ? _filename : sc->filename);
          g_free (_filename );
        }
      else
        gimp_image_set_filename (sc->imageID, sc->filename);
#else
      gimp_image_set_filename (sc->imageID, sc->filename);
#endif
      g_free (sc->filename);
      sc->filename = NULL;

      sc->sas.compression = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbox));

      sc->dialogresult = TRUE;
    }
  else
    sc->dialogresult = FALSE;

  gtk_widget_destroy (sc->filenamefileselector);

  return sc->dialogresult;
}
