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

#ifndef SEPARATE_H
#define SEPARATE_H

#include <gtk/gtk.h>
#include "lcms_wrapper.h"

#define CMYKPROFILE "plug_in_separate_save/cmyk-profile"

#ifdef ENABLE_COLOR_MANAGEMENT
typedef GimpColorRenderingIntent SeparateRenderingIntent;
#else
typedef gint SeparateRenderingIntent;
#endif

enum separate_function { SEP_NONE, SEP_DUOTONE, SEP_SEPARATE, SEP_FULL, SEP_LIGHT, SEP_PROOF, SEP_SAVE, SEP_EXPORT, SEP_LOAD };

typedef struct _SeparateSettings
{
  gboolean preserveblack;
  gboolean overprintblack;
  gboolean profile;
  SeparateRenderingIntent intent;
  gboolean bpc;
  gboolean dither;
  gboolean composite;
} SeparateSettings;

typedef struct _ProofSettings
{
  SeparateRenderingIntent mode;
  gboolean profile;
} ProofSettings;

typedef struct _SaveSettings
{
  gint32 embedprofile;
  gint32 filetype;
  gint32 clipping_path_id;
  gboolean compression;
} SaveSettings;

typedef struct _SeparateContext
{
  /* Settings */
  gchar *displayfilename;
  gchar *rgbfilename;
  gchar *cmykfilename;
  gchar *prooffilename;
  gchar *alt_displayfilename;
  gchar *alt_rgbfilename;
  gchar *alt_cmykfilename;
  gchar *alt_prooffilename;
  gchar *filename;
  SeparateSettings ss;
  ProofSettings ps;
  SaveSettings sas;

  /* Dialog private */
  GtkWidget *dialog;
  GtkWidget *srclabel;
  GtkWidget *rgbfileselector;
  GtkWidget *cmykfileselector;
  GtkWidget *filenamefileselector;
  GtkWidget *profileselector;
  GtkWidget *profilelabel;
  GtkWidget *intentselector;
  GtkWidget *intentlabel;
  GtkWidget *bpcselector;
  gboolean dialogresult;
  gboolean integrated;
  gboolean has_embedded_profile;

  /* Core related */
  gint32 imageID;
  GimpDrawable *drawable;
  gboolean drawable_has_alpha;
  cmsHTRANSFORM hTransform;
  guchar *cmyktemp;
  guchar *destptr[5];
  int bpp[5];
} SeparateContext;

#endif
