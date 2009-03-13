/* 
 * Wavelet denoise GIMP plugin
 * 
 * interface.c
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
#include "interface.h"

GtkWidget **radios_labels[] = { channel_radio, thr_label };

gboolean
user_interface (GimpDrawable * drawable)
{
  /* can ui code be beautiful? */
  GtkRequisition req;
  gboolean run;
  gint i;

  if (channels < 3)
    {
      names = names_gray;
    }
  else
    {
      if (settings.colour_mode == MODE_YCBCR)
	names = names_ycbcr;
      else if (settings.colour_mode == MODE_LAB)
	names = names_lab;
      else
	names = names_rgb;
    }

  gimp_ui_init (PLUGIN_NAME, FALSE);

  /* prepare the preview */
  preview = gimp_drawable_preview_new (drawable, &settings.preview);
  preview_hbox = gimp_preview_get_controls ((GimpPreview *) preview);
  g_signal_connect_swapped (preview, "invalidated", G_CALLBACK (denoise),
			    drawable);
  preview_check = gtk_container_get_children(GTK_CONTAINER(preview_hbox))->data;
  gtk_widget_show (preview);

  /* prepare the colour mode frame */
  if (channels > 2)
    {
      fr_mode = gtk_frame_new (_("Color model"));
      mode_vbox = gtk_vbox_new (FALSE, 0);
      gtk_container_border_width (GTK_CONTAINER (mode_vbox), 5);
      mode_radio[0] = gtk_radio_button_new_with_label (NULL,
						       channels ==
						       3 ? "YCbCr" :
						       "YCbCr(A)");
      mode_list =
	gtk_radio_button_get_group (GTK_RADIO_BUTTON (mode_radio[0]));
      mode_radio[1] =
	gtk_radio_button_new_with_label (mode_list,
					 channels ==
					 3 ? "CIELAB" : "CIELAB(A)");
      mode_list =
	gtk_radio_button_get_group (GTK_RADIO_BUTTON (mode_radio[1]));
      mode_radio[2] =
	gtk_radio_button_new_with_label (mode_list,
					 channels == 3 ? "RGB" : "RGB(A)");
      if (settings.colour_mode == MODE_YCBCR)
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mode_radio[0]), 1);
      else if (settings.colour_mode == MODE_RGB)
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mode_radio[1]), 1);
      else
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mode_radio[2]), 1);
      g_signal_connect (mode_radio[0], "toggled",
			G_CALLBACK (set_ycbcr_mode), radios_labels);
      g_signal_connect (mode_radio[1], "toggled",
			G_CALLBACK (set_lab_mode), radios_labels);
      g_signal_connect (mode_radio[2], "toggled",
			G_CALLBACK (set_rgb_mode), radios_labels);
      g_signal_connect_swapped (mode_radio[0], "toggled",
				G_CALLBACK (gimp_preview_invalidate),
				preview);
      g_signal_connect_swapped (mode_radio[1], "toggled",
				G_CALLBACK (gimp_preview_invalidate),
				preview);
      g_signal_connect_swapped (mode_radio[2], "toggled",
				G_CALLBACK (gimp_preview_invalidate),
				preview);
      gtk_container_add (GTK_CONTAINER (fr_mode), mode_vbox);
      gtk_box_pack_start (GTK_BOX (mode_vbox), mode_radio[0], FALSE,
			  FALSE, 0);
      gtk_box_pack_start (GTK_BOX (mode_vbox), mode_radio[1], FALSE,
			  FALSE, 0);
      gtk_box_pack_start (GTK_BOX (mode_vbox), mode_radio[2], FALSE,
			  FALSE, 0);
      gtk_widget_set_tooltip_text (mode_radio[0], TT_MODEL_YCBCR);
      gtk_widget_set_tooltip_text (mode_radio[1], TT_MODEL_LAB);
      gtk_widget_set_tooltip_text (mode_radio[2], TT_MODEL_RGB);
      gtk_widget_show (mode_radio[0]);
      gtk_widget_show (mode_radio[1]);
      gtk_widget_show (mode_radio[2]);
      gtk_widget_show (mode_vbox);
      gtk_widget_show (fr_mode);
    }
  else
    fr_mode = NULL;

  /* prepare the preview select frame */
  if (channels > 1)
    {
      fr_preview = gtk_frame_new (_("Preview channel"));
      preview_vbox = gtk_vbox_new (FALSE, 0);
      gtk_container_border_width (GTK_CONTAINER (preview_vbox), 5);
      gtk_container_add (GTK_CONTAINER (fr_preview), preview_vbox);

      preview_list = NULL;
      /* TRANSLATORS: *All* channels (from the preview select frame) */
      preview_radio[0] = gtk_radio_button_new_with_label (preview_list,
							  _("All"));
      if (settings.preview_mode == 0)
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (preview_radio[0]),
				      1);
      gtk_box_pack_start (GTK_BOX (preview_vbox), preview_radio[0], FALSE,
			  FALSE, 0);
      g_signal_connect (preview_radio[0], "toggled",
			G_CALLBACK (set_preview_mode), (gpointer) 0);
      g_signal_connect_swapped (preview_radio[0], "toggled",
				G_CALLBACK (gimp_preview_invalidate),
				preview);
      gtk_widget_set_tooltip_text (preview_radio[0], TT_PREVIEW_ALL);
      gtk_widget_show (preview_radio[0]);

      preview_list =
	gtk_radio_button_get_group (GTK_RADIO_BUTTON (preview_radio[0]));
      preview_radio[1] =
	gtk_radio_button_new_with_label (preview_list,
					 (channels >
					  2) ? _("Selected (gray)") :
					 _("Selected"));
      if (settings.preview_mode == 1)
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (preview_radio[1]),
				      1);
      gtk_box_pack_start (GTK_BOX (preview_vbox), preview_radio[1], FALSE,
			  FALSE, 0);
      g_signal_connect (preview_radio[1], "toggled",
			G_CALLBACK (set_preview_mode), (gpointer) 1);
      g_signal_connect_swapped (preview_radio[1], "toggled",
				G_CALLBACK (gimp_preview_invalidate),
				preview);
      gtk_widget_set_tooltip_text (preview_radio[1], TT_PREVIEW_SEL_GRAY);
      gtk_widget_show (preview_radio[1]);

      if (channels > 2)
	{
	  preview_list =
	    gtk_radio_button_get_group (GTK_RADIO_BUTTON (preview_radio[1]));
	  preview_radio[2] =
	    gtk_radio_button_new_with_label (preview_list,
					     _("Selected (color)"));
	  if (settings.preview_mode == 2)
	    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
					  (preview_radio[2]), 1);
	  gtk_box_pack_start (GTK_BOX (preview_vbox), preview_radio[2], FALSE,
			      FALSE, 0);
	  g_signal_connect (preview_radio[2], "toggled",
			    G_CALLBACK (set_preview_mode), (gpointer) 2);
	  g_signal_connect_swapped (preview_radio[2], "toggled",
				    G_CALLBACK (gimp_preview_invalidate),
				    preview);
	  gtk_widget_set_tooltip_text (preview_radio[2],
				       TT_PREVIEW_SEL_COLOUR);
	  gtk_widget_show (preview_radio[2]);
	}
      gtk_widget_show (preview_vbox);
      gtk_widget_show (fr_preview);
    }
  else
    fr_preview = NULL;

  /* prepare the channel select frame */
  if (channels > 1)
    {
      fr_channel = gtk_frame_new (_("Channel select"));
      channel_vbox = gtk_vbox_new (FALSE, 0);
      gtk_container_border_width (GTK_CONTAINER (channel_vbox), 5);
      gtk_container_add (GTK_CONTAINER (fr_channel), channel_vbox);

      channel_list = NULL;
      for (i = 0; i < channels; i++)
	{
	  channel_radio[i] =
	    gtk_radio_button_new_with_label (channel_list, names[i]);
	  if (settings.preview_channel == i)
	    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
					  (channel_radio[i]), 1);
	  g_signal_connect (channel_radio[i], "toggled",
			    G_CALLBACK (set_preview_channel), (gpointer) i);
	  g_signal_connect_swapped (channel_radio[i], "toggled",
				    G_CALLBACK (gimp_preview_invalidate),
				    preview);
	  gtk_box_pack_start (GTK_BOX (channel_vbox), channel_radio[i], FALSE,
			      FALSE, 0);
	  gtk_widget_set_tooltip_text (channel_radio[i], TT_SELECT);
	  gtk_widget_show (channel_radio[i]);
	  channel_list =
	    gtk_radio_button_get_group (GTK_RADIO_BUTTON (channel_radio[i]));
	}
      gtk_widget_show (channel_vbox);
      gtk_widget_show (fr_channel);
    }
  else
    fr_channel = NULL;

  /* prepare the threshold frame */
  fr_threshold = gtk_frame_new ((channels > 1) ? _("Channel settings") :
				/* TRANSLATORS: Channel settings without the word 'channel'. */
				_("Settings"));
  gtk_widget_show (fr_threshold);
  thr_vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (fr_threshold), thr_vbox);
  gtk_container_border_width (GTK_CONTAINER (thr_vbox), 5);
  gtk_widget_show (thr_vbox);

  thr_hbox[0] = gtk_hbox_new (FALSE, 10);
  thr_label[0] = gtk_label_new (_("Threshold"));
  gtk_misc_set_alignment (GTK_MISC (thr_label[0]), 0.0, 0.0);
  if (channels > 2)
    thr_adj[0] =
      gtk_adjustment_new (settings.colour_thresholds
			  [settings.preview_channel], 0, 10, 0.01, 0.01, 0);
  else
    thr_adj[0] =
      gtk_adjustment_new (settings.gray_thresholds[settings.preview_channel],
			  0, 10, 0.01, 0.01, 0);
  thr_spin[0] = gtk_spin_button_new (GTK_ADJUSTMENT (thr_adj[0]), 0.01, 2);
  thr_scale[0] = gtk_hscale_new (GTK_ADJUSTMENT (thr_adj[0]));
  gtk_scale_set_draw_value (GTK_SCALE (thr_scale[0]), FALSE);
  gtk_box_pack_start (GTK_BOX (thr_vbox), thr_hbox[0], FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (thr_hbox[0]), thr_label[0], FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (thr_hbox[0]), thr_scale[0], TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (thr_hbox[0]), thr_spin[0], FALSE, FALSE, 0);
  g_signal_connect_swapped (thr_adj[0], "value_changed",
			    G_CALLBACK (gimp_preview_invalidate), preview);
  g_signal_connect (thr_adj[0], "value_changed",
		    G_CALLBACK (set_threshold), NULL);
  gtk_widget_size_request(thr_scale[0], &req);
  gtk_widget_set_size_request(thr_scale[0], 3 * req.width, req.height);
  gtk_widget_show (thr_scale[0]);
  gtk_widget_show (thr_spin[0]);
  gtk_widget_show (thr_label[0]);
  if (channels > 1)
    {
      gtk_widget_set_tooltip_text (thr_label[0], TT_THR_AMOUNT_COLOUR);
      gtk_widget_set_tooltip_text (thr_scale[0], TT_THR_AMOUNT_COLOUR);
      gtk_widget_set_tooltip_text (thr_spin[0], TT_THR_AMOUNT_COLOUR);
    }
  else
    {
      gtk_widget_set_tooltip_text (thr_label[0], TT_THR_AMOUNT_GRAY);
      gtk_widget_set_tooltip_text (thr_scale[0], TT_THR_AMOUNT_GRAY);
      gtk_widget_set_tooltip_text (thr_spin[0], TT_THR_AMOUNT_GRAY);
    }
  gtk_widget_show (thr_hbox[0]);

  thr_hbox[1] = gtk_hbox_new (FALSE, 10);
  thr_label[1] = gtk_label_new (_("Softness"));
  gtk_misc_set_alignment (GTK_MISC (thr_label[1]), 0.0, 0.0);
  if (channels > 2)
    thr_adj[1] =
      gtk_adjustment_new (settings.colour_low[settings.preview_channel], 0,
			  1, 0.01, 0.01, 0);
  else
    thr_adj[1] =
      gtk_adjustment_new (settings.gray_low[settings.preview_channel],
			  0, 1, 0.01, 0.01, 0);
  thr_spin[1] = gtk_spin_button_new (GTK_ADJUSTMENT (thr_adj[1]), 0.01, 2);
  thr_scale[1] = gtk_hscale_new (GTK_ADJUSTMENT (thr_adj[1]));
  gtk_scale_set_draw_value (GTK_SCALE (thr_scale[1]), FALSE);
  gtk_box_pack_start (GTK_BOX (thr_vbox), thr_hbox[1], FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (thr_hbox[1]), thr_label[1], FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (thr_hbox[1]), thr_scale[1], TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (thr_hbox[1]), thr_spin[1], FALSE, FALSE, 0);
  g_signal_connect_swapped (thr_adj[1], "value_changed",
			    G_CALLBACK (gimp_preview_invalidate), preview);
  g_signal_connect (thr_adj[1], "value_changed", G_CALLBACK (set_low), NULL);
  gtk_widget_size_request(thr_scale[1], &req);
  gtk_widget_set_size_request(thr_scale[1], 3 * req.width, req.height);
  gtk_widget_show (thr_scale[1]);
  gtk_widget_show (thr_spin[1]);
  gtk_widget_show (thr_label[1]);
  if (channels > 1)
    {
      gtk_widget_set_tooltip_text (thr_label[1], TT_THR_DETAIL_COLOUR);
      gtk_widget_set_tooltip_text (thr_scale[1], TT_THR_DETAIL_COLOUR);
      gtk_widget_set_tooltip_text (thr_spin[1], TT_THR_DETAIL_COLOUR);
    }
  else
    {
      gtk_widget_set_tooltip_text (thr_label[1], TT_THR_DETAIL_GRAY);
      gtk_widget_set_tooltip_text (thr_scale[1], TT_THR_DETAIL_GRAY);
      gtk_widget_set_tooltip_text (thr_spin[1], TT_THR_DETAIL_GRAY);
    }
  gtk_widget_show (thr_hbox[1]);

  /* prepare the reset buttons */
  reset_hbox = gtk_hbox_new (FALSE, 10);
  if (channels > 1)
    {
      preview_reset_icon =
	gtk_image_new_from_stock (GTK_STOCK_GO_BACK, GTK_ICON_SIZE_BUTTON);
      //preview_reset = gtk_button_new_with_label(_("Temporary reset"));
      preview_reset = gtk_button_new ();
      gtk_button_set_image (GTK_BUTTON (preview_reset), preview_reset_icon);
      g_signal_connect (preview_reset, "pressed",
			G_CALLBACK (temporarily_reset), (gpointer) 1);
      g_signal_connect (preview_reset, "released",
			G_CALLBACK (temporarily_reset), (gpointer) 0);
      gtk_box_pack_start (GTK_BOX (reset_hbox), preview_reset, TRUE, TRUE, 0);
      gtk_widget_set_tooltip_text (preview_reset, TT_RESET_PREVIEW);
      gtk_widget_show (preview_reset);
      gtk_widget_show (preview_reset_icon);
    }

  reset_button_icon[0] =
    gtk_image_new_from_stock (GIMP_STOCK_RESET, GTK_ICON_SIZE_BUTTON);
  reset_button[0] =
    gtk_button_new_with_label ((channels >
				1) ? _("Reset channel") : _("Reset"));
  gtk_button_set_image (GTK_BUTTON (reset_button[0]), reset_button_icon[0]);
  g_signal_connect (reset_button[0], "clicked", G_CALLBACK (reset_channel),
		    NULL);
  gtk_box_pack_start (GTK_BOX (reset_hbox), reset_button[0], FALSE, FALSE, 0);
  gtk_widget_set_tooltip_text (reset_button[0],
			       (channels >
				1) ? TT_RESET_CHANNEL_COLOUR :
			       TT_RESET_CHANNEL_GRAY);
  gtk_widget_show (reset_button[0]);

  if (channels > 1)
    {
      /* TRANSLATORS: Reset all [channels] */
      reset_button_icon[1] =
	gtk_image_new_from_stock (GIMP_STOCK_RESET, GTK_ICON_SIZE_BUTTON);
      reset_button[1] = gtk_button_new_with_label (_("Reset all"));
      gtk_button_set_image (GTK_BUTTON (reset_button[1]),
			    reset_button_icon[1]);
      gtk_button_set_use_stock (GTK_BUTTON (reset_button[1]), TRUE);
      g_signal_connect (reset_button[1], "clicked", G_CALLBACK (reset_all),
			NULL);
      g_signal_connect_swapped (reset_button[1], "clicked",
				G_CALLBACK (gimp_preview_invalidate),
				preview);
      gtk_box_pack_start (GTK_BOX (reset_hbox), reset_button[1], FALSE, FALSE,
			  0);
      gtk_widget_set_tooltip_text (reset_button[1], TT_RESET_ALL);
      gtk_widget_show (reset_button[1]);
    }
  gtk_widget_show (reset_hbox);

  /* prepeare the dialog boxes */
  frame_hbox = gtk_hbox_new (FALSE, 10);
  gtk_box_set_homogeneous (GTK_BOX (frame_hbox), FALSE);
  dialog_vbox = gtk_vbox_new (FALSE, 10);
  dialog_hbox = gtk_hbox_new (FALSE, 10);
  gtk_container_set_border_width (GTK_CONTAINER (dialog_hbox), 10);

  gtk_box_pack_start (GTK_BOX (dialog_hbox), preview, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (dialog_hbox), dialog_vbox, FALSE, FALSE, 0);

  gtk_box_pack_start (GTK_BOX (dialog_vbox), frame_hbox, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), fr_threshold, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), reset_hbox, FALSE, FALSE, 0);
  /* avoid destruction of the preview check widget and reparent it */
  g_object_ref(preview_check);
  gtk_container_remove(GTK_CONTAINER(preview_hbox), preview_check);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), preview_check, FALSE, FALSE, 0);
  g_object_unref(preview_check);

  if (fr_preview)
    gtk_box_pack_start (GTK_BOX (frame_hbox), fr_preview, TRUE, TRUE, 0);
  if (fr_channel)
    gtk_box_pack_start (GTK_BOX (frame_hbox), fr_channel, TRUE, TRUE, 0);
  if (fr_mode)
    gtk_box_pack_start (GTK_BOX (frame_hbox), fr_mode, TRUE, TRUE, 0);

  /* prepeare the dialog */
  dialog = gimp_dialog_new (PLUGIN_NAME, "wavelet denoise", NULL, 0,
			    gimp_standard_help_func,
			    "plug-in-wavelet-denoise", GTK_STOCK_CANCEL,
			    GTK_RESPONSE_CANCEL, GTK_STOCK_OK,
			    GTK_RESPONSE_OK, NULL);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), dialog_hbox);

  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);

  /* size required for proper noise profiling */
  gtk_widget_set_size_request(preview, 300, 304);

  if (settings.winxsize > 0 && settings.winysize > 0)
    gtk_window_resize (GTK_WINDOW (dialog), settings.winxsize,
		       settings.winysize);

  gtk_widget_show (frame_hbox);
  gtk_widget_show (dialog_hbox);
  gtk_widget_show (dialog_vbox);
  gtk_widget_show (dialog);

  run = (gimp_dialog_run (GIMP_DIALOG (dialog)) == GTK_RESPONSE_OK);

  gtk_window_get_size (GTK_WINDOW (dialog), &(settings.winxsize),
		       &(settings.winysize));

  /* FIXME: destroy all widgets - memory leak! */
  gtk_widget_destroy (dialog);

  return run;
}
