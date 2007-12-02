/* Focus Blur -- blur with focus plug-in.
 * Copyright (C) 2002-2007 Kyoichiro Suda
 *
 * The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
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

#include "config.h"

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "libgimp/stdplugins-intl.h"

#include "focusblur.h"
#include "focusblurparam.h"
#include "interface.h"
#include "render.h"
#include "fftblur.h"

#include "source.h"
#include "diffusion.h"
#include "depthmap.h"
#include "shine.h"


/*---- Types ----*/

#ifdef ENABLE_MP
typedef struct _FblurThreadSet FblurThreadSet;
struct _FblurThreadSet
{
  GimpPixelRgn  *region;
  volatile gint *waiting;
  gint           number;
};
#endif


/*---- Prototypes ----*/

static void     focusblur_execute_region        (FblurParam     *param,
                                                 GimpPixelRgn   *region,
                                                 gint            number);
static GtkWidget* focusblur_notebook_append_vbox  (GtkNotebook  *notebook,
                                                   gchar        *label_text);
static GtkWidget* focusblur_notebook_append_table (GtkNotebook  *notebook,
                                                   gchar        *label_text,
                                                   gint          rows);
static void     focusblur_preview_invalidate    (GimpPreview    *gimppreview,
                                                 FblurParam     *param);
static void     focusblur_widget_pickup_focal_depth (FblurParam *param,
                                                     gint        x,
                                                     gint        y);
static void     focusblur_preview_button_press  (GtkWidget      *widget,
                                                 GdkEvent       *event,
                                                 FblurParam     *param);
static gboolean focusblur_map_constraint        (gint32          image_ID,
                                                 gint32          drawable_ID,
                                                 gpointer        data);

/*---- Funcs ----*/

gboolean
focusblur_execute (FblurParam   *param,
                   GimpPreview  *preview)
{
  GimpPixelRgn   dest_rgn;
  gpointer       reg_rgn;
  gdouble        progress, full_progress;
  gint           x1, y1, width, height;

  /* Nothing to do */
  if (param->store.model_radius <= 0.0f)
    return TRUE;

#ifdef HAVE_FFTW3
  if (focusblur_fft_execute (param, preview))
    return TRUE;
#endif

  if (! focusblur_param_prepare (param))
    return FALSE;

  if (! preview)
    {
      x1 = param->source->x1;
      y1 = param->source->y1;
      width  = param->source->x2 - param->source->x1;
      height = param->source->y2 - param->source->y1;
    }
  else
    {
      gimp_preview_get_position (GIMP_PREVIEW (preview), &x1, &y1);
      gimp_preview_get_size (GIMP_PREVIEW (preview), &width, &height);
    }

  gimp_tile_cache_ntiles (16);

  /* Destined image */
  gimp_pixel_rgn_init (&dest_rgn, param->drawable,
                       x1, y1, width, height, (preview == NULL), TRUE);

  progress = full_progress = 0.0;
  if (! preview)
    {
      full_progress = width * height;
      progress = 0.0;
      gimp_progress_init (_("Focus Blur..."));
      gimp_progress_update (0.0001);
    }

  for (reg_rgn = gimp_pixel_rgns_register (1, &dest_rgn);
       reg_rgn != NULL; reg_rgn = gimp_pixel_rgns_process (reg_rgn))
    {
#ifdef ENABLE_MP
      if (param->max_threads)
        {
          volatile gint  num = param->max_threads;
          FblurThreadSet set[param->max_threads];
          gint           i;

          for (i = 0; i < param->max_threads; i ++)
            {
              set[i].region  = &dest_rgn;
              set[i].waiting = &num;
              set[i].number  =  i;
              g_thread_pool_push (param->thread_pool, &(set[i]), NULL);
            }
          focusblur_execute_region (param, &dest_rgn, param->max_threads);
          while (g_atomic_int_get (&num))
            g_thread_yield ();
        }
      else
#endif /* ENABLE_MP */
        focusblur_execute_region (param, &dest_rgn, 0);

      if (! preview)
        {
          progress += (dest_rgn.w * dest_rgn.h);
          gimp_progress_update (progress / full_progress);
        }
      else
        {
          gimp_drawable_preview_draw_region (GIMP_DRAWABLE_PREVIEW (preview),
                                             &dest_rgn);
        }
    }

  if (! preview)
    {
      /* update the blurred region */
      gimp_drawable_flush (param->drawable);
      gimp_drawable_merge_shadow (param->drawable_ID, TRUE);
      gimp_drawable_update (param->drawable_ID, x1, y1, width, height);
    }

  return TRUE;
}


