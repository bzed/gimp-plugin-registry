/* 
 * Wavelet denoise GIMP plugin
 * 
 * events.c
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

void
temporarily_reset (GtkWidget * w, gpointer data)
{
  static double threshold;
  static double low;
  int do_reset = !!data;
  int current = settings.preview_channel;
  if (do_reset)
    {
      if (channels > 2)
	{
	  threshold = settings.colour_thresholds[current];
	  low = settings.colour_low[current];
	  settings.colour_thresholds[current] = 0.0;
	  settings.colour_low[current] = 1.0;
	}
      else
	{
	  threshold = settings.gray_thresholds[current];
	  low = settings.gray_low[current];
	  settings.gray_thresholds[current] = 0.0;
	  settings.gray_low[current] = 1.0;
	}
    }
  else
    {
      if (channels > 2)
	{
	  settings.colour_thresholds[current] = threshold;
	  settings.colour_low[current] = low;
	}
      else
	{
	  settings.gray_thresholds[current] = threshold;
	  settings.gray_low[current] = low;
	}
    }
  gimp_preview_invalidate ((GimpPreview *) preview);
}

void
reset_channel (GtkWidget * w, gpointer data)
{
  if (channels > 2)
    {
      settings.colour_thresholds[settings.preview_channel] = 0.0;
      settings.colour_low[settings.preview_channel] = 0.0;
    }
  else
    {
      settings.gray_thresholds[settings.preview_channel] = 0.0;
      settings.gray_low[settings.preview_channel] = 0.0;
    }
  gtk_adjustment_set_value (GTK_ADJUSTMENT (thr_adj[0]), 0);
  gtk_adjustment_set_value (GTK_ADJUSTMENT (thr_adj[1]), 0.0);
}

void
reset_all (GtkWidget * w, gpointer data)
{
  if (channels > 2)
    {
      settings.colour_thresholds[0] = 0.0;
      settings.colour_thresholds[1] = 0.0;
      settings.colour_thresholds[2] = 0.0;
      settings.colour_low[0] = 0.0;
      settings.colour_low[1] = 0.0;
      settings.colour_low[2] = 0.0;
      if (channels == 4)
	{
	  settings.colour_thresholds[3] = 0.0;
	  settings.colour_low[3] = 0.0;
	}
    }
  else
    {
      settings.gray_thresholds[0] = 0;
      settings.gray_low[0] = 0.0;
      if (channels == 2)
	{
	  settings.gray_thresholds[1] = 0;
	  settings.gray_low[1] = 0.0;
	}
    }
  gtk_adjustment_set_value (GTK_ADJUSTMENT (thr_adj[0]), 0);
  gtk_adjustment_set_value (GTK_ADJUSTMENT (thr_adj[1]), 0.0);
}

void
set_rgb_mode (GtkWidget * w, gpointer data)
{
  /* change labels in the dialog */
  names = names_rgb;
  gtk_button_set_label (GTK_BUTTON (channel_radio[0]), names_rgb[0]);
  gtk_button_set_label (GTK_BUTTON (channel_radio[1]), names_rgb[1]);
  gtk_button_set_label (GTK_BUTTON (channel_radio[2]), names_rgb[2]);
  settings.colour_mode = MODE_RGB;
}

void
set_lab_mode (GtkWidget * w, gpointer data)
{
  /* change labels in the dialog */
  names = names_lab;
  gtk_button_set_label (GTK_BUTTON (channel_radio[0]), names_lab[0]);
  gtk_button_set_label (GTK_BUTTON (channel_radio[1]), names_lab[1]);
  gtk_button_set_label (GTK_BUTTON (channel_radio[2]), names_lab[2]);
  settings.colour_mode = MODE_LAB;
}

void
set_ycbcr_mode (GtkWidget * w, gpointer data)
{
  /* change labels in the dialog */
  names = names_ycbcr;
  gtk_button_set_label (GTK_BUTTON (channel_radio[0]), names_ycbcr[0]);
  gtk_button_set_label (GTK_BUTTON (channel_radio[1]), names_ycbcr[1]);
  gtk_button_set_label (GTK_BUTTON (channel_radio[2]), names_ycbcr[2]);
  settings.colour_mode = MODE_YCBCR;
}

void
set_preview_mode (GtkWidget * w, gpointer data)
{
  if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w)))
    return;
  settings.preview_mode = (gint) data;
}

void
set_preview_channel (GtkWidget * w, gpointer data)
{
  gint c = (gint) data;
  if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w)))
    return;
  settings.preview_channel = c;
  if (channels > 2)
    {
      gtk_adjustment_set_value (GTK_ADJUSTMENT (thr_adj[0]),
				settings.colour_thresholds[c]);
      gtk_adjustment_set_value (GTK_ADJUSTMENT (thr_adj[1]),
				settings.colour_low[c]);
    }
  else
    {
      gtk_adjustment_set_value (GTK_ADJUSTMENT (thr_adj[0]),
				settings.gray_thresholds[c]);
      gtk_adjustment_set_value (GTK_ADJUSTMENT (thr_adj[1]),
				settings.gray_low[c]);
    }
}

void
set_threshold (GtkWidget * w, gpointer data)
{
  double val;
  val = gtk_adjustment_get_value (GTK_ADJUSTMENT (w));
  if (channels > 2)
    settings.colour_thresholds[settings.preview_channel] = val;
  else
    settings.gray_thresholds[settings.preview_channel] = val;
}

void
set_low (GtkWidget * w, gpointer data)
{
  double val;
  val = gtk_adjustment_get_value (GTK_ADJUSTMENT (w));
  if (channels > 2)
    settings.colour_low[settings.preview_channel] = val;
  else
    settings.gray_low[settings.preview_channel] = val;
}
