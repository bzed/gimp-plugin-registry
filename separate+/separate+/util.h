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

#ifndef SEPARATE_UTIL_H
#define SEPARATE_UTIL_H

enum separate_channel {sep_C,sep_M,sep_Y,sep_K};

gboolean separate_is_CMYK (gint32 image_id);

char *separate_filename_change_extension (char  *root,
                                          char  *newext);
char *separate_filename_add_suffix       (char  *root,
                                          char  *suffix);
gint separate_path_get_extention_offset  (gchar *filename);

gint32 separate_create_RGB            (gchar    *filename,
                                       guint     width,
                                       guint     height,
                                       gboolean  has_alpha,
                                       gint32   *layers);
gint32 separate_create_planes_grey    (gchar    *filename,
                                       guint     width,
                                       guint     height,
                                       gint32   *layers);
gint32 separate_create_planes_CMYK    (gchar    *filename,
                                       guint     width,
                                       guint     height,
                                       gint32   *layers,
                                       guchar   *primaries);
gint32 separate_create_planes_Duotone (gchar    *filename,
                                       guint     width,
                                       guint     height,
                                       gint32   *layers);

void separate_init_settings  (SeparateContext        *sc,
                              enum separate_function  func,
                              gboolean                get_last_values);
void separate_store_settings (SeparateContext        *sc,
                              enum separate_function  func);

GimpDrawable *separate_find_channel (gint32                image_id,
                                     enum separate_channel channel);
GimpDrawable *separate_find_alpha   (gint32                image_id);

#endif