#ifdef ENABLE_MP
void
focusblur_execute_thread (gpointer      data,
                          gpointer      user_data)
{
  FblurThreadSet        *thread_set = data;
  FblurParam            *param = user_data;

  focusblur_execute_region (param, thread_set->region, thread_set->number);
  g_atomic_int_add (thread_set->waiting, -1);
}
#endif /* ENABLE_MP */


static void
focusblur_execute_region (FblurParam    *param,
                          GimpPixelRgn  *region,
                          gint           number)
{
  guchar        *blp, *bp;
  gint           bpp = param->drawable->bpp;
  gint           rowstride = region->rowstride;
  gint           x1, x2, x;
  gint           y1, y2, y;

  blp = region->data;

  x1 = region->x;
  x2 = x1 + region->w;

  y1 = region->y;
  y2 = y1 + region->h;

#ifdef ENABLE_MP
  if (param->max_threads)
    {
      gint h, h0, y3;
      g_assert (number <= param->max_threads);

      h = (region->h + param->max_threads) / (param->max_threads + 1);
      h0 = number * h;
      y1 += h0;
      blp += h0 * rowstride;
      y3 = y1 + h;
      if (y3 < y2)
        y2 = y3;
    }
#endif

  for (y = y1; y < y2; y ++, blp += rowstride)
    for (x = x1, bp = blp; x < x2; x ++, bp += bpp)
      focusblur_render_pixel (x, y, bp, bpp, param);
}


static GtkWidget*
focusblur_notebook_append_vbox (GtkNotebook *notebook,
                                gchar       *label_text)
{
  GtkWidget *vbox;

  vbox = gtk_vbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
  gtk_notebook_append_page (notebook, vbox,
                            gtk_label_new_with_mnemonic (label_text));
  gtk_widget_show (vbox);

  return vbox;
}


static GtkWidget*
focusblur_notebook_append_table (GtkNotebook *notebook,
                                 gchar       *label_text,
                                 gint         rows)
{
  GtkWidget *vbox;
  GtkWidget *table;

  vbox = focusblur_notebook_append_vbox (notebook, label_text);

  table = gtk_table_new (rows, 3, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 6);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);

  gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
  gtk_widget_show (table);

  return table;
}


