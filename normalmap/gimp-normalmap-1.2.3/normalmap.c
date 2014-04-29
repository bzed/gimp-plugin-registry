/*
   normalmap GIMP plugin

   Copyright (C) 2002-2008 Shawn Kirst <skirst@insightbb.com>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <gtk/gtk.h>

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "scale.h"
#include "preview3d.h"

#define PREVIEW_SIZE 150

enum FILTER_TYPE
{
   FILTER_NONE = 0, FILTER_SOBEL_3x3, FILTER_SOBEL_5x5, FILTER_PREWITT_3x3,
   FILTER_PREWITT_5x5, FILTER_3x3, FILTER_5x5, FILTER_7x7, FILTER_9x9,
   MAX_FILTER_TYPE
};

enum ALPHA_TYPE
{
   ALPHA_NONE = 0, ALPHA_HEIGHT, ALPHA_INVERSE_HEIGHT, ALPHA_ZERO, ALPHA_ONE,
   ALPHA_INVERT, ALPHA_MAP, MAX_ALPHA_TYPE
};

enum CONVERSION_TYPE
{
   CONVERT_NONE = 0, CONVERT_BIASED_RGB, CONVERT_RED, CONVERT_GREEN, 
   CONVERT_BLUE, CONVERT_MAX_RGB, CONVERT_MIN_RGB, CONVERT_COLORSPACE,
   CONVERT_NORMALIZE_ONLY, CONVERT_DUDV_TO_NORMAL, CONVERT_HEIGHTMAP,
   MAX_CONVERSION_TYPE
};

enum DUDV_TYPE
{
   DUDV_NONE, DUDV_8BIT_SIGNED, DUDV_8BIT_UNSIGNED, DUDV_16BIT_SIGNED,
   DUDV_16BIT_UNSIGNED,
   MAX_DUDV_TYPE
};

typedef struct
{
   gint filter;
   gdouble minz;
   gdouble scale;
   gint wrap;
   gint height_source;
   gint alpha;
   gint conversion;
   gint dudv;
   gint xinvert;
   gint yinvert;
   gint swapRGB;
   gdouble contrast;
   gint32 alphamap_id;
} NormalmapVals;

static void query(void);
static void run(const gchar *name, gint params, const GimpParam *param,
                gint *nreturn_vals, GimpParam **return_vals);

static gint32 normalmap(GimpDrawable *drawable, gboolean preview_mode);

static gint normalmap_dialog(GimpDrawable *drawable);

GimpPlugInInfo PLUG_IN_INFO =
{
   0, 0, query, run
};

NormalmapVals nmapvals =
{
   .filter = 0,
   .minz = 0.0,
   .scale = 1.0,
   .wrap = 0,
   .height_source = 0,
   .alpha = ALPHA_NONE,
   .conversion = CONVERT_NONE,
   .dudv = DUDV_NONE,
   .xinvert = 0,
   .yinvert = 0,
   .swapRGB = 0,
   .contrast = 0.0,
   .alphamap_id = 0
};

static const float oneover255 = 1.0f / 255.0f;

gint runme = 0;

static GtkWidget *dialog;
static GtkWidget *preview;

MAIN()
   
static void query(void)
{
   static GimpParamDef args[]=
   {
      {GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive"},
      {GIMP_PDB_IMAGE, "image", "Input image (unused)"},
      {GIMP_PDB_DRAWABLE, "drawable", "Input drawable"},
      {GIMP_PDB_INT32, "filter", "Filter type (0 = 4 sample, 1 = sobel 3x3, 2 = sobel 5x5, 3 = prewitt 3x3, 4 = prewitt 5x5, 5-8 = 3x3,5x5,7x7,9x9)"},
      {GIMP_PDB_FLOAT, "minz", "Minimun Z (0 to 1)"},
      {GIMP_PDB_FLOAT, "scale", "Scale (>0)"},
      {GIMP_PDB_INT32, "wrap", "Wrap (0 = no)"},
      {GIMP_PDB_INT32, "height_source", "Height source (0 = average RGB, 1 = alpha channel)"},
      {GIMP_PDB_INT32, "alpha", "Alpha (0 = unchanged, 1 = set to height, 2 = set to inverse height, 3 = set to 0, 4 = set to 1, 5 = invert, 6 = set to alpha map value)"},
      {GIMP_PDB_INT32, "conversion", "Conversion (0 = normalize only, 1 = Biased RGB, 2 = Red, 3 = Green, 4 = Blue, 5 = Max RGB, 6 = Min RGB, 7 = Colorspace, 8 = Normalize only, 9 = Convert to height map)"},
      {GIMP_PDB_INT32, "dudv", "DU/DV map (0 = none, 1 = 8-bit, 2 = 8-bit unsigned, 3 = 16-bit, 4 = 16-bit unsigned)"},
      {GIMP_PDB_INT32, "xinvert", "Invert X component of normal"},
      {GIMP_PDB_INT32, "yinvert", "Invert Y component of normal"},
      {GIMP_PDB_INT32, "swapRGB", "Swap RGB components"},
      {GIMP_PDB_FLOAT, "contrast", "Height contrast (0 to 1). If converting to a height map, this value is applied to the results"},
      {GIMP_PDB_DRAWABLE, "alphamap", "Alpha map drawable"}
   };
   static gint nargs = sizeof(args) / sizeof(args[0]);
   
   gimp_install_procedure("plug_in_normalmap",
                          "Converts image to an RGB normalmap",
                          "foo!",
                          "Shawn Kirst",
                          "Shawn Kirst",
                          "February 2002",
                          "<Image>/Filters/Map/Normalmap...",
                          "RGB*",
                          GIMP_PLUGIN,
                          nargs, 0,
                          args, NULL);
}

static void run(const gchar *name, gint nparams, const GimpParam *param,
                gint *nreturn_vals, GimpParam **return_vals)
{
   static GimpParam values[1];
   GimpDrawable *drawable;
   GimpRunMode run_mode;
   GimpPDBStatusType status = GIMP_PDB_SUCCESS;
   
   run_mode = param[0].data.d_int32;
   
   *nreturn_vals = 1;
   *return_vals = values;
   
   values[0].type = GIMP_PDB_STATUS;
   values[0].data.d_status = status;
   
   drawable = gimp_drawable_get(param[2].data.d_drawable);
   
   switch(run_mode)
   {
      case GIMP_RUN_INTERACTIVE:
         gimp_ui_init("normalmap", 0);
         gimp_get_data("plug_in_normalmap", &nmapvals);
         if(!normalmap_dialog(drawable))
         {
            gimp_drawable_detach(drawable);
            return;
         }
         break;
      case GIMP_RUN_NONINTERACTIVE:
         if(nparams != 16)
            status=GIMP_PDB_CALLING_ERROR;
         else
         {
            nmapvals.filter = param[3].data.d_int32;
            nmapvals.minz = param[4].data.d_float;
            nmapvals.scale = param[5].data.d_float;
            nmapvals.wrap = param[6].data.d_int32;
            nmapvals.height_source = param[7].data.d_int32;
            nmapvals.alpha = param[8].data.d_int32;
            nmapvals.conversion = param[9].data.d_int32;
            nmapvals.dudv = param[10].data.d_int32;
            nmapvals.xinvert = param[11].data.d_int32;
            nmapvals.yinvert = param[12].data.d_int32;
            nmapvals.swapRGB = param[13].data.d_int32;
            nmapvals.contrast = param[14].data.d_float;
            nmapvals.alphamap_id = gimp_drawable_get(param[15].data.d_drawable)->drawable_id;
         }
         break;
      case GIMP_RUN_WITH_LAST_VALS:
         gimp_get_data("plug_in_normalmap", &nmapvals);
         break;
      default:
         break;
   }
   
   gimp_progress_init("Creating normalmap...");
   
   if(normalmap(drawable,FALSE) == -1)
      status = GIMP_PDB_EXECUTION_ERROR;
   
   if(run_mode != GIMP_RUN_NONINTERACTIVE)
      gimp_displays_flush();
   
   if(run_mode == GIMP_RUN_INTERACTIVE)
      gimp_set_data("plug_in_normalmap", &nmapvals, sizeof(nmapvals));
   
   values[0].data.d_status = status;
   
   gimp_drawable_detach(drawable);
}

#ifndef min
#define min(a,b)  ((a)<(b) ? (a) : (b))
#endif
#ifndef max
#define max(a,b)  ((a)>(b) ? (a) : (b))
#endif

#define SQR(x)      ((x) * (x))
#define LERP(a,b,c) ((a) + ((b) - (a)) * (c))

static inline void NORMALIZE(float *v)
{
   float len = sqrtf(SQR(v[0]) + SQR(v[1]) + SQR(v[2]));
   
   if(len > 1e-04f)
   {
      len = 1.0f / len;
      v[0] *= len;
      v[1] *= len;
      v[2] *= len;
   }
   else
      v[0] = v[1] = v[2] = 0;
}

typedef struct
{
   int x,y;
   float w;
} kernel_element;

static void make_kernel(kernel_element *k, float *weights, int size)
{
   int x, y, idx;
   
   for(y = 0; y < size; ++y)
   {
      for(x = 0; x < size; ++x)
      {
         idx = x + y * size;
         k[idx].x = x - (size / 2);
         k[idx].y = (size / 2) - y;
         k[idx].w = weights[idx];
      }
   }
}

static void rotate_array(float *dst, float *src, int size)
{
   int x, y, newx, newy;
   
   for(y = 0; y < size; ++y)
   {
      for(x = 0; x < size; ++x)
      {
         newy = size - x - 1;
         newx = y;
         dst[newx + newy * size] = src[x + y * size];
      }
   }
}

static int sample_alpha_map(unsigned char *pixels, int x, int y,
                            int w, int h, int sw, int sh)
{
   int ix, iy, wx, wy, v;
   int a, b, c, d;
   unsigned char *s;
   
   if(sh > 1)
   {
      iy = (((h - 1) * y) << 7) / (sh - 1);
      if(y == sh - 1) --iy;
      wy = iy & 0x7f;
      iy >>= 7;
   }
   else
      iy = wy = 0;
   
   if(sw > 1)
   {
      ix = (((w - 1) * x) << 7) / (sw - 1);
      if(x == sw - 1) --ix;
      wx = ix & 0x7f;
      ix >>= 7;
   }
   else
      ix = wx = 0;
   
   s = pixels + ((iy - 1) * w + (ix - 1));
   
   b = icerp(s[w + 0],
             s[w + 1],
             s[w + 2],
             s[w + 3], wx);
   if(iy > 0)
   {
      a = icerp(s[0],
                s[1],
                s[2],
                s[3], wx);
   }
   else
      a = b;
   
   c = icerp(s[2 * w + 0],
             s[2 * w + 1],
             s[2 * w + 2],
             s[2 * w + 3], wx);
   if(iy < sh - 1)
   {
      d = icerp(s[3 * w + 0],
                s[3 * w + 1],
                s[3 * w + 2],
                s[3 * w + 3], wx);
   }
   else
      d = c;
   
   v = icerp(a, b, c, d, wy);
            
   if(v <   0) v = 0;
   if(v > 255) v = 255;
   
   return((unsigned char)v);
}

static void make_heightmap(unsigned char *image, int w, int h, int bpp)
{
   unsigned int i, num_pixels = w * h;
   int x, y;
   float v, hmin, hmax;
   float *s, *r;
   
   s = (float*)g_malloc(w * h * 3 * sizeof(float));
   if(s == 0)
   {
      g_message("Memory allocation error!");
      return;
   }
   r = (float*)g_malloc(w * h * 4 * sizeof(float));
   if(r == 0)
   {
      g_free(s);
      g_message("Memory allocation error!");
      return;
   }
   
   /* scale into 0 to 1 range, make signed -1 to 1 */
   for(i = 0; i < num_pixels; ++i)
   {
      s[3 * i + 0] = (((float)image[bpp * i + 0] / 255.0f) - 0.5) * 2.0f;
      s[3 * i + 1] = (((float)image[bpp * i + 1] / 255.0f) - 0.5) * 2.0f;
      s[3 * i + 2] = (((float)image[bpp * i + 2] / 255.0f) - 0.5) * 2.0f;
   }
   
   memset(r, 0, w * h * 4 * sizeof(float));

