/* GIMP LiquidRescaling Plug-in
 * Copyright (C) 2007 Carlo Baldassi (the "Author") <carlobaldassi@yahoo.it>.
 * (implementation based on the GIMP Plug-in Template by Michael Natterer)
 * All Rights Reserved.
 *
 * This plugin implements the algorithm described in the paper
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

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include <math.h>

#include "plugin-intl.h"

#include "main.h"
#include "interface.h"



/*  Constants  */

#define SCALE_WIDTH         80
#define SPIN_BUTTON_WIDTH   75
#define MAX_COEFF	  1000
#define PREVIEW_MAX_WIDTH  300
#define PREVIEW_MAX_HEIGHT 200
//#define RC_STRING_SIZE     200

/*  Local function prototypes  */

static gint count_extra_layers (gint32 image_ID, GimpDrawable * drawable);

static gboolean dialog_layer_constraint_func (gint32 image_id,
                                              gint32 layer_id, gpointer data);

static void callback_pres_combo_get_active(GtkWidget *combo, gpointer data);
static void callback_disc_combo_get_active(GtkWidget *combo, gpointer data);
static void callback_combo_set_sensitive (GtkWidget * button, gpointer data);
static void callback_pres_combo_set_sensitive_preview(GtkWidget * button, gpointer data);
static void callback_disc_combo_set_sensitive_preview(GtkWidget * button, gpointer data);
static void callback_resize_aux_layers_button_set_sensitive (GtkWidget *
                                                             button,
                                                             gpointer data);

/*
static void callback_update_preview (GimpDrawablePreview * preview,
                                     gpointer data);
				     */

static void preview_init_mem(PreviewData * preview_data);
static guchar* preview_build_buffer (guchar * buffer, gint32 layer_ID);
static void preview_build_pixbuf(PreviewData * preview_data);

static gboolean
preview_expose_event_callback (GtkWidget * preview_area,
                               GdkEventExpose * event, gpointer data);


/*  Local variables  */

static PlugInUIVals *ui_state = NULL;
gint32 preview_layer_ID;
gboolean pres_combo_awaked = FALSE;
gboolean disc_combo_awaked = FALSE;

/*
static gchar *rc_template =
  "style \"gimp-large-preview\" { GimpPreview::size = %i} class \"GimpPreview\" style \"gimp-large-preview\"";
gchar rc_string[RC_STRING_SIZE];
*/


/*  Public functions  */

