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

#ifndef __light_h__
#define __light_h__

#include <limits>

#include "Algebra.h"
#include <list>

struct Scene;
struct Camera;
struct Triangle;

struct Light : public Vector3
{
    Matrix3 _worldToLightSpace;
    Matrix3 _cameraToLightSpace;
    Vector3 _inCameraSpace;

    // Shadow buffer, used in modes 7 and 8
    coord _shadowBuffer[SHADOWMAPSIZE][SHADOWMAPSIZE];

    Light(coord x, coord y, coord z)
	:
	Vector3(x,y,z)
    {
	ClearShadowBuffer();
    }

    void ClearShadowBuffer()
    {
	// For both float and double, a "body" of 0xFE..FE is a huge negative number
	memset(reinterpret_cast<void*>(&_shadowBuffer[0][0]), 254, sizeof(_shadowBuffer));
    }

    void PlotShadowPixel(int y, const Vector3& v);
    void InterpolateTriangleOnShadowBuffer(
	const Vector3& v1, const Vector3& v2, const Vector3& v3,
	unsigned *lines,
	Vector3 *left,
	Vector3 *right);

    void RenderSceneIntoShadowBuffer(const Scene&);

    void CalculatePositionInCameraSpace(const Camera& camera);
    void CalculateXformFromCameraToLightSpace(const Camera& eye);
    void CalculateXformFromWorldToLightSpace();
};

#endif
