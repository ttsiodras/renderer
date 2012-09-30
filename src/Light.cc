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
#include <list>
#include <fstream>

#ifdef USE_OPENMP
#include <omp.h>
#endif

#ifdef USE_TBB
#include "tbb/blocked_range.h"
#include "tbb/parallel_for.h"
#endif

#include <assert.h>

#include "3d.h"
#include "Algebra.h"
#include "ScanConverter.h"

// If you want to look at the shadowbuffer (curious, are we? :-)
// just uncomment DUMP_SHADOWFILE below, recompile, run, and
// a file will be created in your current directory, called
// "shadow". Issue
//    ../src/showShadowMap/showShadowMap
// ..and you'll get a look at your shadow map.
//
//#define DUMP_SHADOWFILE

// To cope with the requirements of TBB, OpenMP and single-threaded
// in one single code block, we need to declare a helper class,
// DrawSceneInShadowBuffer. TBB wants a functor to call for each thread
// spawned from the "tbb::parallel_for" below, so DrawSceneInShadowBuffer
// provides one.
// We use the preprocessor to make this class do the right thing
// for all cases... which should be simple, but it isn't :-)
//
// TBB:
// the operator() will be called, with the range of indexes
// that this thread is to work on. We simply call DrawTriangles
// with this range, and since we are already in the scope of our
// working thread, we allocate scanline buffers on our thread stack
// (i.e. DrawTriangles stack space under TBB is thread-private).
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
// thread-specific storage for our scanline buffers.
// We use OpenMP's "private" to specify this; however, this also
// means that OpenMP's threads get their own copies from "auto-storage"
// (stack) and in an uninitialized state - we therefore need to
// call .resize() on the containers.
//    We could have called resize() before the loop and used
// threadprivate and copyin, but GCC doesn't support these for classes
// (it supports them only for primitives).

class DrawSceneInShadowBuffer {
    const Scene& scene;
    Matrix3& mv;
    Light& light;
public:
    DrawSceneInShadowBuffer(const Scene& s, Matrix3& m, Light& l)
	:
	scene(s),
	mv(m),
	light(l)
    {}
    void DrawTriangles(int iStartingTriangleIndex, int iOnePastEndingTriangleIndex) const
    {
	std::vector<unsigned> lines;
	std::vector<Vector3>  left;
	std::vector<Vector3>  right;
#if defined(USE_OPENMP)
	#pragma omp parallel for private(lines, left, right) schedule(dynamic,TRIANGLES_PER_THREAD)
#endif
	for(int j=iStartingTriangleIndex; j<iOnePastEndingTriangleIndex; j++) {

	    lines.resize(SHADOWMAPSIZE);
	    left.resize(SHADOWMAPSIZE);
	    right.resize(SHADOWMAPSIZE);

	    // Draw this triangle into the shadow buffer
	    const Triangle& triangle = scene._triangles[j];
/*
	    // Only draw triangles that don't face this light (convex objects)
	    Vector3 triToLight = *this;
	    triToLight -= triangle._center;
	    //triToLight.normalize();
	    if (dot(triToLight, triangle._normal)>0)
		continue;
*/
	    // Ok, this triangle is facing this light, go...
	    Vector3 worldPointA = *triangle._vertexA;
	    Vector3 worldPointB = *triangle._vertexB;
	    Vector3 worldPointC = *triangle._vertexC; // so far, object space
	    worldPointA -= light;
	    worldPointB -= light;
	    worldPointC -= light; // now, worldspace - light
	    Vector3 xformedA = mv.multiplyRightWith(worldPointA);
	    Vector3 xformedB = mv.multiplyRightWith(worldPointB);
	    Vector3 xformedC = mv.multiplyRightWith(worldPointC); // now, lightspace

	    // Rasterize the triangle in linear interpolation fashion in the shadow buffer

	    // Project x,y on light's 'screen'
	    xformedA._x = SHADOWMAPSIZE/2 + SHADOWMAPSIZE*2 * xformedA._x/xformedA._z;
	    xformedA._y = SHADOWMAPSIZE/2 + SHADOWMAPSIZE*2 * xformedA._y/xformedA._z;
	    xformedA._z  = 1.0f/xformedA._z;

	    xformedB._x = SHADOWMAPSIZE/2 + SHADOWMAPSIZE*2 * xformedB._x/xformedB._z;
	    xformedB._y = SHADOWMAPSIZE/2 + SHADOWMAPSIZE*2 * xformedB._y/xformedB._z;
	    xformedB._z  = 1.0f/xformedB._z;

	    xformedC._x = SHADOWMAPSIZE/2 + SHADOWMAPSIZE*2 * xformedC._x/xformedC._z;
	    xformedC._y = SHADOWMAPSIZE/2 + SHADOWMAPSIZE*2 * xformedC._y/xformedC._z;
	    xformedC._z  = 1.0f/xformedC._z;

	    if (xformedA._y<0 && xformedB._y<0 && xformedC._y<0) continue;
	    if (xformedA._y>=SHADOWMAPSIZE &&
		xformedB._y>=SHADOWMAPSIZE &&
		xformedC._y>=SHADOWMAPSIZE) continue;

	    light.InterpolateTriangleOnShadowBuffer(xformedA, xformedB, xformedC, &lines[0], &left[0], &right[0]);
	}
    }

#ifdef USE_TBB
    // TBB expects functors, and this one simply delegates to DrawTriangles
    void operator()(const tbb::blocked_range<size_t>& r) const {
	DrawTriangles(r.begin(), r.end());
    }
#endif
};