gboolean
dialog (gint32 image_ID,
        GimpDrawable * drawable,
        PlugInVals * vals,
        PlugInImageVals * image_vals,
        PlugInDrawableVals * drawable_vals, PlugInUIVals * ui_vals)
{
  gboolean is_floating_sel = FALSE;
  gint32 active_channel_ID;
  gint32 saved_selection_ID;
  gint num_extra_layers;
  gboolean features_are_sensitive;
  GtkWidget *dlg;
  GtkWidget *main_vbox;
  GtkWidget *frame;
  gfloat wfactor, hfactor;
  PreviewData preview_data;
  //GtkWidget *preview_widget;
  GtkWidget *preview_area;
  //GimpPixelRgn rgn_in;
  GtkWidget *table;
  GtkWidget *hbox;
  //GtkWidget *hbox2;
  GtkWidget *coordinates;
  ToggleData pres_toggle_data;
  GtkWidget *pres_vbox;
  GtkWidget *pres_button;
  ToggleData disc_toggle_data;
  GtkWidget *disc_vbox;
  GtkWidget *disc_button;
  GtkWidget *grad_func_combo_box;
  gchar *text;
  GtkWidget *options_expander;
  GtkWidget *expander_vbox;
  GtkWidget *expander_frame;
  GtkWidget *vbox;
  GtkWidget *update_en_button;
  GtkWidget *resize_canvas_button;
  GtkWidget *resize_aux_layers_button;
  PresDiscStatus presdisc_status;
  GtkWidget *mask_behavior_combo_box = NULL;
  gboolean has_mask = FALSE;
  GtkWidget *combo;
  GtkWidget *label;
  GtkObject *adj;
  gint row;
  gboolean run = FALSE;
  GimpUnit unit;
  gdouble xres, yres;


  ui_state = ui_vals;

  gimp_ui_init (PLUGIN_NAME, TRUE);

  pres_toggle_data.ui_toggled = &(ui_vals->pres_status);
  disc_toggle_data.ui_toggled = &(ui_vals->disc_status);
  if (ui_vals->pres_status == TRUE) {
	  pres_combo_awaked = TRUE;
  }
  if (ui_vals->disc_status == TRUE) {
	  disc_combo_awaked = TRUE;
  }

  if (gimp_layer_is_floating_sel (drawable->drawable_id))
    {
      //printf("is floatingsel!\n"); fflush(stdout);
      is_floating_sel = TRUE;
      gimp_floating_sel_anchor (drawable->drawable_id);
      drawable =
        gimp_drawable_get (gimp_image_get_active_drawable (image_ID));
    }
  if (gimp_drawable_is_channel (drawable->drawable_id))
    {
      //printf("is channel!\n"); fflush(stdout);
      active_channel_ID = gimp_image_get_active_channel (image_ID);
      gimp_image_unset_active_channel (image_ID);
      drawable =
        gimp_drawable_get (gimp_image_get_active_drawable (image_ID));
    }
  if (gimp_selection_is_empty (image_ID) == FALSE)
    {
      //printf("nonempty sel!\n"); fflush(stdout);
      saved_selection_ID = gimp_selection_save (image_ID);
      gimp_selection_none (image_ID);
      gimp_image_unset_active_channel (image_ID);
      drawable =
        gimp_drawable_get (gimp_image_get_active_drawable (image_ID));
    }
  else
    {
      //printf("empty sel!\n");
    }

  if (gimp_drawable_is_layer_mask (drawable->drawable_id) == TRUE)
    {
      drawable =
        gimp_drawable_get (gimp_layer_from_mask (drawable->drawable_id));
    }

  g_assert (gimp_drawable_is_layer (drawable->drawable_id) == TRUE);

  if (gimp_layer_get_mask (drawable->drawable_id) != -1)
    {
      has_mask = TRUE;
    }

  num_extra_layers = count_extra_layers (image_ID, drawable);
  //printf("found %i layers\n", num_extra_layers);
  features_are_sensitive = (num_extra_layers > 0 ? TRUE : FALSE);
  if (!features_are_sensitive)
    {
      ui_vals->pres_status = FALSE;
      ui_vals->disc_status = FALSE;
    }

  vals->new_width = gimp_drawable_width (drawable->drawable_id);
  vals->new_height = gimp_drawable_height (drawable->drawable_id);

  dlg = gimp_dialog_new (_("GIMP LiquidRescale Plug-In"), PLUGIN_NAME,
                         NULL, 0,
                         gimp_standard_help_func, "lqr-plug-in",
                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                         GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);

  gtk_window_set_resizable (GTK_WINDOW (dlg), FALSE);

  main_vbox = gtk_vbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 12);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dlg)->vbox), main_vbox);

  frame = gimp_frame_new (_("Selected layer"));
  gtk_box_pack_start (GTK_BOX (main_vbox), frame, TRUE, FALSE, 0);
  gtk_widget_show (frame);

  /* Preview */

  preview_data.image_ID = image_ID;
  preview_data.vals = vals;
  preview_data.ui_vals = ui_vals;
  wfactor = (gfloat) PREVIEW_MAX_WIDTH / drawable->width;
  hfactor = (gfloat) PREVIEW_MAX_HEIGHT / drawable->height;
  preview_data.factor = MIN (wfactor, hfactor);
  preview_data.factor = MIN (preview_data.factor, 1);


  preview_data.old_width = drawable->width;
  preview_data.old_height = drawable->height;
  gimp_drawable_offsets(drawable->drawable_id, &(preview_data.x_off), &(preview_data.y_off));
  //preview_data.layer_ID = drawable->drawable_id;
  preview_data.layer_ID = gimp_layer_copy (drawable->drawable_id);
  //printf ("layer copied\n"); fflush (stdout);
  gimp_image_add_layer (image_ID, preview_data.layer_ID, 1);

  preview_data.drawable = gimp_drawable_get (preview_data.layer_ID);
  preview_layer_ID = preview_data.layer_ID;
  preview_data.width = drawable->width * preview_data.factor;
  preview_data.height = drawable->height * preview_data.factor;
  //printf ("w,h=%i,%i\n", preview_data.width, preview_data.height); fflush (stdout);
  gimp_layer_scale (preview_data.layer_ID, preview_data.width,
                    preview_data.height, TRUE);
  //printf ("layer scaled\n"); fflush (stdout);
  gimp_layer_add_alpha(preview_data.layer_ID);
  preview_data.drawable = gimp_drawable_get (preview_data.layer_ID);
  preview_data.toggle = TRUE;
  /*
     preview_widget = gimp_drawable_preview_new (preview_data.drawable,
     &preview_data.toggle);
     snprintf (rc_string, RC_STRING_SIZE, rc_template,
     MAX (preview_data.width, preview_data.height));
     gtk_rc_parse_string (rc_string);
     printf ("preview crated\n");
     fflush (stdout);
     preview_data.preview = GIMP_DRAWABLE_PREVIEW (preview_widget);
   */

  /*
     gimp_pixel_rgn_init (&rgn_in, preview_data.drawable, 0, 0,
     preview_data.width, preview_data.height,
     FALSE, FALSE);
     printf ("region initialized\n");
     fflush (stdout);
   */
  //gimp_preview_draw(GIMP_PREVIEW(preview_data.drawable));
  //gimp_drawable_preview_draw_region(GIMP_DRAWABLE_PREVIEW(preview_data.drawable), &rgn_in);
  //printf ("preview drawed\n");
  //fflush (stdout);


  //preview_data.type = GIMP_RGB_IMAGE;
  preview_init_mem(&preview_data);
  preview_data.buffer = preview_build_buffer (preview_data.buffer, preview_data.layer_ID);
  /*
     printf("preview_buffer[20,10] rgb=%i %i %i\n",  ((guchar*)preview_data.buffer)[(10 * preview_data.width + 20) * 4 + 0],
	     ((guchar*)preview_data.buffer)[(10 * preview_data.width + 20) * 4 + 1],
	     ((guchar*)preview_data.buffer)[(10 * preview_data.width + 20) * 4 + 2]); fflush(stdout);
	     */
  gimp_image_remove_layer (image_ID, preview_data.layer_ID);
  preview_build_pixbuf(&preview_data);

  preview_area = gtk_drawing_area_new ();
  preview_data.area = preview_area;
  gtk_drawing_area_size (GTK_DRAWING_AREA (preview_area), preview_data.width,
                         preview_data.height);
  //
  //preview_area = gimp_preview_area_new ();
  //preview_area = gimp_offset_area_new(preview_data.width, preview_data.height);
  //preview_pixbuf = gimp_drawable_get_thumbnail(preview_data.layer_ID, preview_data.width, preview_data.height, GIMP_PIXBUF_KEEP_ALPHA);
  //gimp_offset_area_set_pixbuf(GIMP_OFFSET_AREA(preview_area), preview_pixbuf);
  //gimp_offset_area_set_size(GIMP_OFFSET_AREA(preview_area), preview_data.width, preview_data.height);
  //gtk_drawing_area_size(GTK_DRAWING_AREA(preview_area), preview_data.width, preview_data.height);
  //gimp_preview_area_set_max_size(GIMP_PREVIEW_AREA(preview_area), 2 * preview_data.width, 2 * preview_data.height);
  /*
     gimp_preview_area_draw(GIMP_PREVIEW_AREA(preview_area), 0, 0, preview_data.width, preview_data.height, preview_data.type,
     preview_data.buffer, preview_data.width * 3 * sizeof(guchar));
   */

  g_signal_connect (G_OBJECT (preview_area), "expose_event",
                    G_CALLBACK (preview_expose_event_callback),
                    (gpointer) (&preview_data));

  gtk_container_add (GTK_CONTAINER (frame), preview_area);
  //printf ("preview added\n");
  fflush (stdout);
  gtk_widget_show (preview_area);

  /*  Coordinates widget for selecting new size  */

  frame = gimp_frame_new (_("Select new width and height"));
  gtk_box_pack_start (GTK_BOX (main_vbox), frame, TRUE, TRUE, 0);
  gtk_widget_show (frame);

  hbox = gtk_hbox_new (FALSE, 4);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 4);
  gtk_container_add (GTK_CONTAINER (frame), hbox);
  gtk_widget_show (hbox);

  unit = gimp_image_get_unit (image_ID);
  gimp_image_get_resolution (image_ID, &xres, &yres);

  coordinates =
    gimp_coordinates_new (unit, "%p", TRUE, TRUE, SPIN_BUTTON_WIDTH,
                          GIMP_SIZE_ENTRY_UPDATE_SIZE, ui_vals->chain_active,
                          TRUE, _("Width:"), vals->new_width, xres, 2,
                          vals->new_width * 2 - 1, 0, vals->new_width,
                          _("Height:"), vals->new_height, yres, 2,
                          vals->new_height * 2 - 1, 0, vals->new_height);
  //preview_data.coordinates = (gpointer) coordinates;


  /*
     g_signal_connect_swapped (coordinates, "value_changed",
     G_CALLBACK (gimp_preview_invalidate),
     preview_area);
     g_signal_connect_swapped (coordinates, "refval_changed",
     G_CALLBACK (gimp_preview_invalidate),
     preview_area);


     g_signal_connect (preview_area, "invalidated",
     G_CALLBACK (callback_update_preview),
     (gpointer *) (&preview_data));
   */


  //g_signal_connect(coordinates, "refval-changed", G_CALLBACK(callback_size_changed), &((gpointer)preview_data));

  gtk_box_pack_start (GTK_BOX (hbox), coordinates, FALSE, FALSE, 0);
  gtk_widget_show (coordinates);

  /*  Feature preservation  */

  frame = gimp_frame_new (_("Feature preservation selection"));
  gtk_box_pack_start (GTK_BOX (main_vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  pres_vbox = gtk_vbox_new (FALSE, 4);
  gtk_container_add (GTK_CONTAINER (frame), pres_vbox);
  gtk_widget_show (pres_vbox);

  pres_button =
    gtk_check_button_new_with_label (_("Activate feature preservation"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pres_button),
                                ui_vals->pres_status);

  gtk_widget_set_sensitive (pres_button, features_are_sensitive);


  gtk_box_pack_start (GTK_BOX (pres_vbox), pres_button, FALSE, FALSE, 0);
  gtk_widget_show (pres_button);

  table = gtk_table_new (1, 2, FALSE);
  gtk_container_set_border_width (GTK_CONTAINER (table), 4);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  gtk_table_set_row_spacings (GTK_TABLE (table), 2);
  gtk_box_pack_start (GTK_BOX (pres_vbox), table, FALSE, FALSE, 0);
  gtk_widget_show (table);

  row = 0;

  combo =
    gimp_layer_combo_box_new (dialog_layer_constraint_func,
                              (gpointer *) drawable);

  gimp_int_combo_box_connect (GIMP_INT_COMBO_BOX (combo),
                              drawable->drawable_id,
                              G_CALLBACK (callback_pres_combo_get_active),
                              (gpointer)(&preview_data));

  gimp_int_combo_box_set_active(GIMP_INT_COMBO_BOX(combo), ui_vals->pres_layer_ID);

  label = gimp_table_attach_aligned (GTK_TABLE (table), 0, row++,
                                     _("Available Layers:"), 0.0, 0.5, combo,
                                     1, FALSE);

  gtk_widget_set_sensitive (label, ui_vals->pres_status
                            && features_are_sensitive);

  gtk_widget_set_sensitive (combo, ui_vals->pres_status
                            && features_are_sensitive);
  pres_toggle_data.combo = (gpointer) combo;
  pres_toggle_data.combo_label = (gpointer) label;
  preview_data.pres_combo = combo;


  table = gtk_table_new (1, 2, FALSE);
  gtk_container_set_border_width (GTK_CONTAINER (table), 4);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  gtk_table_set_row_spacings (GTK_TABLE (table), 2);
  gtk_box_pack_start (GTK_BOX (pres_vbox), table, FALSE, FALSE, 0);
  gtk_widget_show (table);

  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, row++,
                              _("Intensity:"), SCALE_WIDTH, SPIN_BUTTON_WIDTH,
                              vals->pres_coeff, 0, MAX_COEFF, 1, 10, 0,
                              TRUE, 0, 0,
                              _("Overall coefficient for feature preservation intensity"),
                              NULL);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (gimp_int_adjustment_update),
                    &vals->pres_coeff);

  gtk_widget_set_sensitive (GIMP_SCALE_ENTRY_LABEL (adj),
                            (ui_vals->pres_status && features_are_sensitive));
  gtk_widget_set_sensitive (GIMP_SCALE_ENTRY_SCALE (adj),
                            (ui_vals->pres_status && features_are_sensitive));
  gtk_widget_set_sensitive (GIMP_SCALE_ENTRY_SPINBUTTON (adj),
                            (ui_vals->pres_status && features_are_sensitive));
  pres_toggle_data.scale = (gpointer) adj;

  pres_toggle_data.status = &(ui_vals->pres_status);

  g_signal_connect (pres_button, "toggled",
                    G_CALLBACK (callback_combo_set_sensitive),
                    (gpointer) (&pres_toggle_data));

  g_signal_connect (G_OBJECT (pres_button), "toggled",
                    G_CALLBACK (callback_pres_combo_set_sensitive_preview),
                    (gpointer) (&preview_data));




  /*  Feature discard  */

  frame = gimp_frame_new (_("Feature discard selection"));
  gtk_box_pack_start (GTK_BOX (main_vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  disc_vbox = gtk_vbox_new (FALSE, 4);
  gtk_container_add (GTK_CONTAINER (frame), disc_vbox);
  gtk_widget_show (disc_vbox);

  disc_button =
    gtk_check_button_new_with_label (_("Activate feature discard"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (disc_button),
                                ui_vals->disc_status);

  gtk_widget_set_sensitive (disc_button, features_are_sensitive);

  gtk_box_pack_start (GTK_BOX (disc_vbox), disc_button, FALSE, FALSE, 0);
  gtk_widget_show (disc_button);



  table = gtk_table_new (1, 2, FALSE);
  gtk_container_set_border_width (GTK_CONTAINER (table), 4);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  gtk_table_set_row_spacings (GTK_TABLE (table), 2);
  gtk_box_pack_start (GTK_BOX (disc_vbox), table, FALSE, FALSE, 0);
  gtk_widget_show (table);

  row = 0;

  combo =
    gimp_layer_combo_box_new (dialog_layer_constraint_func,
                              (gpointer *) drawable);

  gimp_int_combo_box_connect (GIMP_INT_COMBO_BOX (combo),
                              drawable->drawable_id,
                              G_CALLBACK (callback_disc_combo_get_active),
                              (gpointer)(&preview_data));

  gimp_int_combo_box_set_active(GIMP_INT_COMBO_BOX(combo), ui_vals->disc_layer_ID);

  gtk_widget_set_sensitive (combo, ui_vals->disc_status
                            && features_are_sensitive);
  label = gimp_table_attach_aligned (GTK_TABLE (table), 0, row++,
                                     _("Available Layers:"), 0.0, 0.5, combo,
                                     1, FALSE);

  disc_toggle_data.combo = (gpointer) combo;
  disc_toggle_data.combo_label = (gpointer) label;
  preview_data.disc_combo = combo;


  gtk_widget_set_sensitive (label, ui_vals->disc_status
                            && features_are_sensitive);

  table = gtk_table_new (1, 2, FALSE);
  gtk_container_set_border_width (GTK_CONTAINER (table), 4);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  gtk_table_set_row_spacings (GTK_TABLE (table), 2);
  gtk_box_pack_start (GTK_BOX (disc_vbox), table, FALSE, FALSE, 0);
  gtk_widget_show (table);

  adj = gimp_scale_entry_new (GTK_TABLE (table), 0, 0,
                              _("Intensity:"), SCALE_WIDTH, SPIN_BUTTON_WIDTH,
                              vals->disc_coeff, 0, MAX_COEFF, 1, 10, 0,
                              TRUE, 0, 0,
                              _ ("Overall coefficient for feature discard intensity"),
                              NULL);
  g_signal_connect (adj, "value_changed",
                    G_CALLBACK (gimp_int_adjustment_update),
                    &vals->disc_coeff);


  gtk_widget_set_sensitive (GIMP_SCALE_ENTRY_LABEL (adj),
                            (ui_vals->disc_status && features_are_sensitive));
  gtk_widget_set_sensitive (GIMP_SCALE_ENTRY_SCALE (adj),
                            (ui_vals->disc_status && features_are_sensitive));
  gtk_widget_set_sensitive (GIMP_SCALE_ENTRY_SPINBUTTON (adj),
                            (ui_vals->disc_status && features_are_sensitive));

  disc_toggle_data.scale = (gpointer) adj;

  disc_toggle_data.status = &(ui_vals->disc_status);

  g_signal_connect (G_OBJECT (disc_button), "toggled",
                    G_CALLBACK (callback_combo_set_sensitive),
                    (gpointer) (&disc_toggle_data));

  g_signal_connect (G_OBJECT (disc_button), "toggled",
                    G_CALLBACK (callback_disc_combo_set_sensitive_preview),
                    (gpointer) (&preview_data));

  /* Advanced Options */

  text = g_strdup_printf ("<b>%s</b>", _("Advanced Options"));
  options_expander = gtk_expander_new (text);
  gtk_expander_set_use_markup (GTK_EXPANDER (options_expander), TRUE);
  gtk_box_pack_start (GTK_BOX (main_vbox), options_expander, TRUE, TRUE, 0);
  g_free (text);
  gtk_widget_show (options_expander);

  expander_vbox = gtk_vbox_new (FALSE, 4);
  gtk_container_add (GTK_CONTAINER (options_expander), expander_vbox);
  gtk_widget_show (expander_vbox);

  expander_frame = gimp_frame_new ("<expander>");
  gtk_box_pack_start (GTK_BOX (expander_vbox), expander_frame, FALSE, FALSE,
                      0);
  gtk_widget_show (expander_frame);

  vbox = gtk_vbox_new (FALSE, 4);
  gtk_container_add (GTK_CONTAINER (expander_frame), vbox);
  gtk_widget_show (vbox);

  update_en_button =
    gtk_check_button_new_with_label (_("Update energy at every step"));

  gtk_box_pack_start (GTK_BOX (vbox), update_en_button, FALSE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (update_en_button),
                                vals->update_en);
  gtk_widget_show (update_en_button);

  resize_canvas_button =
    gtk_check_button_new_with_label (_("Resize image canvas"));

  gtk_box_pack_start (GTK_BOX (vbox), resize_canvas_button, FALSE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (resize_canvas_button),
                                vals->resize_canvas);
  gtk_widget_show (resize_canvas_button);

  resize_aux_layers_button =
    gtk_check_button_new_with_label (_("Resize preserve/discard layers"));

  gtk_box_pack_start (GTK_BOX (vbox), resize_aux_layers_button, FALSE, FALSE,
                      0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (resize_aux_layers_button),
                                vals->resize_aux_layers);

  presdisc_status.ui_vals = (gpointer) ui_vals;
  presdisc_status.button = (gpointer) resize_aux_layers_button;

  callback_resize_aux_layers_button_set_sensitive (NULL,
                                                   (gpointer)
                                                   (&presdisc_status));

  g_signal_connect (pres_button, "toggled",
                    G_CALLBACK
                    (callback_resize_aux_layers_button_set_sensitive),
                    (gpointer) (&presdisc_status));
  g_signal_connect (disc_button, "toggled",
                    G_CALLBACK
                    (callback_resize_aux_layers_button_set_sensitive),
                    (gpointer) (&presdisc_status));

  gtk_widget_show (resize_aux_layers_button);

  frame = gimp_frame_new (_("Select gradient function"));
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  grad_func_combo_box =
    gimp_int_combo_box_new (_("Transversal absolute value"), LQR_GF_XABS,
                            _("Sum of absolute values"), LQR_GF_SUMABS,
                            _("Norm"), LQR_GF_NORM,
                            /* Null can be translated as Zero */
                            _("Null"), LQR_GF_NULL, NULL);
  gimp_int_combo_box_set_active (GIMP_INT_COMBO_BOX (grad_func_combo_box),
                                 vals->grad_func);

  gtk_container_add (GTK_CONTAINER (frame), grad_func_combo_box);
  gtk_widget_show (grad_func_combo_box);



  /* Mask */

  /*
     if ((gimp_drawable_is_layer_mask (drawable->drawable_id) == TRUE) ||
     ((gimp_drawable_is_layer (drawable->drawable_id) == TRUE) &&
     (gimp_layer_get_mask (drawable->drawable_id) != -1)))
     {
     has_mask = TRUE;
     }
   */

  if (has_mask == TRUE)
    {
      frame = gimp_frame_new (_("Select behavior for the mask"));
      gtk_box_pack_start (GTK_BOX (main_vbox), frame, FALSE, FALSE, 0);
      gtk_widget_show (frame);

      mask_behavior_combo_box =
        gimp_int_combo_box_new (_("Apply"), GIMP_MASK_APPLY, _("Discard"),
                                GIMP_MASK_DISCARD, NULL);
      gimp_int_combo_box_set_active (GIMP_INT_COMBO_BOX
                                     (mask_behavior_combo_box),
                                     vals->mask_behavior);

      gtk_container_add (GTK_CONTAINER (frame), mask_behavior_combo_box);
      gtk_widget_show (mask_behavior_combo_box);
    }

  /*  Show the main containers  */

  gtk_widget_show (main_vbox);
  gtk_widget_show (dlg);

  run = (gimp_dialog_run (GIMP_DIALOG (dlg)) == GTK_RESPONSE_OK);

  if (run)
    {
      /*  Save ui values  */
      ui_state->chain_active =
        gimp_chain_button_get_active (GIMP_COORDINATES_CHAINBUTTON
                                      (coordinates));
      ui_vals->pres_status =
        gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (pres_button));
      ui_vals->disc_status =
        gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (disc_button));
      ui_vals->pres_layer_ID = ui_vals->pres_status ? vals->pres_layer_ID : 0;
      ui_vals->disc_layer_ID = ui_vals->disc_status ? vals->disc_layer_ID : 0;


      vals->new_width =
        (gint) gimp_size_entry_get_refval (GIMP_SIZE_ENTRY (coordinates), 0);
      vals->new_height =
        (gint) gimp_size_entry_get_refval (GIMP_SIZE_ENTRY (coordinates), 1);
      gimp_int_combo_box_get_active (GIMP_INT_COMBO_BOX (grad_func_combo_box),
                                     &(vals->grad_func));
      vals->update_en =
        gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (update_en_button));
      vals->resize_canvas =
        gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
                                      (resize_canvas_button));
      vals->resize_aux_layers =
        gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
                                      (resize_aux_layers_button));
      if (has_mask == TRUE)
        {
          gimp_int_combo_box_get_active (GIMP_INT_COMBO_BOX
                                         (mask_behavior_combo_box),
                                         &(vals->mask_behavior));
        }

    }

  gtk_widget_destroy (dlg);

  return run;
}


