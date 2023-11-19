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

// 
// The following implement a BVH with SAH AABB - that is, a bounding volume hierarchy
// that uses axis-aligned bounding boxes, and also uses the Surface Are Heuristic 
// to determine optimal splitting.
//

#include "config.h"

#include "SDL.h"

#include "BVH.h"
#include "Base3d.h"
#include "Scene.h"
#include "Clock.h"
#include "Defines.h"

#include <algorithm>
#include <vector>
#include <cfloat>
#include <string>
#include <sstream>

#include <assert.h>
#include <stdio.h>

using namespace std;

////////////////////////////////////////////////////////////////
// Periodically display progress report on construction of BVH

#define PROGRESS_REPORT

#define PRINT_REPORT_EVERY  65535

#ifdef PROGRESS_REPORT
#define REPORT(x) x
#define REPORTPRM(x) x,
#else
#define REPORT(x)
#define REPORTPRM(x)
#endif

unsigned g_reportCounter = 0;

#ifndef SIMD_SSE

#define BUILDING_BVH_MSG "Building BVH: "

//
// SSE is not available, using normal C++ constructs
//

//////////////////////////////////////////////////
//  First, the "pure" implementation of the BVH
//////////////////////////////////////////////////

// Work item for creation of BVH:
struct BBoxTmp {
    // Bottom point (ie minx,miny,minz)
    Vector3 _bottom;
    // Top point (ie maxx,maxy,maxz)
    Vector3 _top;
    // Center point, ie 0.5*(top-bottom)
    Vector3 _center;
    // Triangle
    const Triangle *_pTri;
    BBoxTmp()
	:
	_bottom(FLT_MAX,FLT_MAX,FLT_MAX),
	_top(-FLT_MAX,-FLT_MAX,-FLT_MAX),
	_pTri(NULL)
	{}
};
typedef vector<BBoxTmp> BBoxEntries;

