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

/*  Local function prototypes  */

/*
static gboolean   dialog_image_constraint_func (gint32    image_id,
                                                gpointer  data);
*/
static gboolean dialog_layer_constraint_func (gint32 image_id,
                                              gint32 layer_id, gpointer data);


/*  Local variables  */

static PlugInUIVals *ui_state = NULL;


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
  GtkWidget *dlg;
  GtkWidget *main_vbox;
  GtkWidget *frame;
  GtkWidget *table;
  GtkWidget *hbox;
  //GtkWidget *hbox2;
  GtkWidget *coordinates;
  GtkWidget *pres_vbox;
  GtkWidget *pres_button;
  GtkWidget *disc_vbox;
  GtkWidget *disc_button;
  GtkWidget *grad_func_combo_box;
  GtkWidget *vbox;
  GtkWidget *update_en_button;
  GtkWidget *resize_canvas_button;
  GtkWidget *mask_behavior_combo_box = NULL;
  gboolean has_mask = FALSE;
  GtkWidget *combo;
  GtkObject *adj;
  gint row;
  gboolean run = FALSE;
  GimpUnit unit;
  gdouble xres, yres;


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

  vals->new_width = gimp_drawable_width (drawable->drawable_id);
  vals->new_height = gimp_drawable_height (drawable->drawable_id);

  ui_state = ui_vals;

  gimp_ui_init (PLUGIN_NAME, TRUE);

  dlg = gimp_dialog_new (_("GIMP LiquidRescale Plug-In"), PLUGIN_NAME,
                         NULL, 0,
                         gimp_standard_help_func, "lqr-plug-in",
                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                         GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);

  main_vbox = gtk_vbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 12);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dlg)->vbox), main_vbox);

  /*  scale entries to select width and height */

  /*

     frame = gimp_frame_new (_("Select new width and height"));
     gtk_box_pack_start (GTK_BOX (main_vbox), frame, FALSE, FALSE, 0);
     gtk_widget_show (frame);

     table = gtk_table_new (3, 2, FALSE);
     gtk_table_set_col_spacings (GTK_TABLE (table), 6);
     gtk_table_set_row_spacings (GTK_TABLE (table), 2);
     gtk_container_add (GTK_CONTAINER (frame), table);
     gtk_widget_show (table);

     row = 0;

     adj = gimp_scale_entry_new (GTK_TABLE (table), 0, row++,
     _("Width:"), SCALE_WIDTH, SPIN_BUTTON_WIDTH,
     vals->new_width, 2, 2 * vals->new_width - 1, 1, 10, 0,
     TRUE, 0, 0,
     _("Final width"), NULL);
     g_signal_connect (adj, "value_changed",
     G_CALLBACK (gimp_int_adjustment_update),
     &vals->new_width);

     adj = gimp_scale_entry_new (GTK_TABLE (table), 0, row++,
     _("Height:"), SCALE_WIDTH, SPIN_BUTTON_WIDTH,
     vals->new_height, 2, 2 * vals->new_height - 1, 1, 10, 0,
     TRUE, 0, 0,
     _("Final height"), NULL);
     g_signal_connect (adj, "value_changed",
     G_CALLBACK (gimp_int_adjustment_update),
     &vals->new_height);

   */

  /*  coordinates widget for selecting new size  */

  frame = gimp_frame_new (_("Select new width and height"));
  gtk_box_pack_start (GTK_BOX (main_vbox), frame, FALSE, FALSE, 0);
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
                                drawable_vals->pres_status);

  gtk_box_pack_start (GTK_BOX (pres_vbox), pres_button, FALSE, FALSE, 0);
  gtk_widget_show (pres_button);

  table = gtk_table_new (1, 2, FALSE);
  gtk_container_set_border_width (GTK_CONTAINER (table), 4);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  gtk_table_set_row_spacings (GTK_TABLE (table), 2);
  gtk_box_pack_start(GTK_BOX (pres_vbox), table, FALSE, FALSE, 0);
  gtk_widget_show (table);

  row = 0;

  combo =
    gimp_layer_combo_box_new (dialog_layer_constraint_func,
                              (gpointer *) drawable);
  gimp_int_combo_box_connect (GIMP_INT_COMBO_BOX (combo),
                              drawable->drawable_id,
                              G_CALLBACK (gimp_int_combo_box_get_active),
                              &vals->pres_layer_ID);

  gimp_table_attach_aligned (GTK_TABLE (table), 0, row++,
                             _("Available Layers:"), 0.0, 0.5, combo, 1,
                             FALSE);

  table = gtk_table_new (1, 2, FALSE);
  gtk_container_set_border_width (GTK_CONTAINER (table), 4);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  gtk_table_set_row_spacings (GTK_TABLE (table), 2);
  gtk_box_pack_start(GTK_BOX (pres_vbox), table, FALSE, FALSE, 0);
  gtk_widget_show (table);

     adj = gimp_scale_entry_new (GTK_TABLE (table), 0, row++,
     				_("Intensity:"), SCALE_WIDTH, SPIN_BUTTON_WIDTH,
     				vals->pres_coeff, 0, MAX_COEFF, 1, 10, 0,
     				TRUE, 0, 0,
     				_("Overall coefficient for feature preservation intensity"), NULL);
     g_signal_connect (adj, "value_changed",
     			G_CALLBACK (gimp_int_adjustment_update),
    	 		&vals->pres_coeff);



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
                                drawable_vals->disc_status);

  gtk_box_pack_start (GTK_BOX (disc_vbox), disc_button, FALSE, FALSE, 0);
  gtk_widget_show (disc_button);



  table = gtk_table_new (1, 2, FALSE);
  gtk_container_set_border_width (GTK_CONTAINER (table), 4);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  gtk_table_set_row_spacings (GTK_TABLE (table), 2);
  gtk_box_pack_start(GTK_BOX (disc_vbox), table, FALSE, FALSE, 0);
  gtk_widget_show (table);

  row = 0;

  combo =
    gimp_layer_combo_box_new (dialog_layer_constraint_func,
                              (gpointer *) drawable);
  gimp_int_combo_box_connect (GIMP_INT_COMBO_BOX (combo),
                              drawable->drawable_id,
                              G_CALLBACK (gimp_int_combo_box_get_active),
                              &vals->disc_layer_ID);

  gimp_table_attach_aligned (GTK_TABLE (table), 0, row++,
                             _("Available Layers:"), 0.0, 0.5, combo, 1,
                             FALSE);

  table = gtk_table_new (1, 2, FALSE);
  gtk_container_set_border_width (GTK_CONTAINER (table), 4);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  gtk_table_set_row_spacings (GTK_TABLE (table), 2);
  gtk_box_pack_start(GTK_BOX (disc_vbox), table, FALSE, FALSE, 0);
  gtk_widget_show (table);

     adj = gimp_scale_entry_new (GTK_TABLE (table), 0, 0,
     				_("Intensity:"), SCALE_WIDTH, SPIN_BUTTON_WIDTH,
     				vals->disc_coeff, 0, MAX_COEFF, 1, 10, 0,
     				TRUE, 0, 0,
     				_("Overall coefficient for feature discard intensity"), NULL);
     g_signal_connect (adj, "value_changed",
     			G_CALLBACK (gimp_int_adjustment_update),
    	 		&vals->disc_coeff);




  /* Gradient function */

  frame = gimp_frame_new (_("Select gradient function"));
  gtk_box_pack_start (GTK_BOX (main_vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  grad_func_combo_box =
    gimp_int_combo_box_new (_("Sum of absolute values"), LQR_GF_SUMABS,
                            _("Transversal absolute value"), LQR_GF_XABS,
                            _("Norm"), LQR_GF_NORM, NULL);
  gimp_int_combo_box_set_active (GIMP_INT_COMBO_BOX (grad_func_combo_box),
                                 vals->grad_func);

  gtk_container_add (GTK_CONTAINER (frame), grad_func_combo_box);
  gtk_widget_show (grad_func_combo_box);

  /* Options */

  frame = gimp_frame_new (_("Options"));
  gtk_box_pack_start (GTK_BOX (main_vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  vbox = gtk_vbox_new (FALSE, 4);
  gtk_container_add (GTK_CONTAINER (frame), vbox);
  gtk_widget_show (vbox);

  update_en_button =
    gtk_check_button_new_with_label (_("Update energy at every step"));

  //gtk_container_add (GTK_CONTAINER (frame), update_en_button);
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


  /* Mask */

  if ((gimp_drawable_is_layer_mask (drawable->drawable_id) == TRUE) ||
      ((gimp_drawable_is_layer (drawable->drawable_id) == TRUE) &&
       (gimp_layer_get_mask (drawable->drawable_id) != -1)))
    {
      has_mask = TRUE;
    }

  if (has_mask == TRUE)
    {
      frame = gimp_frame_new (_("Select behavior for masks"));
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

  /*
     combo = gimp_image_combo_box_new (dialog_image_constraint_func, NULL);
     gimp_int_combo_box_connect (GIMP_INT_COMBO_BOX (combo), image_ID,
     G_CALLBACK (gimp_int_combo_box_get_active),
     &image_vals->image_id);

     gimp_table_attach_aligned (GTK_TABLE (table), 0, row++,
     _("RGB Images:"), 0.0, 0.5, combo, 1, FALSE);
   */


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

      vals->new_width =
        (gint) gimp_size_entry_get_refval (GIMP_SIZE_ENTRY (coordinates), 0);
      vals->new_height =
        (gint) gimp_size_entry_get_refval (GIMP_SIZE_ENTRY (coordinates), 1);
      drawable_vals->pres_status =
        gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (pres_button));
      drawable_vals->disc_status =
        gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (disc_button));

      gimp_int_combo_box_get_active (GIMP_INT_COMBO_BOX (grad_func_combo_box),
                                     &(vals->grad_func));
      vals->update_en =
        gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (update_en_button));
      vals->resize_canvas =
        gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
                                      (resize_canvas_button));
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

static gboolean
dialog_layer_constraint_func (gint32 image_id, gint32 layer_id, gpointer data)
{
  if (image_id !=
      gimp_drawable_get_image (((GimpDrawable *) data)->drawable_id))
    {
      return FALSE;
    }
  if (layer_id == ((GimpDrawable *) data)->drawable_id)
    {
      return FALSE;
    }
  return TRUE;
}

/*
static gboolean
dialog_image_constraint_func (gint32    image_id,
                              gpointer  data)
{
  return (gimp_image_base_type (image_id) == GIMP_RGB);
}
*/