/*  Private functions  */

static gint
count_extra_layers (gint32 image_ID, GimpDrawable * drawable)
{
  gint32 *layer_array;
  gint num_layers;

  layer_array = gimp_image_get_layers (image_ID, &num_layers);
  return num_layers - 1;
}

static gboolean
dialog_layer_constraint_func (gint32 image_id, gint32 layer_id, gpointer data)
{
  if (image_id !=
      gimp_drawable_get_image (((GimpDrawable *) data)->drawable_id))
    {
      return FALSE;
    }
  if ((layer_id == ((GimpDrawable *) data)->drawable_id) ||
      (layer_id == preview_layer_ID))
    {
      return FALSE;
    }
  return TRUE;
}

static void
callback_pres_combo_get_active(GtkWidget *combo, gpointer data)
{
  gint32 pres_layer_ID;
  gint x_off, y_off;
  gimp_int_combo_box_get_active(GIMP_INT_COMBO_BOX(combo), &(PREVIEW_DATA(data)->vals->pres_layer_ID));
  if (PREVIEW_DATA(data)->ui_vals->pres_status == TRUE) {
	  pres_layer_ID = gimp_layer_copy (PREVIEW_DATA(data)->vals->pres_layer_ID);
	  gimp_image_add_layer (PREVIEW_DATA(data)->image_ID, pres_layer_ID, -1);
	  gimp_drawable_offsets(pres_layer_ID, &x_off, &y_off);
	  gimp_layer_resize(pres_layer_ID, PREVIEW_DATA(data)->old_width, PREVIEW_DATA(data)->old_height, 
			  x_off - PREVIEW_DATA(data)->x_off,
			  y_off - PREVIEW_DATA(data)->y_off);
	  gimp_layer_scale(pres_layer_ID, PREVIEW_DATA(data)->width, PREVIEW_DATA(data)->height, TRUE);
	  gimp_layer_add_alpha(pres_layer_ID);
	  PREVIEW_DATA(data)->pres_buffer = preview_build_buffer(PREVIEW_DATA(data)->pres_buffer, pres_layer_ID);
	  gimp_image_remove_layer(PREVIEW_DATA(data)->image_ID, pres_layer_ID);

  }
  preview_build_pixbuf(PREVIEW_DATA(data));
  gtk_widget_queue_draw(PREVIEW_DATA(data)->area);
}