// This builds the BVH, finding optimal split planes for each depth
BVHNode *Recurse(BBoxEntries& work, REPORTPRM(float pct=0.) int depth=0)
{
    REPORT( coord pctSpan  = 11.f/pow(3.f,depth); )
    if (work.size()<4) {
	BVHLeaf *leaf = new BVHLeaf;
	for(BBoxEntries::iterator it=work.begin(); it!=work.end(); ++it)
	    leaf->_triangles.push_back(it->_pTri);
	return leaf;
    }
    // Start by finding the working list's bounding box
    Vector3 bottom(FLT_MAX,FLT_MAX,FLT_MAX), top(-FLT_MAX,-FLT_MAX,-FLT_MAX);
    for(unsigned i=0; i<work.size(); i++) {
	BBoxTmp& v = work[i];
	bottom.assignSmaller(v._bottom);
	top.assignBigger(v._top);
    }

    coord side1 = top._x-bottom._x;
    coord side2 = top._y-bottom._y;
    coord side3 = top._z-bottom._z;
    // The current box has a cost of (No of triangles)*surfaceArea
    coord minCost = work.size() * (side1*side2 + side2*side3 + side3*side1);
    coord bestSplit = FLT_MAX; // will indicate no split with better cost found (below)
    int bestAxis = -1;

    // Try all different axis
    for (int j=0; j<3; j++) {

	int axis = j;

	// we will try dividing the triangles based on the current axis,
	// and we will try split values from "start" to "stop", one "step" at a time.
	coord start, stop, step;

	if (axis==0) {
	    start = bottom._x;
	    stop  = top._x;
	} else if (axis==1) {
	    start = bottom._y;
	    stop  = top._y;
	} else {
	    start = bottom._z;
	    stop  = top._z;
	}
	// In that axis, do the bounding boxes in the work queue "span" across?
	// Or are they all already "packed" on the axis's plane?
	if (fabsf(stop-start)<1e-4)
	    // No, we must move to a different axis!
	    continue;

	// Try splitting at a uniform sampling that gets smaller the deeper we go:
	// size of "sampling grid": 1024 (depth 0), 512 (depth 1), etc
	step = (stop-start)/(1024.f/(depth+1.f));
	#ifdef PROGRESS_REPORT
	// Progress report variables...
	coord pctStart = pct + j*pctSpan;
	coord pctStep  = pctSpan/((stop - start - 2*step)/step);
	#endif
	for(coord testSplit=start+step; testSplit<stop-step; testSplit+=step) {
	    #ifdef PROGRESS_REPORT
	    if ((PRINT_REPORT_EVERY&g_reportCounter++) == 0) {
		printf("\b\b\b%02d%%", int(pctStart)); fflush(stdout);
		stringstream caption;
		caption << BUILDING_BVH_MSG << int(pctStart) << "%";
		SDL_WM_SetCaption(caption.str().c_str(), caption.str().c_str());
	    }
	    pctStart += pctStep;
	    #endif
	    // The left and right bounding box
	    Vector3 lbottom(FLT_MAX,FLT_MAX,FLT_MAX), ltop(-FLT_MAX,-FLT_MAX,-FLT_MAX);
	    Vector3 rbottom(FLT_MAX,FLT_MAX,FLT_MAX), rtop(-FLT_MAX,-FLT_MAX,-FLT_MAX);
	    // The number of triangles in the left and right bboxes
	    int countLeft=0, countRight=0;
	    // For each test split, allocate triangles based on their bounding boxes centers
	    for(unsigned i=0; i<work.size(); i++) {
		BBoxTmp& v = work[i];

		coord value;
		if (axis==0) value=v._center._x;
		else if (axis==1) value=v._center._y;
		else value=v._center._z;

		if (value < testSplit) {
		    lbottom.assignSmaller(v._bottom);
		    ltop.assignBigger(v._top);
		    countLeft++;
		} else {
		    rbottom.assignSmaller(v._bottom);
		    rtop.assignBigger(v._top);
		    countRight++;
		}
	    }
	    // Now use the Surface Area Heuristic to see if this split has a better "cost"
	    //
	    // First, check for stupid partitionings
	    if (countLeft<=1 || countRight<=1) continue;
	    // It's a real partitioning, calculate the surface areas
	    coord lside1 = ltop._x-lbottom._x;
	    coord lside2 = ltop._y-lbottom._y;
	    coord lside3 = ltop._z-lbottom._z;
	    coord rside1 = rtop._x-rbottom._x;
	    coord rside2 = rtop._y-rbottom._y;
	    coord rside3 = rtop._z-rbottom._z;
	    coord surfaceLeft  = lside1*lside2 + lside2*lside3 + lside3*lside1;
	    coord surfaceRight = rside1*rside2 + rside2*rside3 + rside3*rside1;
	    coord totalCost = surfaceLeft*countLeft + surfaceRight*countRight;
	    if (totalCost < minCost) {
		minCost = totalCost;
		bestSplit = testSplit;
		bestAxis = axis;
	    }
	}
    }

    // We found no split to improve the cost, create a BVH leaf
    if (bestAxis == -1) {
	BVHLeaf *leaf = new BVHLeaf;
	for(BBoxEntries::iterator it=work.begin(); it!=work.end(); ++it)
	    leaf->_triangles.push_back(it->_pTri);
	return leaf;
    }

    // Create a BVH inner node, split with the optimal value we found above
    BBoxEntries left, right;
    Vector3 lbottom(FLT_MAX,FLT_MAX,FLT_MAX), ltop(-FLT_MAX,-FLT_MAX,-FLT_MAX);
    Vector3 rbottom(FLT_MAX,FLT_MAX,FLT_MAX), rtop(-FLT_MAX,-FLT_MAX,-FLT_MAX);
    for(int i=0; i<(int)work.size(); i++) {
	BBoxTmp& v = work[i];

	coord value;
	if (bestAxis==0) value=v._center._x;
	else if (bestAxis==1) value=v._center._y;
	else value=v._center._z;

	if (value < bestSplit) {
	    #ifdef DEBUG_LOG_BVH
	    printf("LADD: B(%f %f %f) T(%f %f %f) C(%f %f %f)\n",
		v._bottom._x,
		v._bottom._y,
		v._bottom._z,
		v._top._x,
		v._top._y,
		v._top._z,
		v._center._x,
		v._center._y,
		v._center._z);
	    #endif
	    left.push_back(v);
	    lbottom.assignSmaller(v._bottom);
	    ltop.assignBigger(v._top);
	} else {
	    #ifdef DEBUG_LOG_BVH
	    printf("RADD: B(%f %f %f) T(%f %f %f) C(%f %f %f)\n",
		v._bottom._x,
		v._bottom._y,
		v._bottom._z,
		v._top._x,
		v._top._y,
		v._top._z,
		v._center._x,
		v._center._y,
		v._center._z);
	    #endif
	    right.push_back(v);
	    rbottom.assignSmaller(v._bottom);
	    rtop.assignBigger(v._top);
	}
    }
    BVHInner *inner = new BVHInner;
    #ifdef PROGRESS_REPORT
    if ((PRINT_REPORT_EVERY&g_reportCounter++) == 0) {
	printf("\b\b\b%2d%%", int(pct+3.f*pctSpan)); // Update progress indicator
	fflush(stdout);
	stringstream caption;
	caption << BUILDING_BVH_MSG << int(pct+3.f*pctSpan) << "%";
	SDL_WM_SetCaption(caption.str().c_str(), caption.str().c_str());
    }
    #endif
    inner->_left = Recurse(left, REPORTPRM(pct+3.f*pctSpan) depth+1);
    inner->_left->_bottom = lbottom;
    inner->_left->_top = ltop;
    #ifdef PROGRESS_REPORT
    if ((PRINT_REPORT_EVERY&g_reportCounter++) == 0) {
	printf("\b\b\b%2d%%", int(pct+6.f*pctSpan)); // Update progress indicator
	fflush(stdout);
	stringstream caption;
	caption << BUILDING_BVH_MSG << int(pct+6.f*pctSpan) << "%";
	SDL_WM_SetCaption(caption.str().c_str(), caption.str().c_str());
    }
    #endif
    inner->_right = Recurse(right, REPORTPRM(pct+6.f*pctSpan) depth+1);
    inner->_right->_bottom = rbottom;
    inner->_right->_top = rtop;

    // Used for debugging only - dump the BVH in stdout
    #ifdef DEBUG_LOG_BVH
    coord origRange[2], newRangeL[2], newRangeR[2];
    if (bestAxis==0) {
	origRange[0] = bottom._x;
	origRange[1] = top._x;
	newRangeL[0] = lbottom._x;
	newRangeL[1] = ltop._x;
	newRangeR[0] = rbottom._x;
	newRangeR[1] = rtop._x;
    } else if (bestAxis==1) {
	origRange[0] = bottom._y;
	origRange[1] = top._y;
	newRangeL[0] = lbottom._y;
	newRangeL[1] = ltop._y;
	newRangeR[0] = rbottom._y;
	newRangeR[1] = rtop._y;
    } else {
	origRange[0] = bottom._z;
	origRange[1] = top._z;
	newRangeL[0] = lbottom._z;
	newRangeL[1] = ltop._z;
	newRangeR[0] = rbottom._z;
	newRangeR[1] = rtop._z;
    }
    printf("(%9f,%9f) => (%9f,%9f) and (%9f,%9f)\n", origRange[0], origRange[1],
	newRangeL[0], newRangeL[1], newRangeR[0], newRangeR[1]);
    #endif

    return inner;
}