#define S(x, y, n) s[(y) * (w * 3) + ((x) * 3) + (n)]
#define R(x, y, n) r[(y) * (w * 4) + ((x) * 4) + (n)]
   
   /* top-left to bottom-right */
   for(x = 1; x < w; ++x)
      R(x, 0, 0) = R(x - 1, 0, 0) + S(x - 1, 0, 0);
   for(y = 1; y < h; ++y)
      R(0, y, 0) = R(0, y - 1, 0) + S(0, y - 1, 1);
   for(y = 1; y < h; ++y)
   {
      for(x = 1; x < w; ++x)
      {
         R(x, y, 0) = (R(x, y - 1, 0) + R(x - 1, y, 0) +
                       S(x - 1, y, 0) + S(x, y - 1, 1)) * 0.5f;
      }
   }

   /* top-right to bottom-left */
   for(x = w - 2; x >= 0; --x)
      R(x, 0, 1) = R(x + 1, 0, 1) - S(x + 1, 0, 0);
   for(y = 1; y < h; ++y)
      R(0, y, 1) = R(0, y - 1, 1) + S(0, y - 1, 1);
   for(y = 1; y < h; ++y)
   {
      for(x = w - 2; x >= 0; --x)
      {
         R(x, y, 1) = (R(x, y - 1, 1) + R(x + 1, y, 1) -
                       S(x + 1, y, 0) + S(x, y - 1, 1)) * 0.5f;
      }
   }

   /* bottom-left to top-right */
   for(x = 1; x < w; ++x)
      R(x, 0, 2) = R(x - 1, 0, 2) + S(x - 1, 0, 0);
   for(y = h - 2; y >= 0; --y)
      R(0, y, 2) = R(0, y + 1, 2) - S(0, y + 1, 1);
   for(y = h - 2; y >= 0; --y)
   {
      for(x = 1; x < w; ++x)
      {
         R(x, y, 2) = (R(x, y + 1, 2) + R(x - 1, y, 2) +
                       S(x - 1, y, 0) - S(x, y + 1, 1)) * 0.5f;
      }
   }

   /* bottom-right to top-left */
   for(x = w - 2; x >= 0; --x)
      R(x, 0, 3) = R(x + 1, 0, 3) - S(x + 1, 0, 0);
   for(y = h - 2; y >= 0; --y)
      R(0, y, 3) = R(0, y + 1, 3) - S(0, y + 1, 1);
   for(y = h - 2; y >= 0; --y)
   {
      for(x = w - 2; x >= 0; --x)
      {
         R(x, y, 3) = (R(x, y + 1, 3) + R(x + 1, y, 3) -
                       S(x + 1, y, 0) - S(x, y + 1, 1)) * 0.5f;
      }
   }

#undef S
#undef R

   /* accumulate, find min/max */
   hmin =  1e10f;
   hmax = -1e10f;
   for(i = 0; i < num_pixels; ++i)
   {
      r[4 * i] += r[4 * i + 1] + r[4 * i + 2] + r[4 * i + 3];
      if(r[4 * i] < hmin) hmin = r[4 * i];
      if(r[4 * i] > hmax) hmax = r[4 * i];
   }

   /* scale into 0 - 1 range */
   for(i = 0; i < num_pixels; ++i)
   {   
      v = (r[4 * i] - hmin) / (hmax - hmin);
      /* adjust contrast */
      v = (v - 0.5f) * nmapvals.contrast + v;
      if(v < 0) v = 0;
      if(v > 1) v = 1;
      r[4 * i] = v;
   }

   /* write out results */
   for(i = 0; i < num_pixels; ++i)
   {
      v = r[4 * i] * 255.0f;
      image[bpp * i + 0] = (unsigned char)v;
      image[bpp * i + 1] = (unsigned char)v;
      image[bpp * i + 2] = (unsigned char)v;
   }

   g_free(s);
   g_free(r);
}