static void
callback_disc_combo_get_active(GtkWidget *combo, gpointer data)
{
  gint32 disc_layer_ID;
  gint x_off, y_off;
  gimp_int_combo_box_get_active(GIMP_INT_COMBO_BOX(combo), &(PREVIEW_DATA(data)->vals->disc_layer_ID));
  if (PREVIEW_DATA(data)->ui_vals->disc_status == TRUE) {
	  disc_layer_ID = gimp_layer_copy (PREVIEW_DATA(data)->vals->disc_layer_ID);
	  gimp_image_add_layer (PREVIEW_DATA(data)->image_ID, disc_layer_ID, -1);
	  gimp_drawable_offsets(disc_layer_ID, &x_off, &y_off);
	  gimp_layer_resize(disc_layer_ID, PREVIEW_DATA(data)->old_width, PREVIEW_DATA(data)->old_height, 
			  x_off - PREVIEW_DATA(data)->x_off,
			  y_off - PREVIEW_DATA(data)->y_off);
	  gimp_layer_scale(disc_layer_ID, PREVIEW_DATA(data)->width, PREVIEW_DATA(data)->height, TRUE);
	  gimp_layer_add_alpha(disc_layer_ID);
	  PREVIEW_DATA(data)->disc_buffer = preview_build_buffer(PREVIEW_DATA(data)->disc_buffer, disc_layer_ID);
	  gimp_image_remove_layer(PREVIEW_DATA(data)->image_ID, disc_layer_ID);

  }
  preview_build_pixbuf(PREVIEW_DATA(data));
  gtk_widget_queue_draw(PREVIEW_DATA(data)->area);
}

