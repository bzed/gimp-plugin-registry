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

#include "scale.h"

void scale_pixels(unsigned char *dst, int dw, int dh,
                  unsigned char *src, int sw, int sh,
                  int bpp)
{
   int x, y, n, ix, iy, wx, wy, v;
   int a, b, c, d;
   int dstride = dw * bpp;
   unsigned char *s;
   
   for(y = 0; y < dh; ++y)
   {
      if(dh > 1)
      {
         iy = (((sh - 1) * y) << 7) / (dh - 1);
         if(y == dh - 1) --iy;
         wy = iy & 0x7f;
         iy >>= 7;
      }
      else
         iy = wy = 0;
      
      for(x = 0; x < dw; ++x)
      {
         if(dw > 1)
         {
            ix = (((sw - 1) * x) << 7) / (dw - 1);
            if(x == dw - 1) --ix;
            wx = ix & 0x7f;
            ix >>= 7;
         }
         else
            ix = wx = 0;
         
         s = src + ((iy - 1) * sw + (ix - 1)) * bpp;
         
         for(n = 0; n < bpp; ++n)
         {
            b = icerp(s[(sw + 0) * bpp],
                      s[(sw + 1) * bpp],
                      s[(sw + 2) * bpp],
                      s[(sw + 3) * bpp], wx);
            if(iy > 0)
            {
               a = icerp(s[      0],
                         s[    bpp],
                         s[2 * bpp],
                         s[3 * bpp], wx);
            }
            else
               a = b;
            
            c = icerp(s[(2 * sw + 0) * bpp],
                      s[(2 * sw + 1) * bpp],
                      s[(2 * sw + 2) * bpp],
                      s[(2 * sw + 3) * bpp], wx);
            if(iy < dh - 1)
            {
               d = icerp(s[(3 * sw + 0) * bpp],
                         s[(3 * sw + 1) * bpp],
                         s[(3 * sw + 2) * bpp],
                         s[(3 * sw + 3) * bpp], wx);
            }
            else
               d = c;
            
            v = icerp(a, b, c, d, wy);
            if(v < 0) v = 0;
            if(v > 255) v = 255;
            dst[(y * dstride) + (x * bpp) + n] = v;
            ++s;
         }
      }
   }
}

