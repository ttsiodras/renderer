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
#include <sstream>
#include <cfloat>

#ifdef USE_TBB
#include "tbb/blocked_range.h"
#include "tbb/parallel_for.h"
#endif

#ifdef USE_OPENMP
#include <omp.h>
#endif

#include "3d.h"
#include "Screen.h"
#include "Clock.h"

// Takes lots of time to raytrace a frame, provide quick abort via keys
#include "Keyboard.h"

///////////////////////////////////////////////
// Raytracing code:
//    RENDER_RAYTRACE
//    RENDER_RAYTRACE_ANTIALIAS
///////////////////////////////////////////////

/////////////////////////////////
// Raytracing configuration

// Should we use Phong interpolation of the normal vector?
#define USE_PHONG_NORMAL

// What depth to stop reflections and refractions?
#define MAX_RAY_DEPTH	    3

// Ray intersections of a distance <=NUDGE_FACTOR (from the origin) don't count
#define NUDGE_FACTOR     1e-5f

//////////////////////////////
// Should we cast shadow rays?
#define USE_SHADOWS

//////////////////////////////
// Enable reflections?
#define REFLECTIONS
#define REFLECTIONS_RATE 0.375

//////////////////////////////
// Enable refractions?
//#define REFRACTIONS
#define REFRACTIONS_RATE 0.58

//////////////////////////////
// Enable ambient occlusion?
//#define AMBIENT_OCCLUSION
// How many ambient rays to spawn per ray intersection?
#define AMBIENT_SAMPLES  32
// How close to check for ambient occlusion?
#define AMBIENT_RANGE    0.15f

// Maximum allowed depth of BVH
// Checked at BVH build time, no runtime crash possible, see below
#define BVH_STACK_SIZE 32

//#define RTCORETEST
//#ifdef RTCORETEST
//#undef MAX_RAY_DEPTH
//#undef USE_SHADOWS
//#undef USE_PHONG_NORMAL
//#undef AMBIENT_OCCLUSION
//#undef REFRACTIONS
//#undef REFLECTIONS
//#define MAX_RAY_DEPTH 1
//#endif