BVHNode *CreateBVH(const Scene *pScene)
{
    vector<BBoxTmp> work;
    Vector3 bottom(FLT_MAX,FLT_MAX,FLT_MAX), top(-FLT_MAX,-FLT_MAX,-FLT_MAX);

    puts("Gathering bounding box info from all triangles...");
    for(int j=0; j<(int)pScene->_triangles.size(); j++) {
	const Triangle& triangle = pScene->_triangles[j];

	BBoxTmp b;
	b._pTri = &triangle;

	b._bottom.assignSmaller(*triangle._vertexA);
	b._bottom.assignSmaller(*triangle._vertexB);
	b._bottom.assignSmaller(*triangle._vertexC);
	b._top.assignBigger(*triangle._vertexA);
	b._top.assignBigger(*triangle._vertexB);
	b._top.assignBigger(*triangle._vertexC);

	bottom.assignSmaller(b._bottom);
	top.assignBigger(b._top);

	b._center = b._top;
	b._center += b._bottom;
	b._center *= 0.5f;
	work.push_back(b);
	#ifdef DEBUG_LOG_BVH
	printf("ADD: B(%f %f %f) T(%f %f %f) C(%f %f %f)\n",
	    b._bottom._x,
	    b._bottom._y,
	    b._bottom._z,
	    b._top._x,
	    b._top._y,
	    b._top._z,
	    b._center._x,
	    b._center._y,
	    b._center._z);
	#endif
    }

    // ...and pass it to the recursive function that creates the SAH AABB BVH
    // (Surface Area Heuristic, Axis-Aligned Bounding Boxes, Bounding Volume Hierarchy)
    printf("Creating Bounding Volume Hierarchy data...    "); fflush(stdout);
    BVHNode *root = Recurse(work);
    printf("\b\b\b100%%\n");
    root->_bottom = bottom;
    root->_top = top;

    return root;
}

