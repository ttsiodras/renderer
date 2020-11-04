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

#ifndef __base3d_h__
#define __base3d_h__

#include <vector>

#include "Types.h"

struct Mesh {
    bool _isSelectedViaMouse;
    Mesh(): _isSelectedViaMouse(false) {}
};

struct Vertex : public Vector3
{
    Vector3 _normal;
    unsigned _ambientOcclusionCoeff;

    Vertex(coord x, coord y, coord z, coord nx, coord ny, coord nz, unsigned char amb=60)
	:
	Vector3(x,y,z), _normal(nx, ny, nz), _ambientOcclusionCoeff(amb)
    {
	// assert |nx,ny,nz| = 1
    }
};

struct Triangle
{
    const Vertex *_vertexA, *_vertexB, *_vertexC;
    Vector3 _center;
    Vector3 _normal;

    // Color: 
    Pixel _colorf;
    // precomputed for SDL surface
    Uint32 _color;

    // Should we backface cull this triangle?
    bool _twoSided;

    // Raytracing intersection pre-computed cache:
    coord _d, _d1, _d2, _d3;
    Vector3 _e1, _e2, _e3;
    Vector3 _bottom;
    Vector3 _top;

    // Which mesh do we belong to?
    // (to allow selection via mouse)
    Mesh *_pMesh;

    Triangle(
	const Vertex *vertexA,
	const Vertex *vertexB,
	const Vertex *vertexC,
	unsigned r, unsigned g, unsigned b,
        Mesh *pMesh,
	bool twosided = false, bool triNormalProvided=false, Vector3 triNormal=Vector3(0.,0.,0.) );
};

#endif
