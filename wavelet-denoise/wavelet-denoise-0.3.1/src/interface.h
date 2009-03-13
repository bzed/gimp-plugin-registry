/* 
 * Wavelet denoise GIMP plugin
 * 
 * interface.h
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

/* colour mode frame */
GtkWidget *fr_mode, *mode_radio[3], *mode_vbox;
GSList *mode_list;

/* preview select frame */
GtkWidget *fr_preview, *preview_radio[3], *preview_vbox, *preview_check;
GSList *preview_list;

/* channel select frame */
GtkWidget *fr_channel, *channel_radio[4], *channel_vbox;
GSList *channel_list;

/* threshold frame */
GtkWidget *fr_threshold, *thr_label[2], *thr_spin[2];
GtkWidget *thr_hbox[2], *thr_vbox, *thr_scale[2];
GtkObject *thr_adj[2];

/* reset buttons */
GtkWidget *reset_button[2], *reset_hbox, *reset_align, *reset_button_icon[2];

/* dialog */
GtkWidget *dialog, *dialog_hbox, *dialog_vbox, *frame_hbox, *dialog_aspect;
GtkWidget *preview, *preview_reset, *preview_hbox, *preview_reset_icon;

extern GtkWidget **radios_labels[];

char **names;