#else

#define BUILDING_BVH_MSG "Building BVH with SSE: "

//
// SSE instructions are available, use SIMD intrinsics
//

#include <xmmintrin.h>

// Work item for creation of BVH:
struct BBoxTmp {
    // Bottom point (ie minx,miny,minz)
    __m128 _bottom;
    // Top point (ie maxx,maxy,maxz)
    __m128 _top;
    // Center point, ie 0.5*(top-bottom)
    __m128 _center;
    // Triangle
    const Triangle *_pTri;
    BBoxTmp()
	:
	_bottom(_mm_set1_ps(FLT_MAX)),
	_top(_mm_set1_ps(-FLT_MAX)),
	_pTri(NULL)
	{
	}
};

#define SSETOVECTOR3(varV, varSSE)						\
    Vector3 varV;								\
    hlp = _mm_shuffle_ps(varSSE, varSSE, _MM_SHUFFLE(0,0,0,3));			\
    varV._x = _mm_cvtss_f32(hlp);						\
    hlp = _mm_shuffle_ps(varSSE, varSSE, _MM_SHUFFLE(0,0,0,2));			\
    varV._y = _mm_cvtss_f32(hlp);						\
    hlp = _mm_shuffle_ps(varSSE, varSSE, _MM_SHUFFLE(0,0,0,1));			\
    varV._z = _mm_cvtss_f32(hlp);

typedef BBoxTmp* BBoxEntries;

/*
BBoxTmp* alignedAlloc(size_t n)
{
    long tmp = reinterpret_cast<long>(malloc(n+15));
    tmp &= ~0xF;
    tmp += 16;
    return (BBoxTmp*)tmp;
}
*/