static gint32 normalmap(GimpDrawable *drawable, gboolean preview_mode)
{
   gint x, y;
   gint width, height, bpp, rowbytes, pw, ph, amap_w = 0, amap_h = 0;
   guchar *d, *dst, *s, *src, *tmp, *amap = 0;
   float *heights;
   float val, du, dv, n[3], weight;
   float rgb_bias[3];
   int i, num_elements = 0;
   kernel_element *kernel_du = 0;
   kernel_element *kernel_dv = 0;
   GimpPixelRgn src_rgn, dst_rgn, amap_rgn;
   GdkCursor *cursor = 0;
   
   if(nmapvals.filter < 0 || nmapvals.filter >= MAX_FILTER_TYPE)
      nmapvals.filter = FILTER_NONE;
   if(drawable->bpp != 4) nmapvals.height_source = 0;
   if(drawable->bpp != 4 && (nmapvals.dudv == DUDV_16BIT_SIGNED ||
                             nmapvals.dudv == DUDV_16BIT_UNSIGNED))
      nmapvals.dudv = DUDV_NONE;
   
   width = drawable->width;
   height = drawable->height;
   bpp = drawable->bpp;
   rowbytes = width * bpp;

   dst = g_malloc(width * height * bpp);
   if(dst == 0)
   {
      g_message("Memory allocation error!");
      return(-1);
   }

   src = g_malloc(width * height * bpp);
   if(src == 0)
   {
      g_message("Memory allocation error!");
      return(-1);
   }
   
   heights = g_new(float, width * height);
   if(heights == 0)
   {
      g_message("Memory allocation error!");
      return(-1);
   }

   if(!nmapvals.dudv && drawable->bpp == 4 && nmapvals.alpha == ALPHA_MAP &&
      nmapvals.alphamap_id != 0)
   {
      GimpDrawable *alphamap = gimp_drawable_get(nmapvals.alphamap_id);
      
      amap_w = alphamap->width;
      amap_h = alphamap->height;
      
      amap = g_malloc(amap_w * amap_h);
      
      gimp_pixel_rgn_init(&amap_rgn, alphamap, 0, 0, amap_w, amap_h, 0, 0);
      gimp_pixel_rgn_get_rect(&amap_rgn, amap, 0, 0, amap_w, amap_h);
   }

   gimp_pixel_rgn_init(&src_rgn, drawable, 0, 0, width, height, 0, 0);
   gimp_pixel_rgn_get_rect(&src_rgn, src, 0, 0, width, height);
   
   switch(nmapvals.filter)
   {
      case FILTER_NONE:
         num_elements = 2;
         kernel_du = (kernel_element*)g_malloc(2 * sizeof(kernel_element));
         kernel_dv = (kernel_element*)g_malloc(2 * sizeof(kernel_element));
         
         kernel_du[0].x = -1; kernel_du[0].y = 0; kernel_du[0].w = -0.5f;
         kernel_du[1].x =  1; kernel_du[1].y = 0; kernel_du[1].w =  0.5f;
         
         kernel_dv[0].x = 0; kernel_dv[0].y =  1; kernel_dv[0].w =  0.5f;
         kernel_dv[1].x = 0; kernel_dv[1].y = -1; kernel_dv[1].w = -0.5f;
         
         break;
      case FILTER_SOBEL_3x3:
         num_elements = 6;
         kernel_du = (kernel_element*)g_malloc(6 * sizeof(kernel_element));
         kernel_dv = (kernel_element*)g_malloc(6 * sizeof(kernel_element));
      
         kernel_du[0].x = -1; kernel_du[0].y =  1; kernel_du[0].w = -1.0f;
         kernel_du[1].x = -1; kernel_du[1].y =  0; kernel_du[1].w = -2.0f;
         kernel_du[2].x = -1; kernel_du[2].y = -1; kernel_du[2].w = -1.0f;
         kernel_du[3].x =  1; kernel_du[3].y =  1; kernel_du[3].w =  1.0f;
         kernel_du[4].x =  1; kernel_du[4].y =  0; kernel_du[4].w =  2.0f;
         kernel_du[5].x =  1; kernel_du[5].y = -1; kernel_du[5].w =  1.0f;
         
         kernel_dv[0].x = -1; kernel_dv[0].y =  1; kernel_dv[0].w =  1.0f;
         kernel_dv[1].x =  0; kernel_dv[1].y =  1; kernel_dv[1].w =  2.0f;
         kernel_dv[2].x =  1; kernel_dv[2].y =  1; kernel_dv[2].w =  1.0f;
         kernel_dv[3].x = -1; kernel_dv[3].y = -1; kernel_dv[3].w = -1.0f;
         kernel_dv[4].x =  0; kernel_dv[4].y = -1; kernel_dv[4].w = -2.0f;
         kernel_dv[5].x =  1; kernel_dv[5].y = -1; kernel_dv[5].w = -1.0f;
         
         break;
      case FILTER_SOBEL_5x5:
         num_elements = 20;
         kernel_du = (kernel_element*)g_malloc(20 * sizeof(kernel_element));
         kernel_dv = (kernel_element*)g_malloc(20 * sizeof(kernel_element));

         kernel_du[ 0].x = -2; kernel_du[ 0].y =  2; kernel_du[ 0].w =  -1.0f;
         kernel_du[ 1].x = -2; kernel_du[ 1].y =  1; kernel_du[ 1].w =  -4.0f;
         kernel_du[ 2].x = -2; kernel_du[ 2].y =  0; kernel_du[ 2].w =  -6.0f;
         kernel_du[ 3].x = -2; kernel_du[ 3].y = -1; kernel_du[ 3].w =  -4.0f;
         kernel_du[ 4].x = -2; kernel_du[ 4].y = -2; kernel_du[ 4].w =  -1.0f;
         kernel_du[ 5].x = -1; kernel_du[ 5].y =  2; kernel_du[ 5].w =  -2.0f;
         kernel_du[ 6].x = -1; kernel_du[ 6].y =  1; kernel_du[ 6].w =  -8.0f;
         kernel_du[ 7].x = -1; kernel_du[ 7].y =  0; kernel_du[ 7].w = -12.0f;
         kernel_du[ 8].x = -1; kernel_du[ 8].y = -1; kernel_du[ 8].w =  -8.0f;
         kernel_du[ 9].x = -1; kernel_du[ 9].y = -2; kernel_du[ 9].w =  -2.0f;
         kernel_du[10].x =  1; kernel_du[10].y =  2; kernel_du[10].w =   2.0f;
         kernel_du[11].x =  1; kernel_du[11].y =  1; kernel_du[11].w =   8.0f;
         kernel_du[12].x =  1; kernel_du[12].y =  0; kernel_du[12].w =  12.0f;
         kernel_du[13].x =  1; kernel_du[13].y = -1; kernel_du[13].w =   8.0f;
         kernel_du[14].x =  1; kernel_du[14].y = -2; kernel_du[14].w =   2.0f;
         kernel_du[15].x =  2; kernel_du[15].y =  2; kernel_du[15].w =   1.0f;
         kernel_du[16].x =  2; kernel_du[16].y =  1; kernel_du[16].w =   4.0f;
         kernel_du[17].x =  2; kernel_du[17].y =  0; kernel_du[17].w =   6.0f;
         kernel_du[18].x =  2; kernel_du[18].y = -1; kernel_du[18].w =   4.0f;
         kernel_du[19].x =  2; kernel_du[19].y = -2; kernel_du[19].w =   1.0f;
      
         kernel_dv[ 0].x = -2; kernel_dv[ 0].y =  2; kernel_dv[ 0].w =   1.0f;
         kernel_dv[ 1].x = -1; kernel_dv[ 1].y =  2; kernel_dv[ 1].w =   4.0f;
         kernel_dv[ 2].x =  0; kernel_dv[ 2].y =  2; kernel_dv[ 2].w =   6.0f;
         kernel_dv[ 3].x =  1; kernel_dv[ 3].y =  2; kernel_dv[ 3].w =   4.0f;
         kernel_dv[ 4].x =  2; kernel_dv[ 4].y =  2; kernel_dv[ 4].w =   1.0f;
         kernel_dv[ 5].x = -2; kernel_dv[ 5].y =  1; kernel_dv[ 5].w =   2.0f;
         kernel_dv[ 6].x = -1; kernel_dv[ 6].y =  1; kernel_dv[ 6].w =   8.0f;
         kernel_dv[ 7].x =  0; kernel_dv[ 7].y =  1; kernel_dv[ 7].w =  12.0f;
         kernel_dv[ 8].x =  1; kernel_dv[ 8].y =  1; kernel_dv[ 8].w =   8.0f;
         kernel_dv[ 9].x =  2; kernel_dv[ 9].y =  1; kernel_dv[ 9].w =   2.0f;
         kernel_dv[10].x = -2; kernel_dv[10].y = -1; kernel_dv[10].w =  -2.0f;
         kernel_dv[11].x = -1; kernel_dv[11].y = -1; kernel_dv[11].w =  -8.0f;
         kernel_dv[12].x =  0; kernel_dv[12].y = -1; kernel_dv[12].w = -12.0f;
         kernel_dv[13].x =  1; kernel_dv[13].y = -1; kernel_dv[13].w =  -8.0f;
         kernel_dv[14].x =  2; kernel_dv[14].y = -1; kernel_dv[14].w =  -2.0f;
         kernel_dv[15].x = -2; kernel_dv[15].y = -2; kernel_dv[15].w =  -1.0f;
         kernel_dv[16].x = -1; kernel_dv[16].y = -2; kernel_dv[16].w =  -4.0f;
         kernel_dv[17].x =  0; kernel_dv[17].y = -2; kernel_dv[17].w =  -6.0f;
         kernel_dv[18].x =  1; kernel_dv[18].y = -2; kernel_dv[18].w =  -4.0f;
         kernel_dv[19].x =  2; kernel_dv[19].y = -2; kernel_dv[19].w =  -1.0f;
      
         break;
      case FILTER_PREWITT_3x3:
         num_elements = 6;
         kernel_du = (kernel_element*)g_malloc(6 * sizeof(kernel_element));
         kernel_dv = (kernel_element*)g_malloc(6 * sizeof(kernel_element));
      
         kernel_du[0].x = -1; kernel_du[0].y =  1; kernel_du[0].w = -1.0f;
         kernel_du[1].x = -1; kernel_du[1].y =  0; kernel_du[1].w = -1.0f;
         kernel_du[2].x = -1; kernel_du[2].y = -1; kernel_du[2].w = -1.0f;
         kernel_du[3].x =  1; kernel_du[3].y =  1; kernel_du[3].w =  1.0f;
         kernel_du[4].x =  1; kernel_du[4].y =  0; kernel_du[4].w =  1.0f;
         kernel_du[5].x =  1; kernel_du[5].y = -1; kernel_du[5].w =  1.0f;
         
         kernel_dv[0].x = -1; kernel_dv[0].y =  1; kernel_dv[0].w =  1.0f;
         kernel_dv[1].x =  0; kernel_dv[1].y =  1; kernel_dv[1].w =  1.0f;
         kernel_dv[2].x =  1; kernel_dv[2].y =  1; kernel_dv[2].w =  1.0f;
         kernel_dv[3].x = -1; kernel_dv[3].y = -1; kernel_dv[3].w = -1.0f;
         kernel_dv[4].x =  0; kernel_dv[4].y = -1; kernel_dv[4].w = -1.0f;
         kernel_dv[5].x =  1; kernel_dv[5].y = -1; kernel_dv[5].w = -1.0f;
         
         break;      
      case FILTER_PREWITT_5x5:
         num_elements = 20;
         kernel_du = (kernel_element*)g_malloc(20 * sizeof(kernel_element));
         kernel_dv = (kernel_element*)g_malloc(20 * sizeof(kernel_element));

         kernel_du[ 0].x = -2; kernel_du[ 0].y =  2; kernel_du[ 0].w = -1.0f;
         kernel_du[ 1].x = -2; kernel_du[ 1].y =  1; kernel_du[ 1].w = -1.0f;
         kernel_du[ 2].x = -2; kernel_du[ 2].y =  0; kernel_du[ 2].w = -1.0f;
         kernel_du[ 3].x = -2; kernel_du[ 3].y = -1; kernel_du[ 3].w = -1.0f;
         kernel_du[ 4].x = -2; kernel_du[ 4].y = -2; kernel_du[ 4].w = -1.0f;
         kernel_du[ 5].x = -1; kernel_du[ 5].y =  2; kernel_du[ 5].w = -2.0f;
         kernel_du[ 6].x = -1; kernel_du[ 6].y =  1; kernel_du[ 6].w = -2.0f;
         kernel_du[ 7].x = -1; kernel_du[ 7].y =  0; kernel_du[ 7].w = -2.0f;
         kernel_du[ 8].x = -1; kernel_du[ 8].y = -1; kernel_du[ 8].w = -2.0f;
         kernel_du[ 9].x = -1; kernel_du[ 9].y = -2; kernel_du[ 9].w = -2.0f;
         kernel_du[10].x =  1; kernel_du[10].y =  2; kernel_du[10].w =  2.0f;
         kernel_du[11].x =  1; kernel_du[11].y =  1; kernel_du[11].w =  2.0f;
         kernel_du[12].x =  1; kernel_du[12].y =  0; kernel_du[12].w =  2.0f;
         kernel_du[13].x =  1; kernel_du[13].y = -1; kernel_du[13].w =  2.0f;
         kernel_du[14].x =  1; kernel_du[14].y = -2; kernel_du[14].w =  2.0f;
         kernel_du[15].x =  2; kernel_du[15].y =  2; kernel_du[15].w =  1.0f;
         kernel_du[16].x =  2; kernel_du[16].y =  1; kernel_du[16].w =  1.0f;
         kernel_du[17].x =  2; kernel_du[17].y =  0; kernel_du[17].w =  1.0f;
         kernel_du[18].x =  2; kernel_du[18].y = -1; kernel_du[18].w =  1.0f;
         kernel_du[19].x =  2; kernel_du[19].y = -2; kernel_du[19].w =  1.0f;
      
         kernel_dv[ 0].x = -2; kernel_dv[ 0].y =  2; kernel_dv[ 0].w =  1.0f;
         kernel_dv[ 1].x = -1; kernel_dv[ 1].y =  2; kernel_dv[ 1].w =  1.0f;
         kernel_dv[ 2].x =  0; kernel_dv[ 2].y =  2; kernel_dv[ 2].w =  1.0f;
         kernel_dv[ 3].x =  1; kernel_dv[ 3].y =  2; kernel_dv[ 3].w =  1.0f;
         kernel_dv[ 4].x =  2; kernel_dv[ 4].y =  2; kernel_dv[ 4].w =  1.0f;
         kernel_dv[ 5].x = -2; kernel_dv[ 5].y =  1; kernel_dv[ 5].w =  2.0f;
         kernel_dv[ 6].x = -1; kernel_dv[ 6].y =  1; kernel_dv[ 6].w =  2.0f;
         kernel_dv[ 7].x =  0; kernel_dv[ 7].y =  1; kernel_dv[ 7].w =  2.0f;
         kernel_dv[ 8].x =  1; kernel_dv[ 8].y =  1; kernel_dv[ 8].w =  2.0f;
         kernel_dv[ 9].x =  2; kernel_dv[ 9].y =  1; kernel_dv[ 9].w =  2.0f;
         kernel_dv[10].x = -2; kernel_dv[10].y = -1; kernel_dv[10].w = -2.0f;
         kernel_dv[11].x = -1; kernel_dv[11].y = -1; kernel_dv[11].w = -2.0f;
         kernel_dv[12].x =  0; kernel_dv[12].y = -1; kernel_dv[12].w = -2.0f;
         kernel_dv[13].x =  1; kernel_dv[13].y = -1; kernel_dv[13].w = -2.0f;
         kernel_dv[14].x =  2; kernel_dv[14].y = -1; kernel_dv[14].w = -2.0f;
         kernel_dv[15].x = -2; kernel_dv[15].y = -2; kernel_dv[15].w = -1.0f;
         kernel_dv[16].x = -1; kernel_dv[16].y = -2; kernel_dv[16].w = -1.0f;
         kernel_dv[17].x =  0; kernel_dv[17].y = -2; kernel_dv[17].w = -1.0f;
         kernel_dv[18].x =  1; kernel_dv[18].y = -2; kernel_dv[18].w = -1.0f;
         kernel_dv[19].x =  2; kernel_dv[19].y = -2; kernel_dv[19].w = -1.0f;
      
         break;
      case FILTER_3x3:
         num_elements = 6;
         kernel_du = (kernel_element*)g_malloc(6 * sizeof(kernel_element));
         kernel_dv = (kernel_element*)g_malloc(6 * sizeof(kernel_element));
      
         weight = 1.0f / 6.0f;
      
         kernel_du[0].x = -1; kernel_du[0].y =  1; kernel_du[0].w = -weight;
         kernel_du[1].x = -1; kernel_du[1].y =  0; kernel_du[1].w = -weight;
         kernel_du[2].x = -1; kernel_du[2].y = -1; kernel_du[2].w = -weight;
         kernel_du[3].x =  1; kernel_du[3].y =  1; kernel_du[3].w =  weight;
         kernel_du[4].x =  1; kernel_du[4].y =  0; kernel_du[4].w =  weight;
         kernel_du[5].x =  1; kernel_du[5].y = -1; kernel_du[5].w =  weight;
         
         kernel_dv[0].x = -1; kernel_dv[0].y =  1; kernel_dv[0].w =  weight;
         kernel_dv[1].x =  0; kernel_dv[1].y =  1; kernel_dv[1].w =  weight;
         kernel_dv[2].x =  1; kernel_dv[2].y =  1; kernel_dv[2].w =  weight;
         kernel_dv[3].x = -1; kernel_dv[3].y = -1; kernel_dv[3].w = -weight;
         kernel_dv[4].x =  0; kernel_dv[4].y = -1; kernel_dv[4].w = -weight;
         kernel_dv[5].x =  1; kernel_dv[5].y = -1; kernel_dv[5].w = -weight;
         break;
      case FILTER_5x5:
      {
         int n;
         float usum = 0, vsum = 0;
         float wt22 = 1.0f / 16.0f;
         float wt12 = 1.0f / 10.0f;
         float wt02 = 1.0f / 8.0f;
         float wt11 = 1.0f / 2.8f;
         num_elements = 20;
         kernel_du = (kernel_element*)g_malloc(20 * sizeof(kernel_element));
         kernel_dv = (kernel_element*)g_malloc(20 * sizeof(kernel_element));
         
         kernel_du[0 ].x = -2; kernel_du[0 ].y =  2; kernel_du[0 ].w = -wt22;
         kernel_du[1 ].x = -1; kernel_du[1 ].y =  2; kernel_du[1 ].w = -wt12;
         kernel_du[2 ].x =  1; kernel_du[2 ].y =  2; kernel_du[2 ].w =  wt12;
         kernel_du[3 ].x =  2; kernel_du[3 ].y =  2; kernel_du[3 ].w =  wt22;
         kernel_du[4 ].x = -2; kernel_du[4 ].y =  1; kernel_du[4 ].w = -wt12;
         kernel_du[5 ].x = -1; kernel_du[5 ].y =  1; kernel_du[5 ].w = -wt11;
         kernel_du[6 ].x =  1; kernel_du[6 ].y =  1; kernel_du[6 ].w =  wt11;
         kernel_du[7 ].x =  2; kernel_du[7 ].y =  1; kernel_du[7 ].w =  wt12;
         kernel_du[8 ].x = -2; kernel_du[8 ].y =  0; kernel_du[8 ].w = -wt02;
         kernel_du[9 ].x = -1; kernel_du[9 ].y =  0; kernel_du[9 ].w = -0.5f;
         kernel_du[10].x =  1; kernel_du[10].y =  0; kernel_du[10].w =  0.5f;
         kernel_du[11].x =  2; kernel_du[11].y =  0; kernel_du[11].w =  wt02;
         kernel_du[12].x = -2; kernel_du[12].y = -1; kernel_du[12].w = -wt12;
         kernel_du[13].x = -1; kernel_du[13].y = -1; kernel_du[13].w = -wt11;
         kernel_du[14].x =  1; kernel_du[14].y = -1; kernel_du[14].w =  wt11;
         kernel_du[15].x =  2; kernel_du[15].y = -1; kernel_du[15].w =  wt12;
         kernel_du[16].x = -2; kernel_du[16].y = -2; kernel_du[16].w = -wt22;
         kernel_du[17].x = -1; kernel_du[17].y = -2; kernel_du[17].w = -wt12;
         kernel_du[18].x =  1; kernel_du[18].y = -2; kernel_du[18].w =  wt12;
         kernel_du[19].x =  2; kernel_du[19].y = -2; kernel_du[19].w =  wt22;
         
         kernel_dv[0 ].x = -2; kernel_dv[0 ].y =  2; kernel_dv[0 ].w =  wt22;
         kernel_dv[1 ].x = -1; kernel_dv[1 ].y =  2; kernel_dv[1 ].w =  wt12;
         kernel_dv[2 ].x =  0; kernel_dv[2 ].y =  2; kernel_dv[2 ].w =  0.25f;
         kernel_dv[3 ].x =  1; kernel_dv[3 ].y =  2; kernel_dv[3 ].w =  wt12;
         kernel_dv[4 ].x =  2; kernel_dv[4 ].y =  2; kernel_dv[4 ].w =  wt22;
         kernel_dv[5 ].x = -2; kernel_dv[5 ].y =  1; kernel_dv[5 ].w =  wt12;
         kernel_dv[6 ].x = -1; kernel_dv[6 ].y =  1; kernel_dv[6 ].w =  wt11;
         kernel_dv[7 ].x =  0; kernel_dv[7 ].y =  1; kernel_dv[7 ].w =  0.5f;
         kernel_dv[8 ].x =  1; kernel_dv[8 ].y =  1; kernel_dv[8 ].w =  wt11;
         kernel_dv[9 ].x =  2; kernel_dv[9 ].y =  1; kernel_dv[9 ].w =  wt22;
         kernel_dv[10].x = -2; kernel_dv[10].y = -1; kernel_dv[10].w = -wt22;
         kernel_dv[11].x = -1; kernel_dv[11].y = -1; kernel_dv[11].w = -wt11;
         kernel_dv[12].x =  0; kernel_dv[12].y = -1; kernel_dv[12].w = -0.5f;
         kernel_dv[13].x =  1; kernel_dv[13].y = -1; kernel_dv[13].w = -wt11;
         kernel_dv[14].x =  2; kernel_dv[14].y = -1; kernel_dv[14].w = -wt12;
         kernel_dv[15].x = -2; kernel_dv[15].y = -2; kernel_dv[15].w = -wt22;
         kernel_dv[16].x = -1; kernel_dv[16].y = -2; kernel_dv[16].w = -wt12;
         kernel_dv[17].x =  0; kernel_dv[17].y = -2; kernel_dv[17].w = -0.25f;
         kernel_dv[18].x =  1; kernel_dv[18].y = -2; kernel_dv[18].w = -wt12;
         kernel_dv[19].x =  2; kernel_dv[19].y = -2; kernel_dv[19].w = -wt22;

         for(n = 0; n < 20; ++n)
         {
            usum += fabsf(kernel_du[n].w);
            vsum += fabsf(kernel_dv[n].w);
         }
         for(n = 0; n < 20; ++n)
         {
            kernel_du[n].w /= usum;
            kernel_dv[n].w /= vsum;
         }
         
         break;
      }
      case FILTER_7x7:
      {
         float du_weights[]=
         {
            -1, -2, -3, 0, 3, 2, 1,
            -2, -3, -4, 0, 4, 3, 2,
            -3, -4, -5, 0, 5, 4, 3,
            -4, -5, -6, 0, 6, 5, 4,
            -3, -4, -5, 0, 5, 4, 3,
            -2, -3, -4, 0, 4, 3, 2,
            -1, -2, -3, 0, 3, 2, 1   
         };
         float dv_weights[49];
         int n;
         float usum = 0, vsum = 0;
         
         num_elements = 49;
         kernel_du = (kernel_element*)g_malloc(49 * sizeof(kernel_element));
         kernel_dv = (kernel_element*)g_malloc(49 * sizeof(kernel_element));
         
         make_kernel(kernel_du, du_weights, 7);
         rotate_array(dv_weights, du_weights, 7);
         make_kernel(kernel_dv, dv_weights, 7);
         
         for(n = 0; n < 49; ++n)
         {
            usum += fabsf(kernel_du[n].w);
            vsum += fabsf(kernel_dv[n].w);
         }
         for(n = 0; n < 49; ++n)
         {
            kernel_du[n].w /= usum;
            kernel_dv[n].w /= vsum;
         }
         
         break;
      }
      case FILTER_9x9:
      {
         float du_weights[]=
         {
            -1, -2, -3, -4, 0, 4, 3, 2, 1,
            -2, -3, -4, -5, 0, 5, 4, 3, 2,
            -3, -4, -5, -6, 0, 6, 5, 4, 3,
            -4, -5, -6, -7, 0, 7, 6, 5, 4,
            -5, -6, -7, -8, 0, 8, 7, 6, 5,
            -4, -5, -6, -7, 0, 7, 6, 5, 4,
            -3, -4, -5, -6, 0, 6, 5, 4, 3,
            -2, -3, -4, -5, 0, 5, 4, 3, 2,
            -1, -2, -3, -4, 0, 4, 3, 2, 1     
         };
         float dv_weights[81];
         int n;
         float usum = 0, vsum = 0;
         
         num_elements = 81;
         kernel_du = (kernel_element*)g_malloc(81 * sizeof(kernel_element));
         kernel_dv = (kernel_element*)g_malloc(81 * sizeof(kernel_element));
         
         make_kernel(kernel_du, du_weights, 9);
         rotate_array(dv_weights, du_weights, 9);
         make_kernel(kernel_dv, dv_weights, 9);
         
         for(n = 0; n < 81; ++n)
         {
            usum += fabsf(kernel_du[n].w);
            vsum += fabsf(kernel_dv[n].w);
         }
         for(n = 0; n < 81; ++n)
         {
            kernel_du[n].w /= usum;
            kernel_dv[n].w /= vsum;
         }
         
         break;
      }
   }

   if(nmapvals.conversion == CONVERT_BIASED_RGB)
   {
      /* approximated average color of the image
       * scale to 16x16, accumulate the pixels and average */
      unsigned int sum[3];

      tmp = g_malloc(16 * 16 * bpp);
      scale_pixels(tmp, 16, 16, src, width, height, bpp);

      sum[0] = sum[1] = sum[2] = 0;
      
      s = src;
      for(y = 0; y < 16; ++y)
      {
         for(x = 0; x < 16; ++x)
         {
            sum[0] += *s++;
            sum[1] += *s++;
            sum[2] += *s++;
            if(bpp == 4) s++;
         }
      }
         
      rgb_bias[0] = (float)sum[0] / 256.0f;
      rgb_bias[1] = (float)sum[1] / 256.0f;
      rgb_bias[2] = (float)sum[2] / 256.0f;
      
      g_free(tmp);
   }
   else
   {
      rgb_bias[0] = 0;
      rgb_bias[1] = 0;
      rgb_bias[2] = 0;
   }

   if(nmapvals.conversion != CONVERT_NORMALIZE_ONLY &&
      nmapvals.conversion != CONVERT_DUDV_TO_NORMAL &&
      nmapvals.conversion != CONVERT_HEIGHTMAP)
   {
      s = src;
      for(y = 0; y < height; ++y)
      {
         for(x = 0; x < width; ++x)
         {
            if(!nmapvals.height_source)
            {
               switch(nmapvals.conversion)
               {
                  case CONVERT_NONE:
                     val = (float)s[0] * 0.3f +
                           (float)s[1] * 0.59f +
                           (float)s[2] * 0.11f;
                     break;
                  case CONVERT_BIASED_RGB:
                     val = (((float)max(0, s[0] - rgb_bias[0])) * 0.3f ) +
                           (((float)max(0, s[1] - rgb_bias[1])) * 0.59f) +
                           (((float)max(0, s[2] - rgb_bias[2])) * 0.11f);
                     break;
                  case CONVERT_RED:
                     val = (float)s[0];
                     break;
                  case CONVERT_GREEN:
                     val = (float)s[1];
                     break;
                  case CONVERT_BLUE:
                     val = (float)s[2];
                     break;
                  case CONVERT_MAX_RGB:
                     val = (float)max(s[0], max(s[1], s[2]));
                     break;
                  case CONVERT_MIN_RGB:
                     val = (float)min(s[0], min(s[1], s[2]));
                     break;
                  case CONVERT_COLORSPACE:
                     val = (1.0f - ((1.0f - ((float)s[0] / 255.0f)) *
                                    (1.0f - ((float)s[1] / 255.0f)) *
                                    (1.0f - ((float)s[2] / 255.0f)))) * 255.0f;
                     break;
                  default:
                     val = 255.0f;
                     break;
               }
            }
            else
               val = (float)s[3];
         
            heights[x + y * width] = val * oneover255;
         
            s += bpp;
         }
      }
   }

#define HEIGHT(x,y) \
   (heights[(max(0, min(width - 1, (x)))) + (max(0, min(height - 1, (y)))) * width])
#define HEIGHT_WRAP(x,y) \
   (heights[((x) < 0 ? (width + (x)) : ((x) >= width ? ((x) - width) : (x)))+ \
            (((y) < 0 ? (height + (y)) : ((y) >= height ? ((y) - height) : (y))) * width)])

   if(preview_mode)
   {
      cursor = gdk_cursor_new(GDK_WATCH);
      gdk_window_set_cursor(GDK_WINDOW(dialog->window), cursor);
      gdk_cursor_unref(cursor);
   }
   
   for(y = 0; y < height; ++y)
   {
      while(gtk_events_pending())
         gtk_main_iteration();
      
      for(x = 0; x < width; ++x)
      {
         d = dst + ((y * rowbytes) + (x * bpp));
         s = src + ((y * rowbytes) + (x * bpp));

         if(nmapvals.conversion == CONVERT_NORMALIZE_ONLY ||
            nmapvals.conversion == CONVERT_HEIGHTMAP)
         {
            n[0] = (((float)s[0] * oneover255) - 0.5f) * 2.0f;
            n[1] = (((float)s[1] * oneover255) - 0.5f) * 2.0f;
            n[2] = (((float)s[2] * oneover255) - 0.5f) * 2.0f;
            n[0] *= nmapvals.scale;
            n[1] *= nmapvals.scale;
         }
         else if(nmapvals.conversion == CONVERT_DUDV_TO_NORMAL)
         {
            n[0] = (((float)s[0] * oneover255) - 0.5f) * 2.0f;
            n[1] = (((float)s[1] * oneover255) - 0.5f) * 2.0f;
            n[2] = sqrtf(1.0f - (n[0] * n[0] - n[1] * n[1]));
            n[0] *= nmapvals.scale;
            n[1] *= nmapvals.scale;
         }
         else
         {
            du = 0; dv = 0;
            if(!nmapvals.wrap)
            {
               for(i = 0; i < num_elements; ++i)
                  du += HEIGHT(x + kernel_du[i].x,
                               y + kernel_du[i].y) * kernel_du[i].w;
               for(i = 0; i < num_elements; ++i)
                  dv += HEIGHT(x + kernel_dv[i].x,
                               y + kernel_dv[i].y) * kernel_dv[i].w;
            }
            else
            {
               for(i = 0; i < num_elements; ++i)
                  du += HEIGHT_WRAP(x + kernel_du[i].x,
                                    y + kernel_du[i].y) * kernel_du[i].w;
               for(i = 0; i < num_elements; ++i)
                  dv += HEIGHT_WRAP(x + kernel_dv[i].x,
                                    y + kernel_dv[i].y) * kernel_dv[i].w;
            }
            
            n[0] = -du * nmapvals.scale;
            n[1] = -dv * nmapvals.scale;
            n[2] = 1.0f;
         }
         
         NORMALIZE(n);
         
         if(n[2] < nmapvals.minz)
         {
            n[2] = nmapvals.minz;
            NORMALIZE(n);
         }
         
         if(nmapvals.xinvert) n[0] = -n[0];
         if(nmapvals.yinvert) n[1] = -n[1];
         if(nmapvals.swapRGB)
         {
            val = n[0];
            n[0] = n[2];
            n[2] = val;
         }
         
         if(!nmapvals.dudv)
         {
            *d++ = (unsigned char)((n[0] + 1.0f) * 127.5f);
            *d++ = (unsigned char)((n[1] + 1.0f) * 127.5f);
            *d++ = (unsigned char)((n[2] + 1.0f) * 127.5f);
         
            if(drawable->bpp == 4)
            {
               switch(nmapvals.alpha)
               {
                  case ALPHA_NONE:
                     *d++ = s[3]; break;
                  case ALPHA_HEIGHT:
                     *d++ = (unsigned char)(heights[x + y * width] * 255.0f); break;
                  case ALPHA_INVERSE_HEIGHT:
                     *d++ = 255 - (unsigned char)(heights[x + y * width] * 255.0f); break;
                  case ALPHA_ZERO:
                     *d++ = 0; break;
                  case ALPHA_ONE:
                     *d++ = 255; break;
                  case ALPHA_INVERT:
                     *d++ = 255 - s[3]; break;
                  case ALPHA_MAP:
                     *d++ = sample_alpha_map(amap, x, y, amap_w, amap_h,
                                             width, height); break;
                  default:
                     *d++ = s[3]; break;
               }
            }
         }
         else
         {
            if(nmapvals.dudv == DUDV_8BIT_SIGNED ||
               nmapvals.dudv == DUDV_8BIT_UNSIGNED)
            {
               if(nmapvals.dudv == DUDV_8BIT_UNSIGNED)
               {
                  n[0] += 1.0f;
                  n[1] += 1.0f;
               }
               *d++ = (unsigned char)(n[0] * 127.5f);
               *d++ = (unsigned char)(n[1] * 127.5f);
               *d++ = 0;
               if(drawable->bpp == 4) *d++ = 255;
            }
            else if(nmapvals.dudv == DUDV_16BIT_SIGNED ||
                    nmapvals.dudv == DUDV_16BIT_UNSIGNED)
            {
               unsigned short *d16 = (unsigned short*)d;
               if(nmapvals.dudv == DUDV_16BIT_UNSIGNED)
               {
                  n[0] += 1.0f;
                  n[1] += 1.0f;
               }
               *d16++ = (unsigned short)(n[0] * 32767.5f);
               *d16++ = (unsigned short)(n[1] * 32767.5f);
            }
         }
      }

      if(!preview_mode)
         gimp_progress_update((double)(y - 1) / (double)(height - 2));
   }
   
   if(nmapvals.conversion == CONVERT_HEIGHTMAP)
      make_heightmap(dst, width, height, bpp);
   
#undef HEIGHT
#undef HEIGHT_WRAP

   if(preview_mode)
   {
      update_3D_preview(width, height, bpp, dst);
      
      pw = GIMP_PREVIEW_AREA(preview)->width;
      ph = GIMP_PREVIEW_AREA(preview)->height;
      rowbytes = pw * bpp;
      
      tmp = g_malloc(pw * ph * bpp);
      scale_pixels(tmp, pw, ph, dst, width, height, bpp);
      
      gimp_preview_area_draw(GIMP_PREVIEW_AREA(preview), 0, 0, pw, ph,
                             (bpp == 4) ? GIMP_RGBA_IMAGE : GIMP_RGB_IMAGE,
                             tmp, rowbytes);
      
      g_free(tmp);
      
      gdk_window_set_cursor(GDK_WINDOW(dialog->window), 0);
   }
   else
   {
      gimp_progress_update(100.0);
      
      gimp_pixel_rgn_init(&dst_rgn, drawable, 0, 0, width, height, 1, 1);
      gimp_pixel_rgn_set_rect(&dst_rgn, dst, 0, 0, width, height);
      
      gimp_drawable_flush(drawable);
      gimp_drawable_merge_shadow(drawable->drawable_id, 1);
      gimp_drawable_update(drawable->drawable_id, 0, 0, width, height);
   }
   
   g_free(heights);
   g_free(src);
   g_free(dst);
   g_free(kernel_du);
   g_free(kernel_dv);
   if(amap) g_free(amap);
   
   return(0);
}

