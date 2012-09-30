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

#ifdef USE_TBB
#include "tbb/blocked_range.h"
#include "tbb/parallel_for.h"
#endif

#ifdef USE_OPENMP
#include <omp.h>
#endif

#include "3d.h"
#include "Screen.h"
#include "Wu.h"

// Clip distance for the triangles (they have a point closer than this, they dont get drawn)
const coord ClipPlaneDistance = 0.2f;

////////////////////////////////////////
// Rendering function for RENDER_POINTS
////////////////////////////////////////

// After transformation in camera space, project and plot (used for point rendering)
void inline ProjectAndPlot(const Vector3& xformed, Uint32 color, Screen& canvas)
{
    if (xformed._z>ClipPlaneDistance) {
	int x = (int)(WIDTH/2  + SCREEN_DIST * xformed._y/xformed._z);
	int y = (int)(HEIGHT/2 - SCREEN_DIST * xformed._x/xformed._z);
	if (y>=0 && y<(int)HEIGHT && x>=0 && x<(int)WIDTH)
	    canvas.DrawPixel(y,x,color);
    }
}

void Scene::renderPoints(const Camera& eye, Screen& canvas, bool asTriangles)
{
    canvas.ClearScreen();
    Uint32 whitePixel = SDL_MapRGB(canvas._surface->format, 255,255,255);

    if (!asTriangles) {
	// Simple projection and ploting of a white point per vertex
#ifdef USE_OPENMP
	#pragma omp parallel for
#endif
	// Plot all the vertices of object i on the canvas
	for(int j=0; j<(int)_vertices.size(); j++) {
	//for(vector<Vertex>::iterator it=_objects[i]->_vertices.begin(); it!=_objects[i]->_vertices.end(); ++it) {

	    // Plot projected coordinates (on screen)
	    ProjectAndPlot(
		Transform(_vertices[j], eye, eye._mv),
		whitePixel,
		canvas);
	}
    } else {
	// Perform culling, projection and use the triangle color
#ifdef USE_OPENMP
	#pragma omp parallel for
#endif
	for(int j=0; j<(int)_triangles.size(); j++) {

	    // First check if the triangle is visible from where we stand
	    // (closed objects only)
	    Vector3 triToEye = eye;
	    triToEye -= _triangles[j]._center;
	    //triToEye -= _objects[i]->_center;
	    // Normally we would normalize, but since we just need the sign
	    // of the dot product (to determine if it facing us or not)...
	    //triToEye.normalize();
	    if (dot(triToEye, _triangles[j]._normal)<0)
		continue;

	    // For each of the 3 vertices of triangle j of object i,
	    // transform to camera space, project and plot them
	    ProjectAndPlot(
		Transform(*_triangles[j]._vertexA, eye, eye._mv),
		_triangles[j]._color,
		canvas);
	    ProjectAndPlot(
		Transform(*_triangles[j]._vertexB, eye, eye._mv),
		_triangles[j]._color,
		canvas);
	    ProjectAndPlot(
		Transform(*_triangles[j]._vertexC, eye, eye._mv),
		_triangles[j]._color,
		canvas);
	}
    }
    canvas.ShowScreen();
}

////////////////////////////////////////
// Rendering function for RENDER_LINES
////////////////////////////////////////

void Scene::renderWireframe(const Camera& eye, Screen& canvas)
{
    canvas.ClearScreen();

    Uint32 greyPixel = SDL_MapRGB(canvas._surface->format, 200,200,200);
    // Or maybe use... _triangles[j]._color

    // Perform culling, projection and use the triangle color
#ifdef USE_OPENMP
    #pragma omp parallel for
#endif
    for(int j=0; j<(int)_triangles.size(); j++) {

	// First check if the triangle is visible from where we stand
	// (closed objects only)
	Vector3 triToEye = eye;
	triToEye -= _triangles[j]._center;
	// Normally we would normalize, but since we just need the sign
	// of the dot product (to determine if it facing us or not)...
	//triToEye.normalize();
	if (dot(triToEye, _triangles[j]._normal)<0)
	    continue;

	// For each of the 3 vertices of triangle j of object i,
	// transform to camera space
	Vector3 xformedA = Transform(*_triangles[j]._vertexA, eye, eye._mv);
	Vector3 xformedB = Transform(*_triangles[j]._vertexB, eye, eye._mv);
	Vector3 xformedC = Transform(*_triangles[j]._vertexC, eye, eye._mv);

#define SCREENSPACE(xformed, xx, yy)					    \
	    xx = int(WIDTH/2  + SCREEN_DIST * xformed._y/xformed._z);	    \
	    yy = int(HEIGHT/2 - SCREEN_DIST * xformed._x/xformed._z);	    \

	bool agood = xformedA._z > ClipPlaneDistance;
	bool bgood = xformedB._z > ClipPlaneDistance;
	bool cgood = xformedC._z > ClipPlaneDistance;

	if (agood) {
	    int ax, ay;
	    SCREENSPACE(xformedA, ax,ay)
	    if (bgood) {
		int bx,by;
		SCREENSPACE(xformedB, bx,by)
		my_aalineColor(canvas._surface, ax, ay, bx, by, greyPixel);
		if (cgood) {
		    int cx,cy;
		    SCREENSPACE(xformedC, cx,cy)
		    my_aalineColor(canvas._surface, ax, ay, cx, cy, greyPixel);
		    my_aalineColor(canvas._surface, bx, by, cx, cy, greyPixel);
		}
	    } else {
		if (cgood) {
		    int cx,cy;
		    SCREENSPACE(xformedC, cx,cy)
		    my_aalineColor(canvas._surface, ax, ay, cx, cy, greyPixel);
		}
	    }
	} else if (bgood && cgood) {
	    int bx,by;
	    int cx,cy;
	    SCREENSPACE(xformedB, bx,by)
	    SCREENSPACE(xformedC, cx,cy)
	    my_aalineColor(canvas._surface, bx, by, cx, cy, greyPixel);
	}
    }
    canvas.ShowScreen();
}

