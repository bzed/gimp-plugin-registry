/* GIMP LiquidRescaling Plug-in
 * Copyright (C) 2007 Carlo Baldassi (the "Author") <carlobaldassi@yahoo.it>.
 * (implementation based on the GIMP Plug-in Template by Michael Natterer)
 * All Rights Reserved.
 *
 * This plugin implements the algorithm described in the paper
 * "Seam Carving for Content-Aware Image Resizing"
 * by Shai Avidan and Ariel Shamir
 * which can be found at http://www.faculty.idc.ac.il/arik/imret.pdf
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 dated June, 1991.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef __MAIN_H__
#define __MAIN_H__

typedef struct
{
  gint new_width;
  gint new_height;
  gint32 pres_layer_ID;
  gint pres_coeff;
  gint32 disc_layer_ID;
  gint disc_coeff;
  gint grad_func;
  gboolean update_en;
  gboolean resize_canvas;
  gint mask_behavior;
} PlugInVals;

typedef struct
{
  gint32 image_id;
} PlugInImageVals;

typedef struct
{
  gint32 layer_ID;
  gboolean pres_status;
  gboolean disc_status;
} PlugInDrawableVals;

typedef struct
{
  gboolean chain_active;
} PlugInUIVals;


/*  Default values  */

extern const PlugInVals default_vals;
extern const PlugInImageVals default_image_vals;
extern const PlugInDrawableVals default_drawable_vals;
extern const PlugInUIVals default_ui_vals;


#endif /* __MAIN_H__ */
