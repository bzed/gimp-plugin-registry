/* Focus Blur -- blur with focus plug-in.
 * Copyright (C) 2002-2007 Kyoichiro Suda
 *
 * The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
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

#ifndef __FOCUSBLUR_ENUMS_H__
#define __FOCUSBLUR_ENUMS_H__

G_BEGIN_DECLS


typedef enum
{
  FBLUR_MODEL_FLAT,
  FBLUR_MODEL_RING,
  FBLUR_MODEL_CONCAVE,
} FblurModelType;


typedef enum
{
  FBLUR_SHINE_NONE,             /* not used */
  FBLUR_SHINE_LUMINOSITY,
  FBLUR_SHINE_SATURATION,
} FblurShineType;


typedef enum
{
  FBLUR_QUALITY_BEST = 0,       /* do not use FFT */
  FBLUR_QUALITY_NORMAL,         /* reduce depth division,
                                   but interpolated */
  FBLUR_QUALITY_LOW,            /* reduce depth division */
  FBLUR_QUALITY_DEFECTIVE       /* use coarsely depth division,
                                   depth fakes is not appeared */
} FblurQualityType;


typedef enum
{
  FBLUR_WIDGET_FOCAL_DEPTH,
  FBLUR_WIDGET_ENABLE_DEPTH_MAP,
  FBLUR_WIDGET_ENABLE_SHINE,
  FBLUR_WIDGET_MAX
} FblurWidgets;


G_END_DECLS

#endif /* __FOCUSBLUR_ENUMS_H__ */