///////////////////////////////////////////////
// Common (templated) rendering code for:
//    RENDER_AMBIENT
//    RENDER_GOURAUD
//    RENDER_PHONG
//    RENDER_PHONG_SHADOWMAPS
//    RENDER_PHONG_SOFTSHADOWMAPS
///////////////////////////////////////////////

// To cope with the requirements of TBB, OpenMP and single-threaded
// in one single code block, we need to declare a helper class,
// RasterizeScene. TBB wants a functor to call for each thread spawned
// from the "tbb::parallel_for" below, so RasterizeScene provides one.
// We use the preprocessor to make this class do the right thing
// for all cases... which should be simple, but it isn't :-)
//
// TBB:
// the operator() will be called, with the range of indexes
// that this thread is to work on. We simply call DrawTriangles
// with this range, and since we are already in the scope of our
// working thread, we allocate scanline buffers (and a TriangleCarrier)
// on our thread stack (i.e. DrawTriangles stack space under TBB
// is thread-private).
//
// OpenMP and SingleThreaded:
// For these, we directly call DrawTriangles(0, totalTriangles).
// This means that when DrawTriangles runs, we are not in thread-scope (yet).
// For SingleThreaded, we just declare stack-based containers
// (we could have used "static", but the cost is low anyway).
//
// For OpenMP:
// We use the pragma omp parallel to use many threads working on
// the triangle drawing loop. Since thread scope starts AFTER
// the "for" (and not before, as is the case for TBB), we need
// thread-specific storage for our scanline buffers (and a TriangleCarrier).
// We use OpenMP's "private" to specify this; however, this also
// means that OpenMP's threads get their own copies from "auto-storage"
// (stack) and in an uninitialized state - we therefore need to
// call .resize() on the containers (TriangleCarrier is assigned later
// on and needs no initialization).
//    We could have called resize() before the loop and used
// threadprivate and copyin, but GCC doesn't support these for classes
// (it supports them only for primitives).

template <typename InterpolatedType>
class RasterizeScene {
    const Scene& scene;
    const Camera& eye;
    Screen& canvas;
public:
    RasterizeScene(const Scene& scene, const Camera& e, Screen& c)
	:
	scene(scene),
	eye(e),
	canvas(c)
    {}