static void
callback_combo_set_sensitive (GtkWidget * button, gpointer data)
{
  gboolean button_status =
    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));
  gtk_widget_set_sensitive ((GtkWidget *) (TOGGLE_DATA (data)->combo),
                            button_status);
  gtk_widget_set_sensitive ((GtkWidget *) (TOGGLE_DATA (data)->combo_label),
                            button_status);
  gtk_widget_set_sensitive (GIMP_SCALE_ENTRY_LABEL
                            (TOGGLE_DATA (data)->scale), button_status);
  gtk_widget_set_sensitive (GIMP_SCALE_ENTRY_SCALE
                            (TOGGLE_DATA (data)->scale), button_status);
  gtk_widget_set_sensitive (GIMP_SCALE_ENTRY_SPINBUTTON
                            (TOGGLE_DATA (data)->scale), button_status);
  *((gboolean *) (TOGGLE_DATA (data)->status)) = button_status;
}

static void
callback_pres_combo_set_sensitive_preview(GtkWidget * button, gpointer data)
{
  if (pres_combo_awaked == FALSE) {
	  g_signal_emit_by_name(G_OBJECT(PREVIEW_DATA(data)->pres_combo), "changed");
	  pres_combo_awaked = TRUE;
  }
  preview_build_pixbuf(PREVIEW_DATA(data));
  gtk_widget_queue_draw(PREVIEW_DATA(data)->area);
}

