/*
	normalmap GIMP plugin

	Copyright (C) 2002 Shawn Kirst <skirst@fuse.net>

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

#ifndef __QUAD_H
#define __QUAD_H

#define QUAD_NUM_VERTS 8
static float quad_verts[QUAD_NUM_VERTS * 16] =
{
   -1,  1,  0.001f, 1, 0, 0, 1, 0, 0, 0, -1, 0, 0, 0,  1, 1,
   -1, -1,  0.001f, 1, 0, 1, 1, 0, 0, 0, -1, 0, 0, 0,  1, 1,
    1,  1,  0.001f, 1, 1, 0, 1, 0, 0, 0, -1, 0, 0, 0,  1, 1,
    1, -1,  0.001f, 1, 1, 1, 1, 0, 0, 0, -1, 0, 0, 0,  1, 1,
   -1,  1, -0.001f, 1, 0, 0, 1, 0, 0, 0, -1, 0, 0, 0, -1, 1,
   -1, -1, -0.001f, 1, 0, 1, 1, 0, 0, 0, -1, 0, 0, 0, -1, 1,
    1,  1, -0.001f, 1, 1, 0, 1, 0, 0, 0, -1, 0, 0, 0, -1, 1,
    1, -1, -0.001f, 1, 1, 1, 1, 0, 0, 0, -1, 0, 0, 0, -1, 1,
};

#define QUAD_NUM_INDICES 12
static unsigned short quad_indices[QUAD_NUM_INDICES] =
{
   0, 1, 2, 2, 1, 3, 4, 5, 6, 6, 5, 7
};

#endif