gboolean
focusblur_dialog (FblurParam *param)
{
  GtkWidget     *dialog;
  GtkWidget     *main_vbox;
  GtkWidget     *preview;
  GtkWidget     *notebook;
  GtkWidget     *nb_vbox;
  GtkWidget     *vbox;
  GtkWidget     *hbox;
  GtkWidget     *label;
  GtkWidget     *align;
  GtkWidget     *spinbutton;
  GtkObject     *adj;
  GtkWidget     *combo_box;
  GtkWidget     *toggle;
  GtkWidget     *table;
  gint           row;
  gboolean       run;

  gimp_ui_init (PLUG_IN_BINARY, TRUE);

  /* Main dialogue */

  dialog = gimp_dialog_new (_("Focus Blur"), PLUG_IN_BINARY,
                            NULL, 0,
                            gimp_standard_help_func, PLUG_IN_PROC,

                            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                            GTK_STOCK_OK,     GTK_RESPONSE_OK,

                            NULL);

  gtk_dialog_set_alternative_button_order (GTK_DIALOG (dialog),
                                           GTK_RESPONSE_OK,
                                           GTK_RESPONSE_CANCEL,
                                           -1);

  gimp_window_set_transient (GTK_WINDOW (dialog));
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

  main_vbox = gtk_vbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 12);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), main_vbox);
  gtk_widget_show (main_vbox);

  /* Preview */

  preview = gimp_drawable_preview_new (param->drawable, NULL);
  gtk_box_pack_start (GTK_BOX (main_vbox), preview, FALSE, TRUE, 0);
  gtk_widget_show (preview);

  g_signal_connect (preview, "invalidated",
                    G_CALLBACK (focusblur_preview_invalidate),
                    param);
  g_signal_connect (gimp_preview_get_area (GIMP_PREVIEW (preview)),
                    "button_press_event",
                    G_CALLBACK (focusblur_preview_button_press),
                    param);

  /* Notebook to select options */

  notebook = gtk_notebook_new ();
  gtk_box_pack_start (GTK_BOX (main_vbox), notebook, TRUE, TRUE, 0);
  gtk_widget_show (notebook);
  /* bind to "Ctrl + Next" and "Ctrl + Prev" */
  g_signal_connect_swapped (G_OBJECT (dialog), "key-press-event",
                            G_CALLBACK (GTK_WIDGET_CLASS (G_OBJECT_GET_CLASS (notebook))->key_press_event),
                            notebook);


  /* Basic parameters */

  nb_vbox = focusblur_notebook_append_vbox
    (GTK_NOTEBOOK (notebook), _("_Basic"));

  /* Diffusion Radius */

  vbox = gtk_vbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX (nb_vbox), vbox, FALSE, FALSE, 0);
  gtk_widget_show (vbox);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  align = gtk_alignment_new (0.0, 0.5, 0.0, 0.0);
  gtk_box_pack_start_defaults (GTK_BOX (hbox), align);
  gtk_widget_show (align);

  label = gtk_label_new_with_mnemonic (_("Diffusion Model and _Radius:"));
  gtk_container_add (GTK_CONTAINER (align), label);
  gtk_widget_show (label);

  spinbutton = gimp_spin_button_new (&adj, param->store.model_radius,
                                     0.0, 100.0, 1.0, 10.0, 0.0, 1.0, 2);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), spinbutton);
  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (gimp_float_adjustment_update),
                    &(param->store.model_radius));
  g_signal_connect_swapped (adj, "value-changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);
  gtk_box_pack_start (GTK_BOX (hbox), spinbutton, FALSE, FALSE, 0);
  gtk_widget_show (spinbutton);

  /* Diffusion Model */

  combo_box = gimp_int_combo_box_new (_("Flat"),    FBLUR_MODEL_FLAT,
                                      _("Ring"),    FBLUR_MODEL_RING,
                                      _("Concave"), FBLUR_MODEL_CONCAVE,
                                      NULL);
  gimp_int_combo_box_connect (GIMP_INT_COMBO_BOX (combo_box),
                              param->store.model_type,
                              G_CALLBACK (gimp_int_combo_box_get_active),
                              &(param->store.model_type));
  g_signal_connect_swapped (combo_box, "changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);
  gtk_box_pack_start (GTK_BOX (vbox), combo_box, FALSE, FALSE, 0);
  gtk_widget_show (combo_box);

  /* the radius to begin to be descending the shine flooding */

  table = gtk_table_new (1, 2, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 6);
  //gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_box_pack_start (GTK_BOX (nb_vbox), table, FALSE, FALSE, 0);
  gtk_widget_show (table);

  spinbutton = gimp_spin_button_new (&adj, param->store.shine_radius,
                                     0.0, 100.0, 1.0, 10.0, 0.0, 1.0, 2);
  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (gimp_float_adjustment_update),
                    &(param->store.shine_radius));
  g_signal_connect_swapped (adj, "value-changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);
  gtk_table_attach (GTK_TABLE (table), spinbutton, 1, 2, 2, 3,
                    GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (spinbutton);

  align = gtk_alignment_new (0.0, 0.5, 0.0, 0.0);
  gtk_table_attach_defaults (GTK_TABLE (table), align, 0, 1, 2, 3);
  gtk_widget_show (align);

  label = gtk_label_new_with_mnemonic (_("P_eak radius for Shining:"));
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), spinbutton);
  gtk_container_add (GTK_CONTAINER (align), label);
  gtk_widget_show (label);

  /* Depth Map */

  vbox = gtk_vbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX (nb_vbox), vbox, FALSE, FALSE, 0);
  gtk_widget_show (vbox);

  toggle = gtk_check_button_new_with_mnemonic (_("_Use Depth map:"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle),
                                param->store.enable_depth_map);
  g_signal_connect (toggle, "toggled",
                    G_CALLBACK (gimp_toggle_button_update),
                    &(param->store.enable_depth_map));
  g_signal_connect_swapped (toggle, "toggled",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);
  gtk_box_pack_start (GTK_BOX (vbox), toggle, FALSE, FALSE, 0);
  gtk_widget_show (toggle);
  param->widgets[FBLUR_WIDGET_ENABLE_DEPTH_MAP] = toggle;

  combo_box = gimp_drawable_combo_box_new (focusblur_map_constraint, NULL);
  gimp_int_combo_box_connect (GIMP_INT_COMBO_BOX (combo_box),
                              param->store.depth_map_ID,
                              G_CALLBACK (gimp_int_combo_box_get_active),
                              &(param->store.depth_map_ID));
  g_signal_connect_swapped (combo_box, "changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);
  gtk_widget_set_sensitive (combo_box, param->store.enable_depth_map);
  g_object_set_data (G_OBJECT (toggle), "set_sensitive", combo_box);
  gtk_box_pack_start (GTK_BOX (vbox), combo_box, FALSE, FALSE, 0);

  gtk_widget_show (combo_box);

  /* Focal Depth */

  table = gtk_table_new (1, 3, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 6);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_widget_set_sensitive (table, param->store.enable_depth_map);
  g_object_set_data (G_OBJECT (combo_box), "set_sensitive", table);
  gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
  gtk_widget_show (table);

  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, 0,
                              _("_Focal depth:"), 100, 0,
                              param->store.focal_depth,
                              0.0, 100.0, 1.0, 10.0, 2, TRUE, 0, 0,
                              NULL, NULL);
  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (gimp_float_adjustment_update),
                    &(param->store.focal_depth));
  g_signal_connect_swapped (adj, "value-changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);
  param->widgets[FBLUR_WIDGET_FOCAL_DEPTH] = GIMP_SCALE_ENTRY_SPINBUTTON (adj);

  /* Model parameters */

  table = focusblur_notebook_append_table (GTK_NOTEBOOK (notebook),
                                           _("_Model"), 3);
  row = 0;

  /* Model fill */

  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, row ++,
                              _("F_illing inside:"), 100, 0,
                              param->store.model_fill,
                              0.0, 100.0, 1.0, 10.0, 2, TRUE, 0, 0,
                              NULL, NULL);
  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (gimp_float_adjustment_update),
                    &(param->store.model_fill));
  g_signal_connect_swapped (adj, "value-changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);

  /* Model softness */

  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, row ++,
                              _("Sof_tness:"), 100, 0,
                              param->store.model_softness,
                              0.0, 100.0, 1.0, 10.0, 2, TRUE, 0, 0,
                              NULL, NULL);
  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (gimp_float_adjustment_update),
                    &(param->store.model_softness));
  g_signal_connect_swapped (adj, "value-changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);

  /* Model softness delay */

  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, row ++,
                              _("Soft_ness delay:"), 100, 0,
                              param->store.model_softness_delay,
                              0.0, 100.0, 1.0, 10.0, 2, TRUE, 0, 0,
                              NULL, NULL);
  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (gimp_float_adjustment_update),
                    &(param->store.model_softness_delay));
  g_signal_connect_swapped (adj, "value-changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);

  /* Shine parameters */

  table = focusblur_notebook_append_table (GTK_NOTEBOOK (notebook),
                                           _("_Shine"), 4);
  row = 0;

  /* Shine type */

  combo_box = gimp_int_combo_box_new (_("Luminosity"),  FBLUR_SHINE_LUMINOSITY,
                                      _("Saturation"),  FBLUR_SHINE_SATURATION,
                                      NULL);
  gimp_int_combo_box_connect (GIMP_INT_COMBO_BOX (combo_box),
                              param->store.shine_type,
                              G_CALLBACK (gimp_int_combo_box_get_active),
                              &(param->store.shine_type));
  g_signal_connect_swapped (combo_box, "changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);
  label = gimp_table_attach_aligned (GTK_TABLE (table), 0, row ++,
                                     _("Sensing t_ype:"), 0.0, 0.5,
                                     combo_box, 2, FALSE);
  gtk_widget_show (combo_box);

  /* Shine Threshold */

  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, row ++,
                              _("_Threshold:"), 100, 0,
                              param->store.shine_threshold,
                              0.0, 100.0, 1.0, 10.0, 2, TRUE, 0, 0,
                              NULL, NULL);
  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (gimp_float_adjustment_update),
                    &(param->store.shine_threshold));
  g_signal_connect_swapped (adj, "value-changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);

  /* Shine Level */

  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, row ++,
                              _("_Level:"), 100, 0,
                              param->store.shine_level,
                              0.0, 100.0, 1.0, 10.0, 2, TRUE, 0, 0,
                              NULL, NULL);
  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (gimp_float_adjustment_update),
                    &(param->store.shine_level));
  g_signal_connect_swapped (adj, "value-changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);

  /* Shine Curve */

  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, row ++,
                              _("C_urve:"), 100, 0,
                              param->store.shine_curve,
                              0.0, 10.0, 0.1, 1.0, 3, TRUE, 0, 0,
                              NULL, NULL);
  gimp_scale_entry_set_logarithmic (adj, TRUE);
  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (gimp_float_adjustment_update),
                    &(param->store.shine_curve));
  g_signal_connect_swapped (adj, "value-changed",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);

  /* Depth parameters */

  table = focusblur_notebook_append_table (GTK_NOTEBOOK (notebook),
                                           _("_Depth"), 1);
  row = 0;

  /* Depth Precedence */

  toggle = gtk_check_button_new_with_mnemonic (_("Depth with p_recedence"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle),
                                param->store.enable_depth_precedence);
  g_signal_connect (toggle, "toggled",
                    G_CALLBACK (gimp_toggle_button_update),
                    &(param->store.enable_depth_precedence));
  g_signal_connect_swapped (toggle, "toggled",
                            G_CALLBACK (gimp_preview_invalidate),
                            preview);
  gtk_table_attach_defaults (GTK_TABLE (table), toggle, 0, 3, row, row + 1);
  row ++;
  gtk_widget_show (toggle);

