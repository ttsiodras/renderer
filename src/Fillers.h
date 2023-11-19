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

#ifndef __FILLERS_H__
#define __FILLERS_H__

#include "Screen.h"
#include "LightingEq.h"

// Contained here: the structures interpolated by the rasterizer,
// and the Filler functions that know how to populate them with the
// "setup" data per triangle.
//

// Helper macros used during definitions of FatPointTYPES' operators, to avoid duplication.

// We need two constructs when describing operations on fields:
// ACT1 is   _x += rhs._x
#define ACT1(field, action)    field action ## = rhs. field;
// ACT2 is   _x *= rhs
#define ACT2(field, action)    field action ## = rhs;

//
// Common macro for all Interpolation operators ( += , -= , *= , /= )
// X_MEMBERS is defined below ( see X-Macros: http://stackoverflow.com/questions/264269 )
//
#define OPERATOR(T1,action,T2) \
    inline T1& operator action ## = (const T2& rhs) { X_MEMBERS ; return *this; }

//
// Ambient-Occlusion-Only
//

#define X_MEMBERS						    \
    /* The screen-space X coordinate */				    \
    X(coord,_projx)						    \
    /* The camera space Z coordinate */				    \
    X(coord,_z)							    \
    /* The triangle color scaled by the ambient occlusion factor */ \
    X(Pixel,_color)

#define T FatPointAmbient
struct T {
    // Member declarations
#define X(type,name) type name;
    X_MEMBERS
#undef X

    // Operator declarations (i.e '+=' on all fields, '-=' on all fields, etc)
#define X(type,name) ACT1(name,+)
    OPERATOR(T,+,T)
#undef X
#define X(type,name) ACT1(name,-)
    OPERATOR(T,-,T)
#undef X
#define X(type,name) ACT2(name,*)
    OPERATOR(T,*,coord)
#undef X
#define X(type,name) ACT2(name,/)
    OPERATOR(T,/,coord)
#undef X
};

//
// Gouraud
//

struct FatPointGouraud : FatPointAmbient {
    // Same contents as the Ambient Fat point, but the _projx, _z, and _color mean:
    //
    // The screen-space X coordinate
    // The camera space Z coordinate
    // The interpolated Gouraud color (calculated in the per-vertex setup, in the Gouraud Filler() below)
};

//
// Phong, ZBuffer
//

#undef X_MEMBERS
#define X_MEMBERS						    \
    /* The screen-space X coordinate */				    \
    X(coord,_projx)						    \
    /* The camera space coordinates */				    \
    X(coord,_x)							    \
    X(coord,_y)							    \
    X(coord,_z)							    \
    /* The (hopefully available from shadevis) ambient occlusion factor */ \
    X(coord,_ambientOcclusionCoeff)				    \
    /* The normal vector will also be interpolated (in camera space, */ \
    /* so it is transformed in the FillerPhong) */ \
    X(Vector3,_normal)

#undef T
#define T FatPointPhong
struct T {
    // Member declarations
#define X(type,name) type name;
    X_MEMBERS
#undef X

    // Operator declarations (i.e '+=' on all fields, '-=' on all fields, etc)
#define X(type,name) ACT1(name,+)
    OPERATOR(T,+,T)
#undef X
#define X(type,name) ACT1(name,-)
    OPERATOR(T,-,T)
#undef X
#define X(type,name) ACT2(name,*)
    OPERATOR(T,*,coord)
#undef X
#define X(type,name) ACT2(name,/)
    OPERATOR(T,/,coord)
#undef X
    // Conversion operator, to get to the interpolated camera-space coordinates
    inline operator Vector3() const { return Vector3(_x,_y,_z); }
};

// Identical interpolations with FatPointPhong; difference is in LightingEquation used
struct FatPointPhongAndShadowed : FatPointPhong {};

// Identical interpolations with FatPointPhong; difference is in LightingEquation used
struct FatPointPhongAndSoftShadowed : FatPointPhongAndShadowed {
};

