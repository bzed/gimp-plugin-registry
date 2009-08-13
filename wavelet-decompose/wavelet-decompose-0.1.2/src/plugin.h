/* 
 * Wavelet decompose GIMP plugin
 * 
 * plugin.h
 * Copyright 2008 by Marco Rossini
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2+
 * as published by the Free Software Foundation.
 */

#ifndef __PLUGIN_H__
#define __PLUGIN_H__

#include <stdlib.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>
#include <libintl.h>
#include <locale.h>

/* has to be at least one byte larger than pixel type */
typedef signed short wdsint;

#define LOCALEDIR "/usr/share/locale/"
#define GETTEXT_PKG "gimp20-wavelet-decompose-plug-in"

#define _(string) gettext(string)
#define N_(string) string

#define MAX2(x,y) ((x) > (y) ? (x) : (y))
#define MIN2(x,y) ((x) < (y) ? (x) : (y))

void query (void);
void run (const gchar * name, gint nparams, const GimpParam * param,
	  gint * nreturn_vals, GimpParam ** return_vals);
void wavelet_decompose (guint32 image, guint32 layer, wdsint * img[3],
			unsigned int width, unsigned int height,
			unsigned int channels, unsigned int scales);
void decompose (gint32 image, GimpDrawable * drawable, unsigned int scales);
gboolean user_interface (GimpDrawable * drawable, unsigned int maxscale);
void add_layer (gint32 image, gint32 parent, wdsint ** img, const char *name,
		GimpLayerModeEffects mode);

extern GimpPlugInInfo PLUG_IN_INFO;

typedef struct
{
  unsigned int scales;
  unsigned int new_image;
  unsigned int add_alpha;
  GimpLayerModeEffects layer_modes;
} wavelet_settings;

extern wavelet_settings settings;

#endif /* __PLUGIN_H__ */