static void
callback_disc_combo_set_sensitive_preview(GtkWidget * button, gpointer data)
{
  if (disc_combo_awaked == FALSE) {
	  g_signal_emit_by_name(G_OBJECT(PREVIEW_DATA(data)->disc_combo), "changed");
	  disc_combo_awaked = TRUE;
  }
  preview_build_pixbuf(PREVIEW_DATA(data));
  gtk_widget_queue_draw(PREVIEW_DATA(data)->area);
}

static void
callback_resize_aux_layers_button_set_sensitive (GtkWidget * button,
                                                 gpointer data)
{
  if ((PLUGIN_UI_VALS (PRESDISC_STATUS (data)->ui_vals)->pres_status == TRUE)
      || (PLUGIN_UI_VALS (PRESDISC_STATUS (data)->ui_vals)->disc_status ==
          TRUE))
    {
      gtk_widget_set_sensitive ((GtkWidget *) (PRESDISC_STATUS (data)->
                                               button), TRUE);
    }
  else
    {
      gtk_widget_set_sensitive ((GtkWidget *) (PRESDISC_STATUS (data)->
                                               button), FALSE);
    }
}

static void
preview_init_mem(PreviewData * preview_data)
{
	int width, height;
	preview_data->buffer = NULL;
	preview_data->pres_buffer = NULL;
	preview_data->disc_buffer = NULL;
	preview_data->pixbuf = NULL;
	width = preview_data->width;
	height = preview_data->height;
	preview_data->preview_buffer = g_new(guchar, width * height * 4);
}