#ifdef HAVE_FFTW3

  /* Quality parameters */

  table = focusblur_notebook_append_table (GTK_NOTEBOOK (notebook),
                                           _("Pre_ferences"), 2);
  row = 0;

  /* Rendering quality */
  combo_box = gimp_int_combo_box_new (_("Best"),        FBLUR_QUALITY_BEST,
                                      _("Normal"),      FBLUR_QUALITY_NORMAL,
                                      _("Low"),         FBLUR_QUALITY_LOW,
                                      NULL);
  gimp_int_combo_box_connect (GIMP_INT_COMBO_BOX (combo_box),
                              param->pref.quality,
                              G_CALLBACK (gimp_int_combo_box_get_active),
                              &(param->pref.quality));
  label = gimp_table_attach_aligned (GTK_TABLE (table), 0, row ++,
                                     _("Rendering _quality:"), 0.0, 0.5,
                                     combo_box, 2, FALSE);
  gtk_widget_show (combo_box);

  /* Detail: preview quality */
  combo_box = gimp_int_combo_box_new (_("Normal"),      FBLUR_QUALITY_NORMAL,
                                      _("Low"),         FBLUR_QUALITY_LOW,
                                      _("Defective"), FBLUR_QUALITY_DEFECTIVE,
                                      NULL);
  gimp_int_combo_box_connect (GIMP_INT_COMBO_BOX (combo_box),
                              param->pref.quality_preview,
                              G_CALLBACK (gimp_int_combo_box_get_active),
                              &(param->pref.quality_preview));
  label = gimp_table_attach_aligned (GTK_TABLE (table), 0, row ++,
                                     _("Pre_view quality:"), 0.0, 0.5,
                                     combo_box, 2, FALSE);
  gtk_widget_show (combo_box);