// The Filler functions 'setup' the interpolation per triangle;
// the signature must therefore be able to convey all the information
// required by all rendering modes. We don't want to waste CPU cycles
// transfering argument data that we don't need, so a TriangleCarrier
// type is defined per rendering mode, that only passes the information
// necessary.
//
// Before defining it, though, we want to only carry the triangle color
// for the Phong modes - not the Ambient and Gouraud ones.

template<class T> struct CarryTriangleColor { Pixel color; };
template<> struct CarryTriangleColor<FatPointAmbient> {};
template<> struct CarryTriangleColor<FatPointGouraud> {};

// Now that we have CarryTriangleColor, we can define TriangleCarrier:
template <typename InterpolatedType>
struct TriangleCarrier : public CarryTriangleColor<InterpolatedType> {
    // screen-space coordinates of the 3 vertices
    int ay; int by; int cy;
    // for each vertex, the data we will interpolate per pixel
    InterpolatedType xformedA, xformedB, xformedC;
    // Due to inheritance, the Phong types will be also carrying: "Pixel color"
};

template <typename TriangleCarrier>
void Filler(
    const Scene&,
    const coord&, const coord&, const coord&, const coord&, const coord&, const coord&,
    const Vector3&, const Vector3&, const Vector3&,
    const Triangle&, const Camera&, TriangleCarrier&);

// In Ambient-only mode, simply scale the triangle's color by the ambient occlusion factor
// And then interpolate this color per-pixel

template<>
void inline Filler(
    const Scene& /*scene*/,
    const coord& ax, const coord& ay, const coord& bx, const coord& by, const coord& cx, const coord& cy,
    const Vector3& inCameraSpaceA, const Vector3& inCameraSpaceB, const Vector3& inCameraSpaceC,
    const Triangle& triangle, const Camera&, TriangleCarrier<FatPointAmbient>& tri)
{
#define COMMON_VERTEX(l, L)										\
    tri. l ## y = (int) l ## y;										\
    tri.xformed ## L ._projx = (coord) l ## x;								\
    tri.xformed ## L ._z = 1.0f/inCameraSpace ## L ._z;

#define AMBIENT_VERTEX(l, L)										\
    COMMON_VERTEX(l, L)											\
    tri.xformed ## L ._color = triangle._colorf;							\
    tri.xformed ## L ._color *= triangle._vertex ## L ->_ambientOcclusionCoeff/255.f;

    AMBIENT_VERTEX(a, A)
    AMBIENT_VERTEX(b, B)
    AMBIENT_VERTEX(c, C)
}

// In Gouraud, calculate the color per vertex (using the LightingEquation)
// The color will then be interpolated per-pixel

