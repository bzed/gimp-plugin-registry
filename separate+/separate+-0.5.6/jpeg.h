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

#ifndef SEPARATE_JPEG_H
#define SEPARATE_JPEG_H

gboolean separate_jpeg_export (gchar         *filename,
                               gint32         imageID,
                               gconstpointer  profile_data,
                               gsize          profile_length,
                               gconstpointer  path_data,
                               gsize          path_length,
                               gboolean       compression);

#endif
