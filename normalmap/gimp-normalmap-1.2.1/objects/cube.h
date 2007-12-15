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

#ifndef __CUBE_H
#define __CUBE_H

#define CUBE_NUM_VERTS 24
static float cube_verts[CUBE_NUM_VERTS * 16] =
{
   -1,  1,  1, 1, 0, 0, 1, 0, 0, 0, -1, 0,  0,  0,  1, 1,
   -1, -1,  1, 1, 0, 1, 1, 0, 0, 0, -1, 0,  0,  0,  1, 1,
    1,  1,  1, 1, 1, 0, 1, 0, 0, 0, -1, 0,  0,  0,  1, 1,
    1, -1,  1, 1, 1, 1, 1, 0, 0, 0, -1, 0,  0,  0,  1, 1,
   -1,  1, -1, 1, 0, 0, 1, 0, 0, 0, -1, 0,  0,  0, -1, 1,
   -1, -1, -1, 1, 0, 1, 1, 0, 0, 0, -1, 0,  0,  0, -1, 1,
    1,  1, -1, 1, 1, 0, 1, 0, 0, 0, -1, 0,  0,  0, -1, 1,
    1, -1, -1, 1, 1, 1, 1, 0, 0, 0, -1, 0,  0,  0, -1, 1,
   -1,  1, -1, 1, 0, 0, 0, 0, 1, 0, -1, 0, -1,  0,  0, 1,
   -1, -1, -1, 1, 0, 1, 0, 0, 1, 0, -1, 0, -1,  0,  0, 1,
   -1,  1,  1, 1, 1, 0, 0, 0, 1, 0, -1, 0, -1,  0,  0, 1,
   -1, -1,  1, 1, 1, 1, 0, 0, 1, 0, -1, 0, -1,  0,  0, 1,
    1,  1, -1, 1, 0, 0, 0, 0, 1, 0, -1, 0,  1,  0,  0, 1,
    1, -1, -1, 1, 0, 1, 0, 0, 1, 0, -1, 0,  1,  0,  0, 1,
    1,  1,  1, 1, 1, 0, 0, 0, 1, 0, -1, 0,  1,  0,  0, 1,
    1, -1,  1, 1, 1, 1, 0, 0, 1, 0, -1, 0,  1,  0,  0, 1,
   -1,  1, -1, 1, 0, 0, 1, 0, 0, 0,  0, 1,  0,  1,  0, 1,
   -1,  1,  1, 1, 0, 1, 1, 0, 0, 0,  0, 1,  0,  1,  0, 1,
    1,  1, -1, 1, 1, 0, 1, 0, 0, 0,  0, 1,  0,  1,  0, 1,
    1,  1,  1, 1, 1, 1, 1, 0, 0, 0,  0, 1,  0,  1,  0, 1,
   -1, -1, -1, 1, 0, 0, 1, 0, 0, 0,  0, 1,  0, -1,  0, 1,
   -1, -1,  1, 1, 0, 1, 1, 0, 0, 0,  0, 1,  0, -1,  0, 1,
    1, -1, -1, 1, 1, 0, 1, 0, 0, 0,  0, 1,  0, -1,  0, 1,
    1, -1,  1, 1, 1, 1, 1, 0, 0, 0,  0, 1,  0, -1,  0, 1,
};

#define CUBE_NUM_INDICES 36
static unsigned short cube_indices[CUBE_NUM_INDICES] =
{
   0,  1,  2,  2,  1,  3,  4,  5,  6,  6,  5,  7,
   8,  9,  10, 10, 9,  11, 12, 13, 14, 14, 13, 15,
   16, 17, 18, 18, 17, 19, 20, 21, 22, 22, 21, 23     
};

#endif