// Helper function, that checks whether a ray intersects a bbox
inline bool RayIntersectsBox(const Vector3& originInWorldSpace, const Vector3& rayInWorldSpace, CacheFriendlyBVHNode *pBox)
{
    // set Tnear = - infinity, Tfar = infinity
    //
    // For each pair of planes P associated with X, Y, and Z do:
    //     (example using X planes)
    //     if direction Xd = 0 then the ray is parallel to the X planes, so
    //         if origin Xo is not between the slabs ( Xo < Xl or Xo > Xh) then
    //             return false
    //     else, if the ray is not parallel to the plane then
    //     begin
    //         compute the intersection distance of the planes
    //         T1 = (Xl - Xo) / Xd
    //         T2 = (Xh - Xo) / Xd
    //         If T1 > T2 swap (T1, T2) /* since T1 intersection with near plane */
    //         If T1 > Tnear set Tnear =T1 /* want largest Tnear */
    //         If T2 < Tfar set Tfar="T2" /* want smallest Tfar */
    //         If Tnear > Tfar box is missed so
    //             return false
    //         If Tfar < 0 box is behind ray
    //             return false
    //     end
    // end of for loop
    //
    // If Box survived all above tests, return true with intersection point Tnear and exit point Tfar.

    CacheFriendlyBVHNode& box = *pBox;
    coord Tnear, Tfar;
    Tnear = -FLT_MAX;
    Tfar = FLT_MAX;

#define CHECK_NEAR_AND_FAR_INTERSECTION(c)										    \
    if (rayInWorldSpace._ ## c == 0.) {								    \
	if (originInWorldSpace._##c < box._bottom._##c) return false;				    \
	if (originInWorldSpace._##c > box._top._##c) return false;				    \
    } else {											    \
	coord T1 = (box._bottom._##c - originInWorldSpace._##c)/rayInWorldSpace._##c;		    \
	coord T2 = (box._top._##c    - originInWorldSpace._##c)/rayInWorldSpace._##c;		    \
	if (T1>T2) { coord tmp=T1; T1=T2; T2=tmp; }						    \
	if (T1 > Tnear) Tnear = T1;								    \
	if (T2 < Tfar)  Tfar = T2;								    \
	if (Tnear > Tfar)									    \
	    return false;									    \
	if (Tfar < 0.)										    \
	    return false;									    \
    }

    CHECK_NEAR_AND_FAR_INTERSECTION(x)
    CHECK_NEAR_AND_FAR_INTERSECTION(y)
    CHECK_NEAR_AND_FAR_INTERSECTION(z)

    return true;
}

template <bool antialias>
class RaytraceScanline {
    // Since this class contains only references and has no virtual methods, it (hopefully)
    // doesn't exist in runtime; it is optimized away when RaytraceHorizontalSegment is called.
    const Scene& scene;
    const Camera& eye;
    Screen& canvas;
    int& y;
public:
    RaytraceScanline(
	const Scene& scene, const Camera& e, Screen& c,
	int& scanline)
	:
	scene(scene),
	eye(e),
	canvas(c),
	y(scanline)
    {}

    // Templated member - offers two compile-time options:
    //
    // The first one is used to discriminate between shadow rays (that stop at the first hit)
    // and normal rays, that have to find the closest hit.
    //
    // The second one enables or disables culling of backfacing triangles, and is enabled
    // by the refraction call (which needs both front and back-faces), but disabled for
    // reflections and shadow rays.
    //
    // C++ power...
    //
    template <bool stopAtfirstRayHit, bool doCulling>
    bool BVH_IntersectTriangles(
	// Inputs
	const Vector3& origin, const Vector3& ray, const Triangle *avoidSelf,
	// outputs
	const Triangle*& pBestTri,
	//
	// both inputs and outputs!
	//
	// for normal rays:
	//  pointHitInWorldSpace (output)
        //  kXX (outputs) perpendicular distances of intersection point from the 3 triangle edges
	//     (used for PhongNormal calculations)
	//
	// for shadow rays:
	//  pointHitInWorldSpace (input) provides the light position
	Vector3& pointHitInWorldSpace,
	coord& kAB, coord& kBC, coord& kCA) const
    {
	// in the loop below, maintain the closest triangle and the point where we hit it:
	pBestTri = NULL;
	coord bestTriDist;

	// light position passed-in pointHitInWorldSpace (only in shadow mode - i.e. stopAtfirstRayHit=true)
	Vector3& lightPos = pointHitInWorldSpace;

	// Compile-time work (stopAtfirstRayHit is template param)
	if (stopAtfirstRayHit)
	    // In shadow ray mode, start from light distance
	    bestTriDist = distancesq(origin, lightPos);
	else
	    // In normal mode, start from infinity
	    bestTriDist = FLT_MAX;

	CacheFriendlyBVHNode* stack[BVH_STACK_SIZE];
	int stackIdx = 0;
	stack[stackIdx++] = scene._pCFBVH;
	while(stackIdx) {
	    CacheFriendlyBVHNode *pCurrent = stack[stackIdx-1];
	    stackIdx--;
	    //if (!pCurrent->IsLeaf()) {
	    if (!(pCurrent->u.leaf._count & 0x80000000)) {
		if (RayIntersectsBox(origin, ray, pCurrent)) {
		    stack[stackIdx++] = &scene._pCFBVH[pCurrent->u.inner._idxRight];
		    stack[stackIdx++] = &scene._pCFBVH[pCurrent->u.inner._idxLeft];
		    assert(stackIdx<=BVH_STACK_SIZE);
		}
	    } else {
		//BVHLeaf *p = dynamic_cast<BVHLeaf*>(pCurrent);
		//for(std::list<const Triangle*>::iterator it=p->_triangles.begin();
		//    it != p->_triangles.end();
		//    ++it)
		for(unsigned i=pCurrent->u.leaf._startIndexInTriIndexList;
		    i<pCurrent->u.leaf._startIndexInTriIndexList + (pCurrent->u.leaf._count & 0x7fffffff);
		    i++)
		{
		    //const Triangle& triangle = *(*it);
		    const Triangle& triangle = scene._triangles[scene._triIndexList[i]];

		    if (avoidSelf == &triangle)
			continue; // avoid self-reflections/refractions

		    // doCulling is a compile-time param, this code will be "codegenerated"
		    // at compile time only for reflection-related calls to Raytrace (see below)
		    if (doCulling && !triangle._twoSided) {
			// Check visibility of triangle via dot product
			Vector3 fromTriToOrigin = origin;
			fromTriToOrigin -= triangle._center;
			// Normally we would normalize, but since we just need the sign
			// of the dot product (to determine if it facing us or not)...
			//fromTriToOrigin.normalize();
			if (dot(fromTriToOrigin, triangle._normal)<0)
			    continue;
		    }

		    // Use the pre-computed triangle intersection data: normal, d, e1/d1, e2/d2, e3/d3
		    coord k = dot(triangle._normal, ray);
		    if (k == 0.0)
			continue; // this triangle is parallel to the ray, ignore it.

		    coord s = (triangle._d - dot(triangle._normal, origin))/k;
		    if (s <= 0.0) // this triangle is "behind" the origin.
			continue;
		    if (s <= NUDGE_FACTOR)
			continue;

		    Vector3 hit = ray*s;
		    hit += origin;

		    // Is the intersection of the ray with the triangle's plane INSIDE the triangle?
		    coord kt1 = dot(triangle._e1, hit) - triangle._d1; if (kt1<0.0) continue;
		    coord kt2 = dot(triangle._e2, hit) - triangle._d2; if (kt2<0.0) continue;
		    coord kt3 = dot(triangle._e3, hit) - triangle._d3; if (kt3<0.0) continue;

		    // It is, "hit" is the world space coordinate of the intersection.

		    // Was this a normal ray or a shadow ray? (template param)
		    if (stopAtfirstRayHit) {
			// Shadow ray, check whether the triangle obstructs the light
			coord dist = distancesq(lightPos, hit);
			if (dist < bestTriDist) // distance to light (squared) passed in kAB
			    return true; // we found a triangle obstructing the light, return true
		    } else {
			// Normal ray - it this intersection closer than all the others?
			coord hitZ = distancesq(origin, hit);
			if (hitZ < bestTriDist) {
			    // maintain the closest hit
			    bestTriDist = hitZ;
			    pBestTri = &triangle;
			    pointHitInWorldSpace = hit;
			    kAB = kt1;
			    kBC = kt2;
			    kCA = kt3;
			}
		    }
		}
	    }
	}
	// Normal ray or shadow ray? (compile-time template param)
	if (!stopAtfirstRayHit)
	    // for normal ray, return true if we pierced a triangle
	    return pBestTri != NULL;
	else
	    // for shadow ray, return true if we found a triangle obstructing the light.
	    return false;
    }

    // Templated member - offers a single compile-time option, whether we are doing culling or not.
    // This is used in the recursive call this member makes (!) to enable backface culling for reflection rays,
    // but disable it for refraction rays.
    //
    // Class-nested C++ recursion, provided via templates...
    template <bool doCulling>
    Pixel Raytrace(Vector3 originInWorldSpace, Vector3 rayInWorldSpace, const Triangle *avoidSelf, int depth) const
    {
	if (depth >= MAX_RAY_DEPTH)
	    return Pixel(0.,0.,0.);

	const Triangle *pBestTri = NULL;
	Vector3 pointHitInWorldSpace;
	coord kAB=0.f, kBC=0.f, kCA=0.f; // distances from the 3 edges of the triangle (from where we hit it)

	// Use the surface-area heuristic based, bounding volume hierarchy of axis-aligned bounding boxes
	// (keywords: SAH, BVH, AABB)
	if (!BVH_IntersectTriangles<false,doCulling>(
		originInWorldSpace, rayInWorldSpace, avoidSelf,
		pBestTri, pointHitInWorldSpace, kAB, kBC, kCA))
	    // We pierced no triangle, return with no contribution (ambient is black)
	    return Pixel(0.,0.,0.);

	// Set this to pass to recursive calls below, so that we don't get self-shadow or self-reflection
	// from this triangle...
	avoidSelf = pBestTri;

	// We'll also calculate the color contributed from this intersection
	// Start from the triangle's color
	Pixel color = pBestTri->_colorf;

#ifdef USE_PHONG_NORMAL
	// These are the closest triangle's vertices...
	const Vector3& bestTriA = *pBestTri->_vertexA;
	const Vector3& bestTriB = *pBestTri->_vertexB;
	const Vector3& bestTriC = *pBestTri->_vertexC;
	// ...and their normal vectors:
	const Vector3& bestTriNrmA = pBestTri->_vertexA->_normal;
	const Vector3& bestTriNrmB = pBestTri->_vertexB->_normal;
	const Vector3& bestTriNrmC = pBestTri->_vertexC->_normal;

	// We now want to interpolate the triangle's normal,
	// so that as the "pointHitInWorldSpace" gets closer to
	// a vertex X, the interpolated normal becomes closer to bestTriNrmX,
	// and becomes EXACTLY bestTriNrmX, if the pointHitInWorldSpace is X.
	//
	// To do that, we use the 3 areas of the triangle, as it is divided
	// by the pointHitInWorldSpace.
	//
	// This is the total triangle's area: cross product of two edges
	// (in fact, we should divide by 2, but since we're only interested
	//  in ratios (see below), there is no need)
	Vector3 AB = bestTriB; AB-= bestTriA;  // edge AB
	Vector3 BC = bestTriC; BC-= bestTriB;  // edge BC
	Vector3 crossAB_BC = cross(AB, BC);
	coord area = crossAB_BC.length();      // 2*area(ABC)

	// And these are the three sub-triangles - kAB,kBC,kCA were found above...
	coord ABx = kAB*distance(bestTriA, bestTriB);
	coord BCx = kBC*distance(bestTriB, bestTriC);
	coord CAx = kCA*distance(bestTriC, bestTriA);

	// use the area of the sub-triangle ACROSS a point, to scale!
	// (which means that if pointHitInCameraSpace is exactly on a vertex,
	//  the area of the sub-triangle becomes the area of the triangle!)
	Vector3 phongNormalA = bestTriNrmA; phongNormalA *= BCx / area;
	Vector3 phongNormalB = bestTriNrmB; phongNormalB *= CAx / area;
	Vector3 phongNormalC = bestTriNrmC; phongNormalC *= ABx / area;

	// and finally, accumulate the three contributions and normalize.
	Vector3 phongNormal = phongNormalA + phongNormalB + phongNormalC;
	phongNormal.normalize();
#else
	const Vector3& phongNormal = pBestTri->_normal;
#endif

#ifdef AMBIENT_OCCLUSION
	// Calculate ambient occlusion - throw AMBIENT_SAMPLES number of random rays 
	// in the hemisphere formed from the pointHitInWorldSpace and the normal vector...
	int i=0;
	coord totalLight = 0.f, maxLight = 0.f;
	while (i<AMBIENT_SAMPLES) {
	    Vector3 ambientRay = phongNormal;
	    ambientRay._x += float(rand()-RAND_MAX/2)/(RAND_MAX/2);
	    ambientRay._y += float(rand()-RAND_MAX/2)/(RAND_MAX/2);
	    ambientRay._z += float(rand()-RAND_MAX/2)/(RAND_MAX/2);
	    float cosangle = dot(ambientRay, phongNormal);
	    if (cosangle<0.f) continue;
	    i++;
	    maxLight += cosangle;
	    ambientRay.normalize();
	    Vector3 temp(pointHitInWorldSpace);
	    temp += ambientRay*AMBIENT_RANGE;
	    const Triangle *dummy;
	    // Some objects needs a "nudge", to avoid self-shadowing
	    //Vector3 nudgedPointHitInWorldSpace = pointHitInWorldSpace;
	    //nudgedPointHitInWorldSpace += ambientRay*.005f;
	    //if (!BVH_IntersectTriangles<true,true>(
	    //	    nudgedPointHitInWorldSpace, ambientRay, avoidSelf,
	    if (!BVH_IntersectTriangles<true,true>(
		    pointHitInWorldSpace, ambientRay, avoidSelf,
		    dummy, temp, kAB, kAB, kAB)) {
		// Accumulate contribution of this random ray
		totalLight += cosangle;
	    }
	}
	// total ambient light, averaged over all random rays
	color *= (AMBIENT/255.0)*(totalLight/maxLight);
#else
	// Dont calculate ambient occlusion, use the pre-calculated value from the model
	// (assuming it exists!)
	#ifdef USE_PHONG_NORMAL
	// we have a phong normal, so use the subtriangle areas
	// to interpolate the 3 ambientOcclusionCoeff values
	coord ambientOcclusionCoeff =
	    pBestTri->_vertexA->_ambientOcclusionCoeff*BCx/area +
	    pBestTri->_vertexB->_ambientOcclusionCoeff*CAx/area +
	    pBestTri->_vertexC->_ambientOcclusionCoeff*ABx/area;
	#else
	// we dont have a phong normal, just average the 3 values of the vertices
	coord ambientOcclusionCoeff = (
	    pBestTri->_vertexA->_ambientOcclusionCoeff +
	    pBestTri->_vertexB->_ambientOcclusionCoeff +
	    pBestTri->_vertexC->_ambientOcclusionCoeff)/3.f;
	#endif
	coord ambientFactor = (coord) ((AMBIENT*ambientOcclusionCoeff/255.0)/255.0);
	color *= ambientFactor;
#endif // AMBIENT_OCCLUSION

	// Now, for all the lights...
	for(unsigned i=0; i<scene._lights.size(); i++) {
	    Light& light = *scene._lights[i];

	    // This light's diffuse and specular contribution
	    Pixel dColor = Pixel(); // start with black

	    // We calculate the vector from point hit, to light (both in world space).
	    Vector3 pointToLight = light;
	    pointToLight -= pointHitInWorldSpace;

#ifdef USE_SHADOWS
	    // this is our distance from the light (squared, i.e. we didnt use an sqrt)
	    coord distanceFromLightSq = pointToLight.lengthsq();

	    Vector3 shadowrayInWorldSpace = pointToLight;
	    shadowrayInWorldSpace /= sqrt(distanceFromLightSq);

	    const Triangle *pDummy; // just to fill-in the param, not used for shadowrays
	    if (BVH_IntersectTriangles<true,doCulling>(
		pointHitInWorldSpace, shadowrayInWorldSpace, avoidSelf,
		pDummy, // dummy
		light,
		kAB, kAB, kAB)) // dummies
	    {
		continue; // we were in shadow, go to next light
	    }
#endif // USE_SHADOWS

	    // Diffuse color
	    pointToLight.normalize();  // vector from point to light (in world space)

	    coord intensity = dot(phongNormal, pointToLight);
	    if (intensity<0.) {
		; // in shadow, let it be in ambient
	    } else {
		Pixel diffuse = pBestTri->_colorf;
		diffuse *= (coord) (DIFFUSE*intensity/255.);   // diffuse set to a maximum of 130/255
		dColor += diffuse;
#ifndef RTCORETEST
		// Specular color
		// We will use the half vector: pointToLight + point to camera
		Vector3 pointToCamera = eye;
		pointToCamera -= pointHitInWorldSpace;
		pointToCamera.normalize();

		Vector3 half = pointToLight;
		half += pointToCamera;
		half.normalize();

		// use the interpolated phong normal!
		coord intensity2 = dot(half, phongNormal);
		if (intensity2>0.) {
		    intensity2 *= intensity2;
		    intensity2 *= intensity2;
		    intensity2 *= intensity2;
		    intensity2 *= intensity2;
		    intensity2 *= intensity2;
		    dColor += Pixel(
			(unsigned char)(SPECULAR*intensity2),
			(unsigned char)(SPECULAR*intensity2),
			(unsigned char)(SPECULAR*intensity2));
		}
#endif // RTCORETEST
	    }
	    color += dColor;
	}

#if defined(REFLECTIONS) || defined(REFRACTIONS)
	originInWorldSpace = pointHitInWorldSpace;
	Vector3& nrm = phongNormal;
	float c1 = -dot(rayInWorldSpace, nrm);
#endif

#ifdef REFLECTIONS
	// Reflections:
	//
	// ray = ray - 2 (ray dot normal) normal
	Vector3 reflectedRay = rayInWorldSpace;
	reflectedRay += nrm*(2.0f*c1);
	reflectedRay.normalize();
#endif // REFLECTIONS

	// Refractions:
	//
	// ray = ... (I use two "materials", toggling upon entry and exit depth
	// between n1 and n2, where n1 and n2 are...
#ifdef REFRACTIONS
	float n1 = 1.f + float(depth&1);
	float n2 = 2.f + float(depth&1);
	float n = n1/n2;
	float c2 = sqrt(1.f - n*n*(1.f - c1*c1));

	Vector3 refractedRay = rayInWorldSpace*n;
	refractedRay += nrm*(n*c1 - c2);
	refractedRay.normalize();
#endif // REFRACTIONS

	return
	    color
#ifdef REFLECTIONS
	    /* use backface culling for reflection rays: <true> */
	    + Raytrace<true>(originInWorldSpace, reflectedRay, avoidSelf, depth+1) * REFLECTIONS_RATE
#endif
#ifdef REFRACTIONS
	    /* Makes chessboard look much better
	    + (nrm._z>0.9 ?
		Pixel(0.,0.,0.) :
		Raytrace<false>(originInWorldSpace, refractedRay, avoidSelf, depth+1) * REFRACTIONS_RATE) */

	    /* dont use backface culling for refraction rays: <false> */
	    + Raytrace<false>(originInWorldSpace, refractedRay, avoidSelf, depth+1) * REFRACTIONS_RATE
#endif
	    ;
    }

    void RaytraceHorizontalSegment(int xStarting, int iOnePastEndingX) const
    {
#ifdef USE_OPENMP
	#pragma omp parallel for schedule(dynamic,10)
#endif
	for(int x=xStarting; x<iOnePastEndingX; x++) {
	    Pixel finalColor(0,0,0);

	    int pixelsTraced = 1;
	    if (antialias)
		pixelsTraced = 4;

	    while(pixelsTraced--) {
		// We will shoot a ray in camera space (from Eye to the screen point, so in camera
		// space, from (0,0,0) to this:
		coord xx = (coord)x;
		coord yy = (coord)y;

		if (antialias) {
		    // nudge in a cross pattern around the pixel center
		    xx += 0.25f - .5f*(pixelsTraced&1);
		    yy += 0.25f - .5f*((pixelsTraced&2)>>1);
		}
		coord lx = coord((HEIGHT/2)-yy)/SCREEN_DIST;
		coord ly = coord(xx-(WIDTH/2))/SCREEN_DIST;
		coord lz = 1.0;
		Vector3 rayInCameraSpace(lx,ly,lz);
		rayInCameraSpace.normalize();

		// We will need the origin in world space
		Vector3 originInWorldSpace = eye;

		// We have a rayInCameraSpace, and we want to use the BVH, which was constructed
		// in World space, so we convert the ray in World space
		Vector3 rayInWorldSpace = eye._mv._row1 * rayInCameraSpace._x;
		rayInWorldSpace += eye._mv._row2 * rayInCameraSpace._y;
		rayInWorldSpace += eye._mv._row3 * rayInCameraSpace._z;
		// in theory, this should not be required
		rayInWorldSpace.normalize();

		// Primary ray, we want backface culling: <true>
		finalColor += Raytrace<true>(originInWorldSpace, rayInWorldSpace, NULL, 0);
	    }
	    if (antialias)
		finalColor /= 4.;
	    if (finalColor._r>255.0f) finalColor._r=255.0f;
	    if (finalColor._g>255.0f) finalColor._g=255.0f;
	    if (finalColor._b>255.0f) finalColor._b=255.0f;
	    canvas.DrawPixel(y,x, SDL_MapRGB(
		canvas._surface->format, (Uint8)finalColor._r, (Uint8)finalColor._g, (Uint8)finalColor._b));
	}
    }

#ifdef USE_TBB
    // TBB expects functors, and this one simply delegates to DrawTriangles
    void operator()(const tbb::blocked_range<size_t>& r) const {
	RaytraceHorizontalSegment(r.begin(), r.end());
    }
#endif
};

int CountBoxes(BVHNode *root)
{
    if (!root->IsLeaf()) {
	BVHInner *p = dynamic_cast<BVHInner*>(root);
	return 1 + CountBoxes(p->_left) + CountBoxes(p->_right);
    } else
	return 1;
}

unsigned CountTriangles(BVHNode *root)
{
    if (!root->IsLeaf()) {
	BVHInner *p = dynamic_cast<BVHInner*>(root);
	return CountTriangles(p->_left) + CountTriangles(p->_right);
    } else {
	BVHLeaf *p = dynamic_cast<BVHLeaf*>(root);
	return (unsigned) p->_triangles.size();
    }
}

void CountDepth(BVHNode *root, int depth, int& maxDepth)
{
    if (maxDepth<depth)
	maxDepth=depth;
    if (!root->IsLeaf()) {
	BVHInner *p = dynamic_cast<BVHInner*>(root);
	CountDepth(p->_left, depth+1, maxDepth);
	CountDepth(p->_right, depth+1, maxDepth);
    }
}

void Scene::PopulateCacheFriendlyBVH(
    Triangle *pFirstTriangle,
    BVHNode *root,
    unsigned& idxBoxes,
    unsigned& idxTriList)
{
    unsigned currIdxBoxes = idxBoxes;
    _pCFBVH[currIdxBoxes]._bottom = root->_bottom;
    _pCFBVH[currIdxBoxes]._top    = root->_top;
    if (!root->IsLeaf()) {
	BVHInner *p = dynamic_cast<BVHInner*>(root);
	int idxLeft = ++idxBoxes;
	PopulateCacheFriendlyBVH(pFirstTriangle, p->_left, idxBoxes, idxTriList);
	int idxRight = ++idxBoxes;
	PopulateCacheFriendlyBVH(pFirstTriangle, p->_right, idxBoxes, idxTriList);
	_pCFBVH[currIdxBoxes].u.inner._idxLeft  = idxLeft;
	_pCFBVH[currIdxBoxes].u.inner._idxRight = idxRight;
    } else {
	BVHLeaf *p = dynamic_cast<BVHLeaf*>(root);
	unsigned count = (unsigned) p->_triangles.size();
	_pCFBVH[currIdxBoxes].u.leaf._count = 0x80000000 | count;
	_pCFBVH[currIdxBoxes].u.leaf._startIndexInTriIndexList = idxTriList;
	for(std::list<const Triangle*>::iterator it=p->_triangles.begin();
	    it != p->_triangles.end();
	    ++it)
	{
	    _triIndexList[idxTriList++] = *it - pFirstTriangle;
	}
    }
}

void Scene::CreateCFBVH()
{
    if (!_pSceneBVH) {
	puts("Internal bug in CreateCFBVH, please report it..."); fflush(stdout);
	exit(1);
    }

    unsigned idxTriList=0;
    unsigned idxBoxes=0;

    _triIndexListNo = CountTriangles(_pSceneBVH);
    _triIndexList = new int[_triIndexListNo];

    _pCFBVH_No = CountBoxes(_pSceneBVH);
    _pCFBVH = new CacheFriendlyBVHNode[_pCFBVH_No];

    PopulateCacheFriendlyBVH(
	&_triangles[0],
	_pSceneBVH,
	idxBoxes,
	idxTriList);

    if ((idxBoxes != _pCFBVH_No-1) || (idxTriList != _triIndexListNo)) {
	puts("Internal bug in CreateCFBVH, please report it..."); fflush(stdout);
	exit(1);
    }

    int maxDepth = 0;
    CountDepth(_pSceneBVH, 0, maxDepth);
    if (maxDepth>=BVH_STACK_SIZE) {
	printf("Max depth of BVH was %d\n", maxDepth);
	puts("Recompile with BVH_STACK_SIZE set to more than that..."); fflush(stdout);
	exit(1);
    }
}

void Scene::UpdateBoundingVolumeHierarchy(const char *filename, bool forceRecalc)
{
    if (!_pCFBVH) {
	std::string BVHcacheFilename(filename);
	BVHcacheFilename += ".bvh";
	FILE *fp = fopen(BVHcacheFilename.c_str(), "rb");
	if (forceRecalc || !fp) {
	    // No cached BVH data - we need to calculate them
	    Clock me;
	    _pSceneBVH = CreateBVH(this);
	    printf("Building the BVH%s took %.2f seconds\n",
		#ifdef SIMD_SSE
		" with SSE",
		#else
		"",
		#endif
		me.readMS()/1000.);

	    // Now that the BVH has been created, copy its data into a more cache-friendly format
	    // (CacheFriendlyBVHNode occupies exactly 32 bytes, i.e. a cache-line)
	    CreateCFBVH();

	    // Now store the results, if possible...

	    // if forceRecalc is in effect, close the old file
	    if (fp) fclose(fp);

	    fp = fopen(BVHcacheFilename.c_str(), "wb");
	    if (!fp) return;
	    if (1 != fwrite(&_pCFBVH_No, sizeof(unsigned), 1, fp)) { fclose(fp); return; }
	    if (1 != fwrite(&_triIndexListNo, sizeof(unsigned), 1, fp)) { fclose(fp); return; }
	    if (_pCFBVH_No != fwrite(_pCFBVH, sizeof(CacheFriendlyBVHNode), _pCFBVH_No, fp)) { fclose(fp); return; }
	    if (_triIndexListNo != fwrite(_triIndexList, sizeof(int), _triIndexListNo, fp)) { fclose(fp); return; }
	    fclose(fp);
	} else {
	    puts("Cache exists, reading the pre-calculated BVH data...");
	    if (1 != fread(&_pCFBVH_No, sizeof(unsigned), 1, fp)) {
		UpdateBoundingVolumeHierarchy(filename, true);
		return;
	    }
	    if (1 != fread(&_triIndexListNo, sizeof(unsigned), 1, fp)) {
		UpdateBoundingVolumeHierarchy(filename, true);
		return;
	    }
	    _pCFBVH = new CacheFriendlyBVHNode[_pCFBVH_No];
	    _triIndexList = new int[_triIndexListNo];
	    if (_pCFBVH_No != fread(_pCFBVH, sizeof(CacheFriendlyBVHNode), _pCFBVH_No, fp)) {
		delete [] _pCFBVH;
		delete [] _triIndexList;
		_pCFBVH = NULL;
		_triIndexList = NULL;
		UpdateBoundingVolumeHierarchy(filename, true);
		return;
	    }
	    if (_triIndexListNo != fread(_triIndexList, sizeof(int), _triIndexListNo, fp)) {
		delete [] _pCFBVH;
		delete [] _triIndexList;
		_pCFBVH = NULL;
		_triIndexList = NULL;
		UpdateBoundingVolumeHierarchy(filename, true);
		return;
	    }
	    fclose(fp);
	}
    }
}

bool Scene::renderRaytracer(Camera& eye, Screen& canvas, bool antialias)
{
    bool needToUpdateTitleBar = !_pSceneBVH; // see below

    // Update the BVH and its cache-friendly version
    extern const char *g_filename;
    UpdateBoundingVolumeHierarchy(g_filename);

    if (needToUpdateTitleBar) {
	// A BVH calculation just occured, the title bar is still saying:
	//    "Creating Bounding Volume Hierarchy data... (SAH/AABB)"
	// Update it, we are now raytracing...
	const char *modeMsg;
	if (antialias)
	    modeMsg = "Raytracing with antialiasing";
	else
	    modeMsg = "Raytracing";
	SDL_WM_SetCaption(modeMsg, modeMsg);
    }

    Keyboard keys;

    // Main loop: for each pixel...
    for(int y=0; y<HEIGHT; y++) {
#ifdef USE_TBB
	// For TBB, use the parallel_for construct.
	// Different threads will execute for segments of the current scanline,
	// calling the operator(), which in turn calls RaytraceHorizontalSegment for the segment.
	// We use the third parameter of parallel_for to chop the triangle list down
	// into batches of 10 horizontal pixels - the dynamic scheduler will feed these batches
	// to our threads, keeping them busy (just like schedule(dynamic,10) for OpenMP)
	if (antialias)
	    tbb::parallel_for(
		tbb::blocked_range<size_t>(0, WIDTH, 10),
		RaytraceScanline<true>(*this, eye, canvas, y) );
	else
	    tbb::parallel_for(
		tbb::blocked_range<size_t>(0, WIDTH, 10),
		RaytraceScanline<false>(*this, eye, canvas, y) );
#else
	// For both OpenMP and single-threaded, call the RaytraceHorizontalSegment member
	// of the RaytraceScanline, requesting drawing of ALL the scanline.
	// For OpenMP, the appropriate pragma inside RaytraceHorizontalSegment will make it execute via SMP...
	if (antialias)
	    RaytraceScanline<true>(*this, eye, canvas, y).RaytraceHorizontalSegment(0, WIDTH);
	else
	    RaytraceScanline<false>(*this, eye, canvas, y).RaytraceHorizontalSegment(0, WIDTH);
#endif

#ifdef HANDLERAYTRACER
	// Since raytracing takes time, allow the user to abort:
	extern bool g_benchmark;
	keys.poll(false); // false=no yielding, we want speed!
	if (keys._isAbort) {
	    while(keys._isAbort) keys.poll(false);
	    if (!g_benchmark)
		return false;
	    else
		exit(1);
	}

	// And every 16 scanlines, show the buffer...
	if (15 == (y&15)) {
	    std::stringstream percentage;
	    if (antialias) percentage << "Anti-aliased r"; else percentage << "R";
	    percentage << "aytracing... hit ESCAPE to abort (" << int(100.*y/HEIGHT) << "%)";
	    static char asyncBufferForCaption[256];
	    strcpy(asyncBufferForCaption, percentage.str().c_str());
	    SDL_WM_SetCaption(asyncBufferForCaption, asyncBufferForCaption);
	    canvas.ShowScreen(true,false);
	}
#endif
    }
    canvas.ShowScreen(true,true);
    return true;
}