#endif /* HAVE_FFTW3 */

#ifdef ENABLE_MP

  /* Disable multi-threads */

  if (row)
    gtk_table_set_row_spacing (GTK_TABLE (table), row - 1, 12);

  toggle = gtk_check_button_new_with_mnemonic (_("Dis_able support for multi-threads"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle),
                                param->pref.disable_mp);
  g_signal_connect (toggle, "toggled",
                    G_CALLBACK (gimp_toggle_button_update),
                    &(param->pref.disable_mp));
  gtk_table_attach_defaults (GTK_TABLE (table), toggle, 0, 3, row, row + 1);
  row ++;
  gtk_widget_show (toggle);

#endif /* ENABLE_MP */


  /* get parameters */

  gtk_widget_show (dialog);
  run = (gimp_dialog_run (GIMP_DIALOG (dialog)) == GTK_RESPONSE_OK);
  gtk_widget_destroy (dialog);

  return run;
}


void
focusblur_widget_set_enable_depth_map (FblurParam       *param,
                                       gboolean          bool)
{
  GtkWidget *widget;

  widget = param->widgets[FBLUR_WIDGET_ENABLE_DEPTH_MAP];
  if (widget)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), bool);
  else
    param->store.enable_depth_map = bool;
}


void
focusblur_widget_set_enable_shine (FblurParam   *param,
                                   gboolean      bool)
{
  GtkWidget *widget;

  widget = param->widgets[FBLUR_WIDGET_ENABLE_SHINE];
  if (widget)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), bool);
  else
    param->store.enable_shine = bool;
}


