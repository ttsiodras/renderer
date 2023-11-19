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

#include <config.h>

#include <vector>

#include <typeinfo>

#include "3d.h"
#include "Screen.h"
#include "Fillers.h"
#include "OnlineHelpKeys.h"

SDL_Surface *Screen::_surface = NULL;
Screen::DrawPixelFun Screen::DrawPixel = NULL;

template<>
void Screen::Plot(
    int y, int x, const FatPointAmbient& v, const TriangleCarrier<FatPointAmbient>&, const Camera&)
{
    // Normal ambient lighting: the per-pixel interpolated color is the
    // ambient occlusion factor times the triangle color (FillerAmbient)
    DrawPixel(y,x,SDL_MapRGB(_surface->format,
		(unsigned char)v._color._r,
		(unsigned char)v._color._g,
		(unsigned char)v._color._b));
}

template<>
void Screen::Plot(
    int y, int x, const FatPointGouraud& v, const TriangleCarrier<FatPointGouraud>&, const Camera&)
{
    // Complete lighting equation (ambient + specular + diffuse) done in FillerGouraud.
    // Color is then interpolated per-pixel:
    DrawPixel(y,x,SDL_MapRGB(_surface->format,
		(unsigned char)v._color._r,
		(unsigned char)v._color._g,
		(unsigned char)v._color._b));
}

// Compile-time dictionary<TypeOfPoint,LightingEquationMode>,
// implemented via type-traits (the nested type "ShadowModel")
template <typename InterpolatedType>
struct ModeSpecificLighting;

#define SELECT_SHADOWS(TypeOfPoint, LightingEquationMode) \
template <> \
struct ModeSpecificLighting<TypeOfPoint> { \
    typedef LightingEquation<LightingEquationMode> ShadowModel; \
};

// For FatPointPhong, don't involve shadows at all
SELECT_SHADOWS(FatPointPhong, NoShadows)
// For FatPointPhongAndShadowed, involve shadows, but not soft shadows
SELECT_SHADOWS(FatPointPhongAndShadowed, ShadowMapping)
// For FatPointPhongAndSoftShadowed, involve shadows, AND use soft shadows
SELECT_SHADOWS(FatPointPhongAndSoftShadowed, SoftShadowMapping)

template <typename InterpolatedType, typename TriangleCarrier>
Uint32 IlluminatePixel(
    const InterpolatedType& v, const TriangleCarrier& tri, const Scene& _scene, SDL_Surface *_surface)
{
    // The FatPoint (v) carries the camera space coordinates (_x/_z, _y/_z, 1/_z => x, y, z)
    Vector3 point = v;
    point._x /= point._z;
    point._y /= point._z;
    point._z = 1.0f/point._z; // True camera space
    Vector3 normal = v._normal;
    normal.normalize();
    Pixel color = Pixel(); // Start from complete darkness...
    typename ModeSpecificLighting<InterpolatedType>::ShadowModel compute(_scene);
    compute.ComputePixel( point, normal, tri.color, v._ambientOcclusionCoeff, color);
    return SDL_MapRGB(_surface->format, (Uint8)color._r, (Uint8)color._g, (Uint8)color._b);
}

template<>
void Screen::Plot(
    int y, int x, const FatPointPhong& v, const TriangleCarrier<FatPointPhong>& tri, const Camera& /*camera*/)
{
    DrawPixel(y,x,IlluminatePixel(v,tri,_scene,_surface));
}

template<>
void Screen::Plot(
    int y, int x, const FatPointPhongAndShadowed& v, const TriangleCarrier<FatPointPhongAndShadowed>& tri, const Camera& /*camera*/)
{
    DrawPixel(y,x,IlluminatePixel(v,tri,_scene,_surface));
}

template<>
void Screen::Plot(
    int y, int x, const FatPointPhongAndSoftShadowed& v, const TriangleCarrier<FatPointPhongAndSoftShadowed>& tri, const Camera& /*camera*/)
{
    DrawPixel(y,x,IlluminatePixel(v,tri,_scene,_surface));
}