static void do_cleanup(gpointer data)
{
   destroy_3D_preview();
   gtk_main_quit();
}

static int update_preview = 0;

static gint idle_callback(gpointer data)
{
   if(update_preview)
   {
      update_preview = 0;
      normalmap((GimpDrawable*)data, TRUE);
   }
   return(1);
}

static void filter_type_selected(GtkWidget *widget, gpointer data)
{
   if(nmapvals.filter != (gint)((size_t)data))
   {
      nmapvals.filter = (gint)((size_t)data);
      update_preview = 1;
   }
}

static void minz_changed(GtkWidget *widget, gpointer data)
{
   nmapvals.minz = gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
   update_preview = 1;
}

static void scale_changed(GtkWidget *widget, gpointer data)
{
   nmapvals.scale = gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
   update_preview = 1;
}

static void height_source_selected(GtkWidget *widget, gpointer data)
{
   GtkWidget *opt;
   
   if(nmapvals.height_source == (gint)((size_t)data)) return;
   
   nmapvals.height_source = (gint)((size_t)data);
   
   opt = g_object_get_data(G_OBJECT(widget), "conversion_opt");
   if(!nmapvals.height_source)
   {
      gtk_widget_set_sensitive(opt, 1);
   }
   else
   {
      nmapvals.conversion = CONVERT_NONE;
      gtk_option_menu_set_history(GTK_OPTION_MENU(opt), nmapvals.conversion);
      gtk_widget_set_sensitive(opt, 0);
   }
      
   update_preview = 1;
}