void Light::CalculatePositionInCameraSpace(const Camera& camera)
{
    // During Phong and PhongShadowmap rendering, we interpolate
    // in camera space. To do the rendering faster, we calculate
    // the light position in camera space here, and use it
    // in the per-pixel calculations (that work in camera space).
    Vector3 cameraToLight = *this;
    cameraToLight -= camera;
    _inCameraSpace = camera._mv.multiplyRightWith(cameraToLight);
}

void Light::CalculateXformFromWorldToLightSpace()
{
    Vector3 lightToWorldCenter(-_x, -_y, -_z);
    lightToWorldCenter.normalize();
    Vector3 zenith(0., 0., 1.);
    Vector3 rightAxis = cross(lightToWorldCenter, zenith);
    rightAxis.normalize();
    Vector3 upAxis = cross(rightAxis, lightToWorldCenter);
    upAxis.normalize();

    // Now that we have the three axes, create the transform matrix
    // for this light, to prepare for the transforms of the scene's
    // triangles. The world coordinates will be xformed into the 
    // light's, located at _x,_y,_z and having the axis we just calc'ed

    Matrix3& mv = _worldToLightSpace;
    mv._row1 = upAxis;			// x-axis
    mv._row2 = rightAxis;		// y-axis
    mv._row3 = lightToWorldCenter;	// z-axis
}

void Light::CalculateXformFromCameraToLightSpace(const Camera& eye)
{
    // During Phong and PhongShadowmap rendering, we interpolate
    // in camera space. To do the rendering faster, we calculate
    // a Matrix3 that allows us to go in one step from camera space
    // (i.e. the [_x/_z, _y/_z, 1/_z] contents of the FatPoints)
    // to light space - and thus, easily check for Shadowmap Z values
    // in the per-pixel calculations (that work in camera space).
    Vector3 lightToWorldCenter(-_x, -_y, -_z);
    lightToWorldCenter.normalize();
    Vector3 zenith(0., 0., 1.);
    Vector3 rightAxis = cross(lightToWorldCenter, zenith);
    rightAxis.normalize();
    Vector3 upAxis = cross(rightAxis, lightToWorldCenter);
    upAxis.normalize();

    // OK, we calculated the 3 light axis in worldspace.
    // We need them in camera space, so...
    Matrix3& mv = _cameraToLightSpace;
    mv._row1 = eye._mv.multiplyRightWith(upAxis);
    mv._row2 = eye._mv.multiplyRightWith(rightAxis);
    mv._row3 = eye._mv.multiplyRightWith(lightToWorldCenter);
}

void Light::RenderSceneIntoShadowBuffer(const Scene& scene)
{
    CalculateXformFromWorldToLightSpace();

#ifdef USE_TBB
    // For TBB, use the parallel_for construct.
    // Different threads will execute for segments of the triangles' vector,
    // calling the operator(), which in turn calls DrawTriangles for the vector's segment.
    // We use the third parameter of parallel_for to chop the triangle list down
    // into batches of 100 triangles - the dynamic scheduler will feed these batches
    // to our threads, keeping them busy (just like schedule(dynamic,100) for OpenMP)
    tbb::parallel_for(
	tbb::blocked_range<size_t>(0, scene._triangles.size(), 100),
	DrawSceneInShadowBuffer(scene, _worldToLightSpace, *this) );
#else
    // For both OpenMP and single-threaded, call the DrawTriangles member
    // of the DrawSceneInShadowBuffer, requesting drawing of ALL triangles.
    // For OpenMP, the appropriate pragma inside DrawTriangles will make it execute via SMP...
    DrawSceneInShadowBuffer(scene, _worldToLightSpace, *this).DrawTriangles(0, scene._triangles.size());
#endif

#ifdef DUMP_SHADOWFILE
    fstream test("shadow", ios::binary | ios::out);
    for(unsigned i=0; i<SHADOWMAPSIZE; i++)
    	test.write(reinterpret_cast<char *>(&_shadowBuffer[i][0]), SHADOWMAPSIZE*sizeof(coord));
#endif
}

class AccessShadowPixelX {
    const Vector3& _v;
public:
    AccessShadowPixelX(const Vector3& v):_v(v) {}
    coord operator()() const { return _v._x; }
};

inline void Light::PlotShadowPixel(int y, const Vector3& v)
{
    int idx = (int) v._x;
    if (idx>=0 && idx<(int)SHADOWMAPSIZE)
	if (_shadowBuffer[y][idx] < v._z)
	    _shadowBuffer[y][idx] = v._z;
}

void Light::InterpolateTriangleOnShadowBuffer(
    const Vector3& v1, const Vector3& v2, const Vector3& v3,
    unsigned *lines,
    Vector3 *left,
    Vector3 *right)
{
    ScanConverter<Vector3,AccessShadowPixelX,SHADOWMAPSIZE>
	scanner(lines, left, right);

    scanner.ScanConvert(int(v1._y), v1, int(v2._y), v2);
    scanner.ScanConvert(int(v2._y), v2, int(v3._y), v3);
    scanner.ScanConvert(int(v1._y), v1, int(v3._y), v3);

    // Time for linear interpolation
    for(int y=scanner._minimum; y<=scanner._maximum; y++) {
	if (lines[y] == 1) {
	    PlotShadowPixel(y, left[y]);
	} else {
	    int x1 = (int) left[y]._x;
	    int x2 = (int) right[y]._x;
	    int steps = abs(x2-x1);
	    if (!steps) {
		PlotShadowPixel(y, left[y]);
		PlotShadowPixel(y, right[y]);
	    } else {
		Vector3 start = left[y];
		Vector3 dLR = right[y]; dLR -= start; dLR /= (coord)steps;
		PlotShadowPixel(y, start);
		while(steps--) {
		    start += dLR;
		    PlotShadowPixel(y, start);
		}
	    }
	}
    }
}

