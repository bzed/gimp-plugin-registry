/* 
 * Wavelet decompose GIMP plugin
 * 
 * interface.c
 * Copyright 2008 by Marco Rossini
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2+
 * as published by the Free Software Foundation.
 */

#include "plugin.h"

#define SCALES_TT _("Sets the number of detail scales for the wavelet decomposition. This value is restricted by the size of the image. Two to the power of the maximal scale must be smaller or equal to the shortest edge.")
#define MODE_TT _("If checked (default) the layer modes of the detail scales are set such that the image is automatically recomposed. If you want to see the individual scales by themselves uncheck this.")
#define NEW_TT _("Creates a new image with the wavelet decomposition in it. This does not alter the original image.")
#define ALPHA_TT _("Always adds an alpha channel to each detail scale layer, regardless on whether the original layer had one or not.")

static void
scale_update (GtkWidget * w, gpointer * p)
{
  settings.scales = gtk_adjustment_get_value (GTK_ADJUSTMENT (w));
}

static void
mode_update (GtkWidget * w, gpointer * p)
{
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w)))
    settings.layer_modes = GIMP_GRAIN_MERGE_MODE;
  else
    settings.layer_modes = GIMP_NORMAL_MODE;
}

static void
toggle_update_bool (GtkWidget * w, gpointer * p)
{
  int *bool;
  bool = (int *) p;
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w)))
    *bool = 1;
  else
    *bool = 0;
}

gboolean
user_interface (GimpDrawable * drawable, unsigned int maxscale)
{
  gboolean run;

  GtkWidget *dialog;
  GtkWidget *dialog_box;
  GtkWidget *scales_spin, *scales_label, *scales_box, *scales_align;
  GtkObject *scales_adj;
  GtkWidget *mode_check;
  GtkWidget *alpha_check;
  GtkWidget *new_check;

  gimp_ui_init (_("Wavelet decompose"), FALSE);

  scales_box = gtk_hbox_new (FALSE, 10);
  /* TRANSLATORS: wavlet scales is a technical term */
  scales_label = gtk_label_new (_("Number of wavelet detail scales:"));
  scales_adj = gtk_adjustment_new (settings.scales, 1, maxscale, 1, 1, 0);
  scales_spin = gtk_spin_button_new (GTK_ADJUSTMENT (scales_adj), 1, 0);
  gtk_widget_set_tooltip_text (scales_spin, SCALES_TT);
  gtk_widget_set_tooltip_text (scales_label, SCALES_TT);
  scales_align = gtk_alignment_new (0, 0.5, 0, 0);
  gtk_container_add (GTK_CONTAINER (scales_align), scales_label);
  g_signal_connect (scales_adj, "value_changed", G_CALLBACK (scale_update),
		    NULL);
  gtk_box_pack_start (GTK_BOX (scales_box), scales_align, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (scales_box), scales_spin, FALSE, FALSE, 0);
  gtk_widget_show (scales_box);
  gtk_widget_show (scales_label);
  gtk_widget_show (scales_spin);
  gtk_widget_show (scales_align);

  /* prepare layer mode checkbutton */
  mode_check =
    gtk_check_button_new_with_label (_("Set layer modes for recomposition"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mode_check),
				settings.layer_modes ==
				GIMP_GRAIN_MERGE_MODE);
  gtk_widget_set_tooltip_text (mode_check, MODE_TT);
  g_signal_connect (mode_check, "toggled", G_CALLBACK (mode_update), NULL);
  gtk_widget_show (mode_check);

  /* prepare alpha add checkbutton */
  alpha_check =
    gtk_check_button_new_with_label (_
				     ("Add alpha channels to detail scale layers"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (alpha_check),
				settings.add_alpha);
  gtk_widget_set_tooltip_text (alpha_check, ALPHA_TT);
  g_signal_connect (alpha_check, "toggled", G_CALLBACK (toggle_update_bool),
		    &(settings.add_alpha));
  gtk_widget_show (alpha_check);

  /* prepare new image checkbutton */
  new_check = gtk_check_button_new_with_label (_("Create new image"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (new_check),
				settings.new_image);
  gtk_widget_set_tooltip_text (new_check, NEW_TT);
  g_signal_connect (new_check, "toggled", G_CALLBACK (toggle_update_bool),
		    &(settings.new_image));
  gtk_widget_show (new_check);

  /* dialog box */
  dialog_box = gtk_vbox_new (FALSE, 10);
  gtk_container_set_border_width (GTK_CONTAINER (dialog_box), 10);
  gtk_box_pack_start (GTK_BOX (dialog_box), scales_box, FALSE, FALSE, 10);
  gtk_box_pack_start (GTK_BOX (dialog_box), mode_check, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (dialog_box), alpha_check, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (dialog_box), new_check, FALSE, FALSE, 0);
  gtk_widget_show (dialog_box);

  /* prepeare the dialog */
  dialog =
    gimp_dialog_new (_("Wavelet decompose"), "wavelet decompose", NULL, 0,
		     gimp_standard_help_func, "plug-in-wavelet-decompose",
		     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OK,
		     GTK_RESPONSE_OK, NULL);

  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), dialog_box);

  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);

  gtk_widget_show (dialog);

  run = (gimp_dialog_run (GIMP_DIALOG (dialog)) == GTK_RESPONSE_OK);

  /* FIXME: destroy all widgets - memory leak! */
  gtk_widget_destroy (dialog);

  return run;
}