static void alpha_result_selected(GtkWidget *widget, gpointer data)
{
   if(nmapvals.alpha != (gint)((size_t)data))
   {
      nmapvals.alpha = (gint)((size_t)data);
      update_preview = 1;
   }
}

static void conversion_selected(GtkWidget *widget, gpointer data)
{
   GtkWidget *contrast_spin;
   
   if(nmapvals.conversion != (gint)((size_t)data))
   {
      nmapvals.conversion = (gint)((size_t)data);
      contrast_spin = g_object_get_data(G_OBJECT(widget), "contrast_spin");
      gtk_widget_set_sensitive(contrast_spin, nmapvals.conversion == CONVERT_HEIGHTMAP);
      update_preview = 1;
   }
}

static void preview_clicked(GtkWidget *widget, gpointer data)
{
   GimpDrawable *drawable;
   if(!is_3D_preview_active())
   {
      drawable = g_object_get_data(G_OBJECT(widget), "drawable");
      show_3D_preview(drawable);
      update_preview = 1;
   }
}

static void dudv_selected(GtkWidget *widget, gpointer data)
{
   GimpDrawable *drawable;
   GtkWidget *opt;
   
   if(nmapvals.dudv == (gint)((size_t)data)) return;
   
   nmapvals.dudv = (gint)((size_t)data);
   
   drawable = g_object_get_data(G_OBJECT(widget), "drawable");
   opt = g_object_get_data(G_OBJECT(widget), "alpha_opt");

   if(nmapvals.dudv == DUDV_NONE)
   {
      if(drawable->bpp == 4)
         gtk_widget_set_sensitive(opt, 1);
   }
   else
   {
      nmapvals.alpha = 0;
      gtk_option_menu_set_history(GTK_OPTION_MENU(opt), nmapvals.alpha);
      gtk_widget_set_sensitive(opt, 0);
   }
      
   update_preview = 1;
}