// This builds the BVH, finding optimal split planes for each depth
BVHNode *Recurse(int size, BBoxEntries& work, REPORTPRM(float pct=0.) int depth=0)
{
    __m128 hlp;

    REPORT( coord pctSpan  = 11.f/pow(3.f,depth); )
    if (size<4) {
	BVHLeaf *leaf = new BVHLeaf;
	for(int j=0; j<size; j++)
	    leaf->_triangles.push_back(work[j]._pTri);
	return leaf;
    }

    // Start by finding the working list's bounding box
    __m128 ssebottom(_mm_set1_ps(FLT_MAX)), ssetop(_mm_set1_ps(-FLT_MAX));
    for(int i=0; i<size; i++) {
        BBoxTmp& v = work[i];
        ssebottom = _mm_min_ps(ssebottom, v._bottom);
        ssetop = _mm_max_ps(ssetop, v._top);
    }

    __m128 topMinusBottom = _mm_sub_ps(ssetop, ssebottom);
    __m128 topMinusBottomShifted = _mm_shuffle_ps(
	topMinusBottom,	topMinusBottom,	_MM_SHUFFLE(2,1,3,0));
    topMinusBottom = _mm_mul_ps(topMinusBottom, topMinusBottomShifted);
    topMinusBottomShifted = _mm_shuffle_ps(
        topMinusBottom,topMinusBottom,_MM_SHUFFLE(2,0,0,0));
    __m128 topMinusBottomShifted2 = _mm_shuffle_ps(
        topMinusBottom,topMinusBottom,_MM_SHUFFLE(1,0,0,0));
    topMinusBottom = _mm_add_ps(topMinusBottom, topMinusBottomShifted);
    topMinusBottom = _mm_add_ps(topMinusBottom, topMinusBottomShifted2);
    topMinusBottom = _mm_shuffle_ps(
	topMinusBottom,topMinusBottom,_MM_SHUFFLE(0,0,0,3));

    // The current box has a cost of (No of triangles)*surfaceArea
    coord minCost = size * _mm_cvtss_f32(topMinusBottom);
	    //(side1*side2 + side2*side3 + side3*side1);

    SSETOVECTOR3(bottom, ssebottom)
    SSETOVECTOR3(top, ssetop)

    coord bestSplit = FLT_MAX; // will indicate no split with better cost found (below)
    int bestAxis = -1;
    int bestCountLeft = 0, bestCountRight = 0;

    // Try all different axis
    for (int testAxis=0; testAxis<3; testAxis++) {

	int axis = testAxis;

	// we will try dividing the triangles based on the current axis,
	// and we will try split values from "start" to "stop", one "step" at a time.
	coord start, stop, step;

	start = bottom._v[axis];
	stop  = top._v[axis];

	// In that axis, do the bounding boxes in the work queue "span" across?
	// Or are they all already "packed" on the axis's plane?
	if (fabsf(stop-start)<1e-4)
	    // No, we must move to a different axis!
	    continue;

	// Try splitting at a uniform sampling that gets smaller the deeper we go:
	// size of "sampling grid": 1024 (depth 0), 512 (depth 1), etc
	step = (stop-start)/(1024.f/(depth+1.f));

	// Progress report variables...
	REPORT( coord pctStart = pct + testAxis*pctSpan; )
	REPORT( coord pctStep  = pctSpan/((stop - start - 2*step)/step); )

	for(coord testSplit=start+step; testSplit<stop-step; testSplit+=step) {
	    #ifdef PROGRESS_REPORT
	    if ((PRINT_REPORT_EVERY&g_reportCounter++) == 0) {
		printf("\b\b\b%02d%%", int(pctStart)); fflush(stdout);
		stringstream caption;
		caption << BUILDING_BVH_MSG << int(pctStart) << "%";
		SDL_WM_SetCaption(caption.str().c_str(), caption.str().c_str());
	    }
	    pctStart += pctStep;
	    #endif

	    // The left and right bounding box
	    __m128 lbottom(_mm_set1_ps(FLT_MAX)), ltop(_mm_set1_ps(-FLT_MAX));
	    __m128 rbottom(_mm_set1_ps(FLT_MAX)), rtop(_mm_set1_ps(-FLT_MAX));

	    // The number of triangles in the left and right bboxes
	    int countLeft=0, countRight=0;
	    // For each test split, allocate triangles based on their bounding boxes centers
	    for(int i=0; i<size; i++) {
		const BBoxTmp& v = work[i];

		coord value;
		if (axis == 0)
		    hlp = _mm_shuffle_ps(v._center, v._center, _MM_SHUFFLE(0,0,0,3));
		else if (axis == 1)
		    hlp = _mm_shuffle_ps(v._center, v._center, _MM_SHUFFLE(0,0,0,2));
		else
		    hlp = _mm_shuffle_ps(v._center, v._center, _MM_SHUFFLE(0,0,0,1));
		value = _mm_cvtss_f32(hlp);

		if (value < testSplit) {
		    lbottom = _mm_min_ps(lbottom, v._bottom);
		    ltop = _mm_max_ps(ltop, v._top);
		    countLeft++;
		} else {
		    rbottom = _mm_min_ps(rbottom, v._bottom);
		    rtop = _mm_max_ps(rtop, v._top);
		    countRight++;
		}
	    }

	    // Now use the Surface Area Heuristic to see if this split has a better "cost"
	    //
	    // First, check for stupid partitionings
	    if (countLeft<=1 || countRight<=1) continue;

	    __m128 ltopMinusBottom = _mm_sub_ps(ltop, lbottom);
	    __m128 rtopMinusBottom = _mm_sub_ps(rtop, rbottom);

	    __m128 ltopMinusBottomShifted = _mm_shuffle_ps(
		ltopMinusBottom, ltopMinusBottom, _MM_SHUFFLE(2,1,3,0));
	    ltopMinusBottom = _mm_mul_ps(ltopMinusBottom, ltopMinusBottomShifted);
	    ltopMinusBottomShifted = _mm_shuffle_ps(
		ltopMinusBottom,ltopMinusBottom,_MM_SHUFFLE(2,0,0,0));
	    __m128 ltopMinusBottomShifted2 = _mm_shuffle_ps(
		ltopMinusBottom,ltopMinusBottom,_MM_SHUFFLE(1,0,0,0));
	    ltopMinusBottom = _mm_add_ps(ltopMinusBottom, ltopMinusBottomShifted);
	    ltopMinusBottom = _mm_add_ps(ltopMinusBottom, ltopMinusBottomShifted2);
	    ltopMinusBottom = _mm_shuffle_ps(
		ltopMinusBottom,ltopMinusBottom,_MM_SHUFFLE(0,0,0,3));
	    coord leftCost = countLeft * _mm_cvtss_f32(ltopMinusBottom);

	    __m128 rtopMinusBottomShifted = _mm_shuffle_ps(
		rtopMinusBottom, rtopMinusBottom, _MM_SHUFFLE(2,1,3,0));
	    rtopMinusBottom = _mm_mul_ps(rtopMinusBottom, rtopMinusBottomShifted);
	    rtopMinusBottomShifted = _mm_shuffle_ps(
		rtopMinusBottom,rtopMinusBottom,_MM_SHUFFLE(2,0,0,0));
	    __m128 rtopMinusBottomShifted2 = _mm_shuffle_ps(
		rtopMinusBottom,rtopMinusBottom,_MM_SHUFFLE(1,0,0,0));
	    rtopMinusBottom = _mm_add_ps(rtopMinusBottom, rtopMinusBottomShifted);
	    rtopMinusBottom = _mm_add_ps(rtopMinusBottom, rtopMinusBottomShifted2);
	    rtopMinusBottom = _mm_shuffle_ps(
		rtopMinusBottom,rtopMinusBottom,_MM_SHUFFLE(0,0,0,3));
	    coord rightCost = countRight * _mm_cvtss_f32(rtopMinusBottom);

	    coord totalCost = leftCost + rightCost;
	    if (totalCost < minCost) {
		minCost = totalCost;
		bestSplit = testSplit;
		bestAxis = axis;
		bestCountLeft = countLeft;
		bestCountRight = countRight;
	    }
	}
    }

    // We found no split to improve the cost, create a BVH leaf
    if (bestAxis == -1) {
	BVHLeaf *leaf = new BVHLeaf;
	for(int j=0; j<size; j++)
	    leaf->_triangles.push_back(work[j]._pTri);
	return leaf;
    }

    // Create a BVH inner node, split with the optimal value we found above
    BBoxEntries left, right;
    left  = (BBoxTmp*)_mm_malloc(bestCountLeft*sizeof(BBoxTmp), 16);
    right = (BBoxTmp*)_mm_malloc(bestCountRight*sizeof(BBoxTmp), 16);

    int countLeft=0, countRight=0;
    __m128 lbottom(_mm_set1_ps(FLT_MAX)), ltop(_mm_set1_ps(-FLT_MAX));
    __m128 rbottom(_mm_set1_ps(FLT_MAX)), rtop(_mm_set1_ps(-FLT_MAX));

    for(int i=0; i<size; i++) {
	const BBoxTmp& v = work[i];
	coord value;
	if (bestAxis == 0)
	    hlp = _mm_shuffle_ps(v._center, v._center, _MM_SHUFFLE(0,0,0,3));
	else if (bestAxis == 1)
	    hlp = _mm_shuffle_ps(v._center, v._center, _MM_SHUFFLE(0,0,0,2));
	else
	    hlp = _mm_shuffle_ps(v._center, v._center, _MM_SHUFFLE(0,0,0,1));
	value = _mm_cvtss_f32(hlp);

	if (value < bestSplit) {
	    #ifdef DEBUG_LOG_BVH
	    SSETOVECTOR3(dbb, v._bottom)
	    SSETOVECTOR3(dbt, v._top)
	    SSETOVECTOR3(dbc, v._center)
	    printf("LADD: B(%f %f %f) T(%f %f %f) C(%f %f %f)\n",
		dbb._x, dbb._y, dbb._z,
		dbt._x, dbt._y, dbt._z,
		dbc._x, dbc._y, dbc._z);
	    #endif
	    lbottom = _mm_min_ps(lbottom, v._bottom);
	    ltop = _mm_max_ps(ltop, v._top);
	    left[countLeft++] = v;
	} else {
	    #ifdef DEBUG_LOG_BVH
	    SSETOVECTOR3(dbb, v._bottom)
	    SSETOVECTOR3(dbt, v._top)
	    SSETOVECTOR3(dbc, v._center)
	    printf("RADD: B(%f %f %f) T(%f %f %f) C(%f %f %f)\n",
		dbb._x, dbb._y, dbb._z,
		dbt._x, dbt._y, dbt._z,
		dbc._x, dbc._y, dbc._z);
	    #endif
	    rbottom = _mm_min_ps(rbottom, v._bottom);
	    rtop = _mm_max_ps(rtop, v._top);
	    right[countRight++] = v;
	}
    }

    SSETOVECTOR3(vlBottom, lbottom)
    SSETOVECTOR3(vlTop, ltop)
    SSETOVECTOR3(vrBottom, rbottom)
    SSETOVECTOR3(vrTop, rtop)

    BVHInner *inner = new BVHInner;
    #ifdef PROGRESS_REPORT
    if ((PRINT_REPORT_EVERY&g_reportCounter++) == 0) {
	printf("\b\b\b%2d%%", int(pct+3.f*pctSpan)); // Update progress indicator
	fflush(stdout);
	stringstream caption;
	caption << BUILDING_BVH_MSG << int(pct+3.f*pctSpan) << "%";
	SDL_WM_SetCaption(caption.str().c_str(), caption.str().c_str());
    }
    #endif
    inner->_left = Recurse(countLeft, left, REPORTPRM(pct+3.f*pctSpan) depth+1);
    inner->_left->_bottom = vlBottom;
    inner->_left->_top = vlTop;
    #ifdef PROGRESS_REPORT
    if ((PRINT_REPORT_EVERY&g_reportCounter++) == 0) {
	printf("\b\b\b%2d%%", int(pct+6.f*pctSpan)); // Update progress indicator
	fflush(stdout);
	stringstream caption;
	caption << BUILDING_BVH_MSG << int(pct+6.f*pctSpan) << "%";
	SDL_WM_SetCaption(caption.str().c_str(), caption.str().c_str());
    }
    #endif
    inner->_right = Recurse(countRight, right, REPORTPRM(pct+6.f*pctSpan) depth+1);
    inner->_right->_bottom = vrBottom;
    inner->_right->_top = vrTop;

    #ifdef DEBUG_LOG_BVH
    // Used for debugging only - dump the BVH in stdout
    {
        coord origRange[2], newRangeL[2], newRangeR[2];
        if (bestAxis==0) {
            origRange[0] = bottom._x;
            origRange[1] = top._x;
            newRangeL[0] = vlBottom._x;
            newRangeL[1] = vlTop._x;
            newRangeR[0] = vrBottom._x;
            newRangeR[1] = vrTop._x;
        } else if (bestAxis==1) {
            origRange[0] = bottom._y;
            origRange[1] = top._y;
            newRangeL[0] = vlBottom._y;
            newRangeL[1] = vlTop._y;
            newRangeR[0] = vrBottom._y;
            newRangeR[1] = vrTop._y;
        } else {
            origRange[0] = bottom._z;
            origRange[1] = top._z;
            newRangeL[0] = vlBottom._z;
            newRangeL[1] = vlTop._z;
            newRangeR[0] = vrBottom._z;
            newRangeR[1] = vrTop._z;
        }
        printf("(%9f,%9f) => (%9f,%9f) and (%9f,%9f)\n", origRange[0], origRange[1], 
           newRangeL[0], newRangeL[1], newRangeR[0], newRangeR[1]);
    }
    #endif

    _mm_free(left);
    _mm_free(right);
    return inner;
}

