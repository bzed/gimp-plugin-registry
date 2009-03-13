/* 
 * Wavelet denoise GIMP plugin
 * 
 * plugin.h
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

#ifndef __PLUGIN_H__
#define __PLUGIN_H__

#include <stdlib.h>
#include <math.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>
#include <libintl.h>

#define LOCALEDIR "/usr/share/locale"

#define _(str) gettext(str)
#define gettext_noop(str) str
#define N_(str) gettext_noop(str)

#include "messages.h"

#define MAX2(x,y) ((x) > (y) ? (x) : (y))
#define MIN2(x,y) ((x) < (y) ? (x) : (y))
#define CLIP(x,min,max) MAX2((min), MIN2((x), (max)))

#define TIMER_READ (1.0 / 10)
#define TIMER_WRITE (1.0 / 7)
#define TIMER_PROCESS (1.0 - TIMER_READ - TIMER_WRITE)

#define MODE_YCBCR 0
#define MODE_RGB 1
#define MODE_LAB 2

void query (void);
void run (const gchar * name, gint nparams, const GimpParam * param,
		 gint * nreturn_vals, GimpParam ** return_vals);
void wavelet_denoise (float *fimg[3], unsigned int width,
			     unsigned int height, float threshold, double low,
			     float a, float b);
void denoise (GimpDrawable * drawable, GimpPreview * preview);
void set_rgb_mode (GtkWidget * w, gpointer data);
void set_lab_mode (GtkWidget * w, gpointer data);
void set_ycbcr_mode (GtkWidget * w, gpointer data);
void set_preview_mode (GtkWidget * w, gpointer data);
void set_preview_channel (GtkWidget * w, gpointer data);
void set_threshold (GtkWidget * w, gpointer data);
void set_low (GtkWidget * w, gpointer data);
gboolean user_interface (GimpDrawable * drawable);
void reset_channel (GtkWidget * w, gpointer data);
void reset_all (GtkWidget * w, gpointer data);
void temporarily_reset (GtkWidget * w, gpointer data);

void srgb2rgb (float **fimg, int size);
void rgb2srgb (float **fimg, int size, int pc);
void srgb2ycbcr (float **fimg, int size);
void ycbcr2srgb (float **fimg, int size, int pc);
void srgb2lab (float **fimg, int size);
void lab2srgb (float **fimg, int size, int pc);
void srgb2xyz (float **fimg, int size);
void xyz2srgb (float **fimg, int size, int pc);

extern GimpPlugInInfo PLUG_IN_INFO;

typedef struct
{
  double gray_thresholds[2];
  double gray_low[2];
  double colour_thresholds[4];
  double colour_low[4];
  guint colour_mode;
  gint preview_channel;
  gboolean preview_mode;
  gboolean preview;
  float times[3];
  gint winxsize, winysize;
} wavelet_settings;

extern wavelet_settings settings;

extern char *names_ycbcr[];
extern char *names_rgb[];
extern char *names_gray[];
extern char *names_lab[];

float *fimg[4];
float *buffer[3];
gint channels;

GTimer *timer;

#endif /* __PLUGIN_H__ */