template<>
void inline Filler(
    const Scene& scene,
    const coord& ax, const coord& ay, const coord& bx, const coord& by, const coord& cx, const coord& cy,
    const Vector3& inCameraSpaceA, const Vector3& inCameraSpaceB, const Vector3& inCameraSpaceC,
    const Triangle& triangle, const Camera& camera, TriangleCarrier<FatPointGouraud>& tri)
{
    // No shadow checking
    LightingEquation<NoShadows> compute(scene);

#define GOURAUD_VERTEX(l, L)											\
    COMMON_VERTEX(l, L)												\
    tri.xformed ## L._color = Pixel();										\
    Vector3 normal ## L = camera._mv.multiplyRightWith( triangle._vertex ## L->_normal);			\
    compute.ComputePixel(											\
	inCameraSpace ## L, normal ## L, triangle._colorf, (coord)triangle._vertex ## L->_ambientOcclusionCoeff,	\
	tri.xformed ## L._color);

    GOURAUD_VERTEX(a, A)
    GOURAUD_VERTEX(b, B)
    GOURAUD_VERTEX(c, C)
}

// In phong, we interpolate the normal and do LightingEquation per-pixel.
// All 3 kinds of Phong (Phong, PhongShadowed and PhongSoftShadowed) have exactly the same
// setup - their differ in their Plot<FatPointPhong...> logic.
//
// The common Phong setup code is placed in this template function
// (it has to work with all TriangleCarrier<Phong...> types, so we need a template)

template <class TriangleCarrier>
void PhongSetup(TriangleCarrier& tri, const Triangle& triangle, const Camera& eye,
    const coord& ax, const coord& ay, const coord& bx, const coord& by, const coord& cx, const coord& cy,
    const Vector3& inCameraSpaceA, const Vector3& inCameraSpaceB, const Vector3& inCameraSpaceC)
{
#define PHONG_VERTEX(l, L)								    \
    COMMON_VERTEX(l, L)									    \
    tri.xformed ## L._x = inCameraSpace ## L._x/inCameraSpace ## L._z;			    \
    tri.xformed ## L._y = inCameraSpace ## L._y/inCameraSpace ## L._z;			    \
    tri.xformed ## L._ambientOcclusionCoeff = (coord) triangle._vertex ## L->_ambientOcclusionCoeff;

#define PHONG_VERTEX_2(l, L) \
    tri.xformed ## L._normal = eye._mv.multiplyRightWith( triangle._vertex ## L->_normal );

    PHONG_VERTEX(a, A)
    PHONG_VERTEX(b, B)
    PHONG_VERTEX(c, C)

    // Better cache-coherency: the "eye._mv" Matrix stays in L1-cache for setup of all 3 vertices
    PHONG_VERTEX_2(a, A)
    PHONG_VERTEX_2(b, B)
    PHONG_VERTEX_2(c, C)

    tri.color = triangle._colorf; // Store this in the TriangleCarrier, so we can use it in LightingEquation
}

//
// Phong, ZBuffer
//

template<>
void inline Filler(
    const Scene& /*scene*/,
    const coord& ax, const coord& ay, const coord& bx, const coord& by, const coord& cx, const coord& cy,
    const Vector3& inCameraSpaceA, const Vector3& inCameraSpaceB, const Vector3& inCameraSpaceC,
    const Triangle& triangle, const Camera& eye, TriangleCarrier<FatPointPhong>& tri)
{
    // Common setup for all 3 Phong modes (Phong, PhongShadowed, PhongSoftShadowed)
    PhongSetup(tri,triangle,eye,ax,ay,bx,by,cx,cy,inCameraSpaceA,inCameraSpaceB,inCameraSpaceC);
}

//
// Phong, ZBuffer, ShadowMaps
//

template<>
void inline Filler(
    const Scene& /*scene*/,
    const coord& ax, const coord& ay, const coord& bx, const coord& by, const coord& cx, const coord& cy,
    const Vector3& inCameraSpaceA, const Vector3& inCameraSpaceB, const Vector3& inCameraSpaceC,
    const Triangle& triangle, const Camera& eye, TriangleCarrier<FatPointPhongAndShadowed>& tri)
{
    // Same setup data as Phong (the difference is in how Plot<FatPointPhongAndShadowed> works)
    PhongSetup(tri,triangle,eye,ax,ay,bx,by,cx,cy,inCameraSpaceA,inCameraSpaceB,inCameraSpaceC);
}

//
// Phong, ZBuffer, Soft ShadowMaps
//

template<>
void inline Filler(
    const Scene& /*scene*/,
    const coord& ax, const coord& ay, const coord& bx, const coord& by, const coord& cx, const coord& cy,
    const Vector3& inCameraSpaceA, const Vector3& inCameraSpaceB, const Vector3& inCameraSpaceC,
    const Triangle& triangle, const Camera& eye, TriangleCarrier<FatPointPhongAndSoftShadowed>& tri)
{
    // Same setup data as Phong (the difference is in how Plot<FatPointPhongAndSoftShadowed> works)
    PhongSetup(tri,triangle,eye,ax,ay,bx,by,cx,cy,inCameraSpaceA,inCameraSpaceB,inCameraSpaceC);
}

#endif