BVHNode *CreateBVH(const Scene *pScene)
{
    __m128 hlp;

    BBoxEntries work = (BBoxTmp*)_mm_malloc(pScene->_triangles.size()*sizeof(BBoxTmp), 16);
    __m128 bottom(_mm_set1_ps(FLT_MAX)), top(_mm_set1_ps(-FLT_MAX));

    ASSERT_OR_DIE(pScene->_triangles.size());

    puts("Gathering bounding box info from all triangles...");
    for(unsigned j=0; j<pScene->_triangles.size(); j++) {
	const Triangle& triangle = pScene->_triangles[j];

	BBoxTmp b;
	b._pTri = &triangle;

	const Vertex *p1 = triangle._vertexA;
	const Vertex *p2 = triangle._vertexB;
	const Vertex *p3 = triangle._vertexC;
	b._bottom = _mm_min_ps(b._bottom, _mm_set_ps(p1->_x, p1->_y, p1->_z, 0.));
	b._bottom = _mm_min_ps(b._bottom, _mm_set_ps(p2->_x, p2->_y, p2->_z, 0.));
	b._bottom = _mm_min_ps(b._bottom, _mm_set_ps(p3->_x, p3->_y, p3->_z, 0.));
	b._top = _mm_max_ps(b._top, _mm_set_ps(p1->_x, p1->_y, p1->_z, 0.));
	b._top = _mm_max_ps(b._top, _mm_set_ps(p2->_x, p2->_y, p2->_z, 0.));
	b._top = _mm_max_ps(b._top, _mm_set_ps(p3->_x, p3->_y, p3->_z, 0.));

	bottom = _mm_min_ps(bottom, b._bottom);
	top = _mm_max_ps(top, b._top);

	b._center = _mm_add_ps(b._top, b._bottom);
	b._center = _mm_mul_ps(b._center, _mm_set1_ps(0.5f));
	work[j] = b;
	#ifdef DEBUG_LOG_BVH
	{
	    SSETOVECTOR3(dbb, b._bottom)
	    SSETOVECTOR3(dbt, b._top)
	    SSETOVECTOR3(dbc, b._center)
	    printf("ADD: B(%f %f %f) T(%f %f %f) C(%f %f %f)\n",
		dbb._x, dbb._y, dbb._z,
		dbt._x, dbt._y, dbt._z,
		dbc._x, dbc._y, dbc._z);
	}
	#endif
    }
    // ...and pass it to the recursive function that creates the SAH AABB BVH
    // (Surface Area Heuristic, Axis-Aligned Bounding Boxes, Bounding Volume Hierarchy)
    printf("Creating Bounding Volume Hierarchy data...    "); fflush(stdout);
    BVHNode *root = Recurse(pScene->_triangles.size(), work);
    printf("\b\b\b100%%\n");

    SSETOVECTOR3(b, bottom)
    SSETOVECTOR3(t, top)

    root->_bottom = b;
    root->_top = t;

    _mm_free(work);
    return root;
}

#endif