    void DrawTriangles(int iStartingTriangleIndex, int iOnePastEndingTriangleIndex) const
    {
	std::vector<unsigned> lines;
	std::vector<InterpolatedType> left;
	std::vector<InterpolatedType> right;
	TriangleCarrier<InterpolatedType> triInfoForFillerToFill;

#ifdef USE_OPENMP
	#pragma omp parallel for private(lines, left, right, triInfoForFillerToFill) schedule(dynamic,TRIANGLES_PER_THREAD)
#endif
	for(int j=iStartingTriangleIndex; j<iOnePastEndingTriangleIndex; j++) {

	    lines.resize(HEIGHT);
	    left.resize(HEIGHT);
	    right.resize(HEIGHT);

	    // Draw triangle j on the canvas and the Zbuffer
	    const Triangle& triangle = scene._triangles[j];

	    // First check if the triangle is visible from where we stand
	    // (we only work with closed objects)
	    if (!triangle._twoSided) {
		Vector3 triToEye = eye;
		triToEye -= triangle._center;
		// Normally we would normalize, but since we just need the sign
		// of the dot product (to determine if it facing us or not)...
		//triToEye.normalize();
		if (dot(triToEye, triangle._normal)<0)
		    continue;
	    }

	    // Triangle is visible, transform its vertices into camera space
	    Vector3 inCameraSpaceA = Transform(*triangle._vertexA, eye, eye._mv);
	    if (inCameraSpaceA._z<ClipPlaneDistance) continue;

	    Vector3 inCameraSpaceB = Transform(*triangle._vertexB, eye, eye._mv);
	    if (inCameraSpaceB._z<ClipPlaneDistance) continue;

	    Vector3 inCameraSpaceC = Transform(*triangle._vertexC, eye, eye._mv);
	    if (inCameraSpaceC._z<ClipPlaneDistance) continue;

	    // Calculate projected coordinates (on screen)
	    // and prepare for linear interpolation (x/z, y/z, 1/z)

	    coord ax,ay, bx,by, cx,cy;

	    ay = HEIGHT/2 - SCREEN_DIST * inCameraSpaceA._x/inCameraSpaceA._z;
	    by = HEIGHT/2 - SCREEN_DIST * inCameraSpaceB._x/inCameraSpaceB._z;
	    cy = HEIGHT/2 - SCREEN_DIST * inCameraSpaceC._x/inCameraSpaceC._z;
	    if (ay<0 && by<0 && cy<0) continue;
	    if (ay>=HEIGHT && by>=HEIGHT && cy>=HEIGHT) continue;
	    ax = WIDTH/2  + SCREEN_DIST * inCameraSpaceA._y/inCameraSpaceA._z;
	    bx = WIDTH/2  + SCREEN_DIST * inCameraSpaceB._y/inCameraSpaceB._z;
	    cx = WIDTH/2  + SCREEN_DIST * inCameraSpaceC._y/inCameraSpaceC._z;

	    // Prepare the values to interpolate per pixel (mode-dependent)
	    Filler(
		scene,
		ax,ay, bx,by, cx,cy,
		inCameraSpaceA, inCameraSpaceB, inCameraSpaceC,
		triangle,
		eye,
		triInfoForFillerToFill);

	    // And rasterize the triangle, interpolating per-pixel... (mode-dependent)
	    canvas.RasterizeTriangle(
		triInfoForFillerToFill, eye, &lines[0], &left[0], &right[0]);
	}
    }

#ifdef USE_TBB
    // TBB expects functors, and this one simply delegates to DrawTriangles
    void operator()(const tbb::blocked_range<size_t>& r) const {
	DrawTriangles(r.begin(), r.end());
    }
#endif
};

template <typename InterpolatedType>
void RenderInParallel(
    Scene& scene,
    const Camera& eye,
    Screen& canvas)
{
    canvas.ClearScreen();
    canvas.ClearZbuffer();

/* Done in the main loop, only when the user moves the light - Huge savings...

    for(unsigned i=0; i<_lights.size(); i++) {
	_lights[i]->ClearShadowBuffer();
	_lights[i]->RenderSceneIntoShadowBuffer(*this);
    }
*/

#ifdef USE_TBB
    // For TBB, use the parallel_for construct.
    // Different threads will execute for segments of the triangles' vector,
    // calling the operator(), which in turn calls DrawTriangles for the vector's segment.
    // We use the third parameter of parallel_for to chop the triangle list down
    // into batches of TRIANGLES_PER_THREAD triangles - the dynamic scheduler will feed
    // these batches to our threads, keeping them busy (just like schedule(dynamic,N) does for OpenMP)
    tbb::parallel_for(
	tbb::blocked_range<size_t>(0, scene._triangles.size(), TRIANGLES_PER_THREAD),
	RasterizeScene<InterpolatedType>(scene, eye, canvas) );
#else
    // For both OpenMP and single-threaded, call the DrawTriangles member
    // of the RasterizeScene, requesting drawing of ALL triangles.
    // For OpenMP, the appropriate pragma inside DrawTriangles will make it execute via SMP...
    RasterizeScene<InterpolatedType>(
	scene, eye, canvas).
	    DrawTriangles(0, scene._triangles.size());
#endif
    canvas.ShowScreen();
}

#include "Fillers.h"

void Scene::renderAmbient(const Camera& eye, Screen& canvas)
{
    RenderInParallel<FatPointAmbient>(*this, eye, canvas);
}

void Scene::renderGouraud(const Camera& eye, Screen& canvas)
{
    RenderInParallel<FatPointGouraud>( *this, eye, canvas);
}

void Scene::renderPhong(const Camera& eye, Screen& canvas)
{
    RenderInParallel<FatPointPhong>( *this, eye, canvas);
}

void Scene::renderPhongAndShadowed(const Camera& eye, Screen& canvas)
{
    RenderInParallel<FatPointPhongAndShadowed>( *this, eye, canvas);
}

void Scene::renderPhongAndSoftShadowed(const Camera& eye, Screen& canvas)
{
    RenderInParallel<FatPointPhongAndSoftShadowed>( *this, eye, canvas);
}