static void contrast_changed(GtkWidget *widget, gpointer data)
{
   nmapvals.contrast = gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
   update_preview = 1;
}

static void toggle_clicked(GtkWidget *widget, gpointer data)
{
   *((int*)data) = !(*((int*)data));
   update_preview = 1;
}

static gint dialog_constrain(gint32 image_id, gint32 drawable_id,
                             gpointer data)
{
   if(drawable_id == -1) return(1);
   if(gimp_drawable_is_gray(drawable_id))
   {
      *(int*)data += 1;
      return(1);
   }
   return(0);
}

static void alphamap_callback(gint32 id, gpointer data)
{
   if(nmapvals.alphamap_id != id)
   {
      nmapvals.alphamap_id = id;
      update_preview = 1;
   }
}

static void normalmap_dialog_response(GtkWidget *widget, gint response_id,
                                      gpointer data)
{
   switch(response_id)
   {
      case GTK_RESPONSE_OK:
         runme = 1;
      default:
         gtk_widget_destroy(widget);
         break;
   }
}

static gint normalmap_dialog(GimpDrawable *drawable)
{
   GtkWidget *hbox, *vbox, *abox;
   GtkWidget *btn;
   GtkWidget *table;
   GtkWidget *opt, *alpha_result_opt;
   GtkWidget *menu;
   GtkWidget *menuitem, *item_height_source[2];
   GtkWidget *label;
   GtkObject *adj;
   GtkWidget *spin;
   GtkWidget *frame;
   GtkWidget *check;
   GtkWidget *conversion_menu;
   GList *curr;
   int num_amaps = 0;
   
   if(nmapvals.alpha == ALPHA_MAP)
   {
      if(!gimp_drawable_get(nmapvals.alphamap_id) ||
         !gimp_drawable_is_gray(nmapvals.alphamap_id))
      {
         nmapvals.alpha = 0;
         nmapvals.alphamap_id = 0;
      }
   }
   
   gimp_ui_init("normalmap", TRUE);
      
   dialog = gimp_dialog_new("Normalmap", "normalmap",
                            0, 0, gimp_standard_help_func, 0,
                            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                            GTK_STOCK_OK, GTK_RESPONSE_OK,
                            NULL);

   gtk_signal_connect(GTK_OBJECT(dialog), "response",
                      GTK_SIGNAL_FUNC(normalmap_dialog_response),
                      0);
   gtk_signal_connect(GTK_OBJECT(dialog), "destroy",
                      GTK_SIGNAL_FUNC(do_cleanup),
                      0);
   
   hbox = gtk_hbox_new(0, 8);
   gtk_container_set_border_width(GTK_CONTAINER(hbox), 8);
   gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),hbox, 1, 1, 0);
   gtk_widget_show(hbox);
   
   vbox = gtk_vbox_new(0, 8);
   gtk_box_pack_start(GTK_BOX(hbox), vbox, 1, 1, 0);
   gtk_widget_show(vbox);
   
   frame = gtk_frame_new("Preview");
   gtk_box_pack_start(GTK_BOX(vbox), frame, 0, 0, 0);
   gtk_widget_show(frame);
   
   abox = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
   gtk_container_set_border_width(GTK_CONTAINER (abox), 4);
   gtk_container_add(GTK_CONTAINER(frame), abox);
   gtk_widget_show(abox);

   frame = gtk_frame_new(NULL);
   gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
   gtk_container_add(GTK_CONTAINER(abox), frame);
   gtk_widget_show(frame);
   
   preview = gimp_preview_area_new();
   gimp_preview_area_set_max_size(GIMP_PREVIEW_AREA(preview), PREVIEW_SIZE, PREVIEW_SIZE);
   gtk_drawing_area_size(GTK_DRAWING_AREA(preview), PREVIEW_SIZE, PREVIEW_SIZE);
   gtk_container_add(GTK_CONTAINER(frame), preview);
   gtk_widget_show(preview);
   
   btn = gtk_button_new_with_label("3D Preview");
   gtk_signal_connect(GTK_OBJECT(btn), "clicked",
                      GTK_SIGNAL_FUNC(preview_clicked), 0);
   g_object_set_data(G_OBJECT(btn), "drawable", drawable);
   gtk_box_pack_start(GTK_BOX(vbox), btn, 0, 0, 0);
   gtk_widget_show(btn);

   label = gtk_label_new("Alpha map:");
   gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
   gtk_widget_show(label);
   gtk_box_pack_start(GTK_BOX(vbox), label, 0, 0, 0);
   
   opt = gtk_option_menu_new();
   gtk_widget_show(opt);
   menu = gimp_drawable_menu_new(dialog_constrain,
                                 alphamap_callback,
                                 &num_amaps, drawable->drawable_id);
   gtk_option_menu_set_menu(GTK_OPTION_MENU(opt), menu);
   gtk_box_pack_start(GTK_BOX(vbox), opt, 0, 0, 0);
   
   if(drawable->bpp != 4)
      gtk_widget_set_sensitive(opt, 0);
   
   table = gtk_table_new(9, 2, 0);
   gtk_widget_show(table);
   gtk_box_pack_start(GTK_BOX(hbox), table, 1, 1, 0);
   gtk_table_set_row_spacings(GTK_TABLE(table), 8);
   gtk_table_set_col_spacings(GTK_TABLE(table), 8);
   
   opt = gtk_option_menu_new();
   gtk_widget_show(opt);
   gimp_table_attach_aligned(GTK_TABLE(table), 0, 0, "Filter:", 0, 0.5,
                             opt, 1, 0);

   menu = gtk_menu_new();

   menuitem = gtk_menu_item_new_with_label("4 sample");
   gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
                      GTK_SIGNAL_FUNC(filter_type_selected),
                      (gpointer)FILTER_NONE);
   gtk_widget_show(menuitem);
   gtk_menu_append(GTK_MENU(menu), menuitem);
   menuitem = gtk_menu_item_new_with_label("Sobel 3x3");
   gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
                      GTK_SIGNAL_FUNC(filter_type_selected),
                      (gpointer)FILTER_SOBEL_3x3);
   gtk_widget_show(menuitem);
   gtk_menu_append(GTK_MENU(menu), menuitem);
   menuitem = gtk_menu_item_new_with_label("Sobel 5x5");
   gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
                      GTK_SIGNAL_FUNC(filter_type_selected),
                      (gpointer)FILTER_SOBEL_5x5);
   gtk_widget_show(menuitem);
   gtk_menu_append(GTK_MENU(menu), menuitem);
   menuitem = gtk_menu_item_new_with_label("Prewitt 3x3");
   gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
                      GTK_SIGNAL_FUNC(filter_type_selected),
                      (gpointer)FILTER_PREWITT_3x3);
   gtk_widget_show(menuitem);
   gtk_menu_append(GTK_MENU(menu), menuitem);
   menuitem = gtk_menu_item_new_with_label("Prewitt 5x5");
   gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
                      GTK_SIGNAL_FUNC(filter_type_selected),
                      (gpointer)FILTER_PREWITT_5x5);
   gtk_widget_show(menuitem);
   gtk_menu_append(GTK_MENU(menu), menuitem);
   menuitem = gtk_menu_item_new_with_label("3x3");
   gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
                      GTK_SIGNAL_FUNC(filter_type_selected),
                      (gpointer)FILTER_3x3);
   gtk_widget_show(menuitem);
   gtk_menu_append(GTK_MENU(menu), menuitem);
   menuitem = gtk_menu_item_new_with_label("5x5");
   gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
                      GTK_SIGNAL_FUNC(filter_type_selected),
                      (gpointer)FILTER_5x5);
   gtk_widget_show(menuitem);
   gtk_menu_append(GTK_MENU(menu), menuitem);
   menuitem = gtk_menu_item_new_with_label("7x7");
   gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
                      GTK_SIGNAL_FUNC(filter_type_selected),
                      (gpointer)FILTER_7x7);
   gtk_widget_show(menuitem);
   gtk_menu_append(GTK_MENU(menu), menuitem);
   menuitem = gtk_menu_item_new_with_label("9x9");
   gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
                      GTK_SIGNAL_FUNC(filter_type_selected),
                      (gpointer)FILTER_9x9);
   gtk_widget_show(menuitem);
   gtk_menu_append(GTK_MENU(menu), menuitem);
   gtk_menu_set_active(GTK_MENU(menu), nmapvals.filter);
   gtk_option_menu_set_menu(GTK_OPTION_MENU(opt), menu);
   
   adj = gtk_adjustment_new(nmapvals.minz, 0, 1, 0.01, 0.05, 0.1);
   spin = gtk_spin_button_new(GTK_ADJUSTMENT(adj), 0.01, 5);
   gtk_widget_show(spin);
   gtk_spin_button_set_update_policy(GTK_SPIN_BUTTON(spin), GTK_UPDATE_IF_VALID);
   gtk_signal_connect(GTK_OBJECT(spin), "value_changed",
                      GTK_SIGNAL_FUNC(minz_changed), 0);
   gimp_table_attach_aligned(GTK_TABLE(table), 0, 1, "Minimum Z:", 0, 0.5,
                             spin, 1, 0);

   adj = gtk_adjustment_new(nmapvals.scale, -100, 100, 1, 5, 5);
   spin = gtk_spin_button_new(GTK_ADJUSTMENT(adj), 1, 5);
   gtk_widget_show(spin);
   gtk_spin_button_set_update_policy(GTK_SPIN_BUTTON(spin), GTK_UPDATE_IF_VALID);
   gtk_signal_connect(GTK_OBJECT(spin), "value_changed",
                      GTK_SIGNAL_FUNC(scale_changed), 0);
   gimp_table_attach_aligned(GTK_TABLE(table), 0, 2, "Scale:", 0, 0.5,
                             spin, 1, 0);

   opt = gtk_option_menu_new();
   gtk_widget_show(opt);
   gimp_table_attach_aligned(GTK_TABLE(table), 0, 3, "Height source:", 0, 0.5,
                             opt, 1, 0);

   menu = gtk_menu_new();

   if(drawable->bpp != 4)
      nmapvals.height_source = 0;
   
   menuitem = item_height_source[0] = gtk_menu_item_new_with_label("Average RGB");
   gtk_signal_connect(GTK_OBJECT(menuitem), "activate", 
                      GTK_SIGNAL_FUNC(height_source_selected), 
                      (gpointer)0);
   gtk_widget_show(menuitem);
   gtk_menu_append(GTK_MENU(menu), menuitem);
   menuitem = item_height_source[1] = gtk_menu_item_new_with_label("Alpha");
   gtk_signal_connect(GTK_OBJECT(menuitem), "activate", 
                      GTK_SIGNAL_FUNC(height_source_selected), 
                      (gpointer)1);
   gtk_widget_show(menuitem);
   gtk_menu_append(GTK_MENU(menu), menuitem);

   gtk_menu_set_active(GTK_MENU(menu), nmapvals.height_source);
   gtk_option_menu_set_menu(GTK_OPTION_MENU(opt), menu);
   
   if(drawable->bpp != 4)
      gtk_widget_set_sensitive(opt, 0);
   
   opt = alpha_result_opt = gtk_option_menu_new();
   gtk_widget_show(opt);
   gimp_table_attach_aligned(GTK_TABLE(table), 0, 4, "Alpha channel:", 0, 0.5,
                             opt, 1, 0);

   if(drawable->bpp != 4)
      nmapvals.alpha = 0;
      
   menu = gtk_menu_new();
   
   menuitem = gtk_menu_item_new_with_label("Unchanged");
   gtk_signal_connect(GTK_OBJECT(menuitem), "activate", 
                      GTK_SIGNAL_FUNC(alpha_result_selected), 
                      (gpointer)ALPHA_NONE);
   gtk_widget_show(menuitem);
   gtk_menu_append(GTK_MENU(menu), menuitem);
   menuitem = gtk_menu_item_new_with_label("Height");
   gtk_signal_connect(GTK_OBJECT(menuitem), "activate", 
                      GTK_SIGNAL_FUNC(alpha_result_selected), 
                      (gpointer)ALPHA_HEIGHT);
   gtk_widget_show(menuitem);
   gtk_menu_append(GTK_MENU(menu), menuitem);
   menuitem = gtk_menu_item_new_with_label("Inverse height");
   gtk_signal_connect(GTK_OBJECT(menuitem), "activate", 
                      GTK_SIGNAL_FUNC(alpha_result_selected), 
                      (gpointer)ALPHA_INVERSE_HEIGHT);
   gtk_widget_show(menuitem);
   gtk_menu_append(GTK_MENU(menu), menuitem);
   menuitem = gtk_menu_item_new_with_label("Set to 0");
   gtk_signal_connect(GTK_OBJECT(menuitem), "activate", 
                      GTK_SIGNAL_FUNC(alpha_result_selected), 
                      (gpointer)ALPHA_ZERO);
   gtk_widget_show(menuitem);
   gtk_menu_append(GTK_MENU(menu), menuitem);
   menuitem = gtk_menu_item_new_with_label("Set to 1");
   gtk_signal_connect(GTK_OBJECT(menuitem), "activate", 
                      GTK_SIGNAL_FUNC(alpha_result_selected), 
                      (gpointer)ALPHA_ONE);
   gtk_widget_show(menuitem);
   gtk_menu_append(GTK_MENU(menu), menuitem);
   menuitem = gtk_menu_item_new_with_label("Invert");
   gtk_signal_connect(GTK_OBJECT(menuitem), "activate", 
                      GTK_SIGNAL_FUNC(alpha_result_selected), 
                      (gpointer)ALPHA_INVERT);
   gtk_widget_show(menuitem);
   gtk_menu_append(GTK_MENU(menu), menuitem);
   menuitem = gtk_menu_item_new_with_label("Use alpha map");
   gtk_signal_connect(GTK_OBJECT(menuitem), "activate", 
                      GTK_SIGNAL_FUNC(alpha_result_selected), 
                      (gpointer)ALPHA_MAP);
   gtk_widget_show(menuitem);
   gtk_menu_append(GTK_MENU(menu), menuitem);

   if(num_amaps == 0)
   {
      gtk_widget_set_sensitive(menuitem, 0);
      if(nmapvals.alpha == ALPHA_MAP)
         nmapvals.alpha = 0;
   }

   gtk_menu_set_active(GTK_MENU(menu), nmapvals.alpha);
   gtk_option_menu_set_menu(GTK_OPTION_MENU(opt), menu);
   
   if(drawable->bpp !=4 || nmapvals.dudv != DUDV_NONE)
      gtk_widget_set_sensitive(opt, 0);

   opt = gtk_option_menu_new();
   gtk_widget_show(opt);
   gimp_table_attach_aligned(GTK_TABLE(table), 0, 5, "Conversion:", 0, 0.5,
                             opt, 1, 0);

   g_object_set_data(G_OBJECT(item_height_source[0]), "conversion_opt", opt);
   g_object_set_data(G_OBJECT(item_height_source[1]), "conversion_opt", opt);
   
   conversion_menu = menu = gtk_menu_new();
   
   menuitem = gtk_menu_item_new_with_label("None");
   gtk_signal_connect(GTK_OBJECT(menuitem), "activate", 
                      GTK_SIGNAL_FUNC(conversion_selected), 
                      (gpointer)CONVERT_NONE);
   gtk_widget_show(menuitem);
   gtk_menu_append(GTK_MENU(menu), menuitem);
   menuitem = gtk_menu_item_new_with_label("Biased RGB");
   gtk_signal_connect(GTK_OBJECT(menuitem), "activate", 
                      GTK_SIGNAL_FUNC(conversion_selected), 
                      (gpointer)CONVERT_BIASED_RGB);
   gtk_widget_show(menuitem);
   gtk_menu_append(GTK_MENU(menu), menuitem);
   menuitem = gtk_menu_item_new_with_label("Red");
   gtk_signal_connect(GTK_OBJECT(menuitem), "activate", 
                      GTK_SIGNAL_FUNC(conversion_selected), 
                      (gpointer)CONVERT_RED);
   gtk_widget_show(menuitem);
   gtk_menu_append(GTK_MENU(menu), menuitem);
   menuitem = gtk_menu_item_new_with_label("Green");
   gtk_signal_connect(GTK_OBJECT(menuitem), "activate", 
                      GTK_SIGNAL_FUNC(conversion_selected), 
                      (gpointer)CONVERT_GREEN);
   gtk_widget_show(menuitem);
   gtk_menu_append(GTK_MENU(menu), menuitem);
   menuitem = gtk_menu_item_new_with_label("Blue");
   gtk_signal_connect(GTK_OBJECT(menuitem), "activate", 
                      GTK_SIGNAL_FUNC(conversion_selected), 
                      (gpointer)CONVERT_BLUE);
   gtk_widget_show(menuitem);
   gtk_menu_append(GTK_MENU(menu), menuitem);
   menuitem = gtk_menu_item_new_with_label("Max RGB");
   gtk_signal_connect(GTK_OBJECT(menuitem), "activate", 
                      GTK_SIGNAL_FUNC(conversion_selected), 
                      (gpointer)CONVERT_MAX_RGB);
   gtk_widget_show(menuitem);
   gtk_menu_append(GTK_MENU(menu), menuitem);
   menuitem = gtk_menu_item_new_with_label("Min RGB");
   gtk_signal_connect(GTK_OBJECT(menuitem), "activate", 
                      GTK_SIGNAL_FUNC(conversion_selected), 
                      (gpointer)CONVERT_MIN_RGB);
   gtk_widget_show(menuitem);
   gtk_menu_append(GTK_MENU(menu), menuitem);
   menuitem = gtk_menu_item_new_with_label("Colorspace");
   gtk_signal_connect(GTK_OBJECT(menuitem), "activate", 
                      GTK_SIGNAL_FUNC(conversion_selected), 
                      (gpointer)CONVERT_COLORSPACE);
   gtk_widget_show(menuitem);
   gtk_menu_append(GTK_MENU(menu), menuitem);
   menuitem = gtk_menu_item_new_with_label("Normalize only");
   gtk_signal_connect(GTK_OBJECT(menuitem), "activate", 
                      GTK_SIGNAL_FUNC(conversion_selected), 
                      (gpointer)CONVERT_NORMALIZE_ONLY);
   gtk_widget_show(menuitem);
   gtk_menu_append(GTK_MENU(menu), menuitem);
   menuitem = gtk_menu_item_new_with_label("DUDV to Normal");
   gtk_signal_connect(GTK_OBJECT(menuitem), "activate", 
                      GTK_SIGNAL_FUNC(conversion_selected), 
                      (gpointer)CONVERT_DUDV_TO_NORMAL);
   gtk_widget_show(menuitem);
   gtk_menu_append(GTK_MENU(menu), menuitem);
   menuitem = gtk_menu_item_new_with_label("Convert to height");
   gtk_signal_connect(GTK_OBJECT(menuitem), "activate", 
                      GTK_SIGNAL_FUNC(conversion_selected), 
                      (gpointer)CONVERT_HEIGHTMAP);
   gtk_widget_show(menuitem);
   gtk_menu_append(GTK_MENU(menu), menuitem);

   gtk_menu_set_active(GTK_MENU(menu), nmapvals.conversion);
   gtk_option_menu_set_menu(GTK_OPTION_MENU(opt), menu);
   
   if(nmapvals.height_source)
      gtk_widget_set_sensitive(opt, 0);

   opt = gtk_option_menu_new();
   gtk_widget_show(opt);
   gimp_table_attach_aligned(GTK_TABLE(table), 0, 6, "DU/DV map:", 0, 0.5,
                             opt, 1, 0);

   if(drawable->bpp != 4 && (nmapvals.dudv == DUDV_16BIT_SIGNED ||
                             nmapvals.dudv == DUDV_16BIT_UNSIGNED))
      nmapvals.dudv = DUDV_NONE;
   
   menu = gtk_menu_new();

   menuitem = gtk_menu_item_new_with_label("None");
   gtk_signal_connect(GTK_OBJECT(menuitem), "activate", 
                      GTK_SIGNAL_FUNC(dudv_selected), 
                      (gpointer)DUDV_NONE);
   g_object_set_data(G_OBJECT(menuitem), "drawable", drawable);
   g_object_set_data(G_OBJECT(menuitem), "alpha_opt", alpha_result_opt);
   gtk_widget_show(menuitem);
   gtk_menu_append(GTK_MENU(menu), menuitem);
   menuitem = gtk_menu_item_new_with_label("8 bits");
   gtk_signal_connect(GTK_OBJECT(menuitem), "activate", 
                      GTK_SIGNAL_FUNC(dudv_selected), 
                      (gpointer)DUDV_8BIT_SIGNED);
   g_object_set_data(G_OBJECT(menuitem), "drawable", drawable);
   g_object_set_data(G_OBJECT(menuitem), "alpha_opt", alpha_result_opt);
   gtk_widget_show(menuitem);
   gtk_menu_append(GTK_MENU(menu), menuitem);
   menuitem = gtk_menu_item_new_with_label("8 bits (unsigned)");
   gtk_signal_connect(GTK_OBJECT(menuitem), "activate", 
                      GTK_SIGNAL_FUNC(dudv_selected), 
                      (gpointer)DUDV_8BIT_UNSIGNED);
   g_object_set_data(G_OBJECT(menuitem), "drawable", drawable);
   g_object_set_data(G_OBJECT(menuitem), "alpha_opt", alpha_result_opt);
   gtk_widget_show(menuitem);
   gtk_menu_append(GTK_MENU(menu), menuitem);
   menuitem = gtk_menu_item_new_with_label("16 bits");
   gtk_signal_connect(GTK_OBJECT(menuitem), "activate", 
                      GTK_SIGNAL_FUNC(dudv_selected), 
                      (gpointer)DUDV_16BIT_SIGNED);
   g_object_set_data(G_OBJECT(menuitem), "drawable", drawable);
   g_object_set_data(G_OBJECT(menuitem), "alpha_opt", alpha_result_opt);
   gtk_widget_show(menuitem);
   gtk_menu_append(GTK_MENU(menu), menuitem);
   if(drawable->bpp != 4)
      gtk_widget_set_sensitive(menuitem, 0);
   menuitem = gtk_menu_item_new_with_label("16 bits (unsigned)");
   gtk_signal_connect(GTK_OBJECT(menuitem), "activate", 
                      GTK_SIGNAL_FUNC(dudv_selected), 
                      (gpointer)DUDV_16BIT_UNSIGNED);
   g_object_set_data(G_OBJECT(menuitem), "drawable", drawable);
   g_object_set_data(G_OBJECT(menuitem), "alpha_opt", alpha_result_opt);
   gtk_widget_show(menuitem);
   gtk_menu_append(GTK_MENU(menu), menuitem);
   if(drawable->bpp != 4)
      gtk_widget_set_sensitive(menuitem, 0);

   gtk_menu_set_active(GTK_MENU(menu), nmapvals.dudv);
   gtk_option_menu_set_menu(GTK_OPTION_MENU(opt), menu);

   adj = gtk_adjustment_new(nmapvals.contrast, 0, 1, 0.01, 0.05, 0.1);
   spin = gtk_spin_button_new(GTK_ADJUSTMENT(adj), 0.01, 5);
   gtk_widget_show(spin);
   gtk_spin_button_set_update_policy(GTK_SPIN_BUTTON(spin), GTK_UPDATE_IF_VALID);
   gtk_signal_connect(GTK_OBJECT(spin), "value_changed",
                      GTK_SIGNAL_FUNC(contrast_changed), 0);
   gimp_table_attach_aligned(GTK_TABLE(table), 0, 7, "Contrast:", 0, 0.5,
                             spin, 1, 0);

   gtk_widget_set_sensitive(spin, nmapvals.conversion == CONVERT_HEIGHTMAP);

   curr = gtk_container_get_children(GTK_CONTAINER(conversion_menu));
   while(curr)
   {
      g_object_set_data(G_OBJECT(curr->data), "contrast_spin", spin);
      curr = curr->next;
   }
   
   frame = gtk_frame_new("Options");
   gtk_box_pack_start(GTK_BOX(hbox), frame, 0, 1, 0);
   gtk_widget_show(frame);

   vbox = gtk_vbox_new(0, 0);
   gtk_widget_show(vbox);
   gtk_container_set_border_width(GTK_CONTAINER(vbox), 4);
   gtk_container_add(GTK_CONTAINER(frame), vbox);

   check = gtk_check_button_new_with_label("Wrap");
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), nmapvals.wrap);
   gtk_widget_show(check);
   gtk_box_pack_start(GTK_BOX(vbox), check, 0, 1, 0);
   gtk_signal_connect(GTK_OBJECT(check), "clicked",
                      GTK_SIGNAL_FUNC(toggle_clicked), &nmapvals.wrap);
   check = gtk_check_button_new_with_label("Invert X");
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), nmapvals.xinvert);
   gtk_widget_show(check);
   gtk_box_pack_start(GTK_BOX(vbox), check, 0, 1, 0);
   gtk_signal_connect(GTK_OBJECT(check), "clicked",
                      GTK_SIGNAL_FUNC(toggle_clicked), &nmapvals.xinvert);
   check = gtk_check_button_new_with_label("Invert Y");
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), nmapvals.yinvert);
   gtk_widget_show(check);
   gtk_box_pack_start(GTK_BOX(vbox), check, 0, 1, 0);
   gtk_signal_connect(GTK_OBJECT(check), "clicked",
                      GTK_SIGNAL_FUNC(toggle_clicked), &nmapvals.yinvert);
   check = gtk_check_button_new_with_label("Swap RGB");
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), nmapvals.swapRGB);
   gtk_widget_show(check);
   gtk_box_pack_start(GTK_BOX(vbox), check, 0, 1, 0);
   gtk_signal_connect(GTK_OBJECT(check), "clicked",
                      GTK_SIGNAL_FUNC(toggle_clicked), &nmapvals.swapRGB);
   
   gtk_widget_show(dialog);

   update_preview = 1;
   gtk_timeout_add(100, idle_callback, drawable);
   
   runme = 0;
   
   gtk_main();
   
   return(runme);
}