static guchar*
preview_build_buffer (guchar *buffer, gint32 layer_ID)
{

  gint x, y, k;
  gint width, height;
  GimpPixelRgn rgn_in;
  guchar *inrow;
  GimpDrawable *drawable;
  gboolean isgray;

  //printf("build buf!\n"); fflush(stdout);
 
  drawable = gimp_drawable_get (layer_ID);
  width = drawable->width;
  height = drawable->height;
  isgray = gimp_drawable_is_gray(drawable->drawable_id);

  gimp_pixel_rgn_init (&rgn_in, drawable, 0, 0, width, height, FALSE, FALSE);

  inrow = g_new (guchar, drawable->bpp * width);
  if (buffer != NULL) {
	  g_free(buffer);
  }
  buffer = g_new (guchar, 4 * width * height);

  for (y = 0; y < height; y++)
    {
      gimp_pixel_rgn_get_row (&rgn_in, inrow, 0, y, width);

      for (x = 0; x < width; x++)
        {
          for (k = 0; k < 3; k++)
            {
	      if (isgray) {
                buffer[(y * width + x) * 4 + k] =
                  inrow[2 * x];
	      } else {
                buffer[(y * width + x) * 4 + k] =
                  inrow[4 * x + k];
	      }
            }
         if (isgray) {
             buffer[(y * width + x) * 4 + 3] =
	       inrow[2 * x + 1];
          } else {
	   buffer[(y * width + x) * 4 + 3] =
	    inrow[4 * x + 3];
          }
        }

    }

  g_free (inrow);
  return buffer;
}

