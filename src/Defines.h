/*
 *  renderer - A simple implementation of polygon-based 3D algorithms.
 *  Copyright (C) 2004  Thanassis Tsiodras (ttsiodras@gmail.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __defines_h__
#define __defines_h__

#define TRI_MAGIC	0xDEADBEEF
#define TRI_MAGICNORMAL 0xDEADC0DE
#define SHADOWMAPSIZE	1024
#define WIDTH		800
#define HEIGHT		600
#define SCREEN_DIST	(HEIGHT*2)

#define AMBIENT		96.f
#define DIFFUSE		128.f
#define SPECULAR	192.f

// Maximum allowed depth of BVH
// Checked at BVH build time, no runtime crash possible, see CreateCFBVH()
#define BVH_STACK_SIZE 32

#define TRIANGLES_PER_THREAD  50

#define ASSERT_OR_DIE(x) do {                \
    if (!(x)) {                              \
        fprintf(stderr, "Internal error\n"); \
        exit(1);                             \
    }                                        \
} while(0)

#endif