static void
focusblur_preview_invalidate (GimpPreview *gimppreview,
                              FblurParam  *param)
{
  focusblur_execute (param, gimppreview);
}


static void
focusblur_widget_pickup_focal_depth (FblurParam *param,
                                     gint        x,
                                     gint        y)
{
  GtkSpinButton         *spin;
  gfloat                 focal_depth;
  gint                   depth;
  guchar                *pixel;
  gint                   num_channels;

  if (! param->store.enable_depth_map ||
      ! gimp_drawable_is_valid (param->store.depth_map_ID))
    return;

  pixel = gimp_drawable_get_pixel
    (param->store.depth_map_ID, x, y, &num_channels);

  switch (gimp_drawable_type (param->store.depth_map_ID))
    {
    case GIMP_GRAYA_IMAGE:
      g_assert (num_channels == 2);
      if (! pixel[1])
        goto exit;

    case GIMP_GRAY_IMAGE:
      g_assert (num_channels >= 1);
      depth = FBLUR_DEPTH_MAX * pixel[0] / 255;
      break;

    case GIMP_RGBA_IMAGE:
      g_assert (num_channels == 4);
      if (! pixel[4])
        goto exit;

    case GIMP_RGB_IMAGE:
      g_assert (num_channels >= 3);
      depth = FBLUR_DEPTH_MAX * (pixel[0] + pixel[1] + pixel[2]) / (3 * 255);
      break;

    default:
      g_assert_not_reached ();
    }

  focal_depth = (100.0f / (gfloat) FBLUR_DEPTH_MAX) * depth;
  spin = GTK_SPIN_BUTTON (param->widgets[FBLUR_WIDGET_FOCAL_DEPTH]);
  gtk_spin_button_set_value (spin, focal_depth);

 exit:
  g_free (pixel);
}


static void
focusblur_preview_button_press (GtkWidget       *widget,
                                GdkEvent        *event,
                                FblurParam      *param)
{
  GimpPreviewArea       *area;
  gint                   x, y;

  if (event->button.button != 2)
    return;

  if (! param->store.enable_depth_map ||
      ! gimp_drawable_is_valid (param->store.depth_map_ID))
    return;

  area = GIMP_PREVIEW_AREA (widget);
  x = area->offset_x + event->button.x;
  y = area->offset_y + event->button.y;

  focusblur_widget_pickup_focal_depth (param, x, y);
}


static gboolean
focusblur_map_constraint (gint32         image_ID,
                          gint32         drawable_ID,
                          gpointer       data)
{
  if (drawable_ID == -1)
    return FALSE;

  return (gimp_drawable_is_rgb (drawable_ID) ||
          gimp_drawable_is_gray (drawable_ID));
}