static void preview_build_pixbuf(PreviewData * preview_data)
{
	gint bpp;
	gint x, y, k;
	gint index, index1;
	gdouble tfactor_orig, tfactor_pres, tfactor_disc, tfactor;
	gdouble value;

	//printf("build pixbuf\n"); fflush(stdout);

	bpp = 4;

	for (y = 0; y < preview_data->height; y++) {
		for (x = 0; x < preview_data->width; x++) {
			index1 = (y * preview_data->width + x);
			tfactor_orig = 0;
			tfactor_pres = 1;
			tfactor_disc = 1;
			tfactor_orig = (gdouble) (255 - preview_data->buffer[index1 * bpp + bpp - 1]) / 255;
			if ((preview_data->pres_buffer != NULL) && (preview_data->ui_vals->pres_status == TRUE)) {
				tfactor_pres = (gdouble) (255 - preview_data->pres_buffer[index1 * bpp + bpp - 1] / 2) / 255;
			}
			if ((preview_data->disc_buffer != NULL) && (preview_data->ui_vals->disc_status == TRUE)) {
				tfactor_disc = (gdouble) (255 - 0.5 * preview_data->disc_buffer[(index1 + 1) * bpp - 1]) / 255;
			}
			tfactor = (1 - tfactor_orig) * tfactor_pres * tfactor_disc;
			for (k = 0; k < bpp; k++) {
				index = index1 * bpp + k;
				value = (tfactor * preview_data->buffer[index]);
				if (tfactor_pres < 1) {
					value += (guchar) (tfactor_disc * (1 - tfactor_pres) * preview_data->pres_buffer[index]);
				}
				if (tfactor_disc < 1) {
					value += (guchar) ((1 - tfactor_disc) * preview_data->disc_buffer[index]);
				}
				if (value > 255) {
					value = 255;
				}
				preview_data->preview_buffer[index] = (guchar) value;
			}
		}
	}
	if (preview_data->pixbuf != NULL) {
		g_object_unref(G_OBJECT(preview_data->pixbuf));
	}
	preview_data->pixbuf = gdk_pixbuf_new_from_data ( preview_data->preview_buffer,
			GDK_COLORSPACE_RGB,
			TRUE, 8,
			preview_data->width,
			preview_data->height,
			bpp *
			preview_data->width *
			sizeof (guchar), NULL, NULL);

	//printf("pixbuf built\n"); fflush(stdout);
}
			

static gboolean
preview_expose_event_callback (GtkWidget * preview_area,
                               GdkEventExpose * event, gpointer data)
{
  //printf("expose!\n"); fflush(stdout);

  gdk_draw_pixbuf (PREVIEW_DATA (data)->area->window, NULL,
                   PREVIEW_DATA (data)->pixbuf, 0, 0, 0, 0, -1, -1,
                   GDK_RGB_DITHER_NORMAL, 0, 0);

  return TRUE;
}
