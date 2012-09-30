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

#ifndef __LIGHTING_H__
#define __LIGHTING_H__

#include <assert.h>

#include "Defines.h"
#include "Types.h"
#include "Scene.h"

enum LightingMode {
    NoShadows,
    ShadowMapping,
    SoftShadowMapping
};

template <LightingMode lightingMode>
class LightingEquation {
    const Scene& _scene;
public:
    LightingEquation(
	const Scene& scene)
	:
	_scene(scene)
	{}

    void ComputePixel(
	const Vector3& inCameraSpace,
	const Vector3& normalInCameraSpace,
	const Pixel& material,
	const coord ambientOcclusionCoeff,
	Pixel& target)
    {
	// First, add the ambient component...
	target = material;
	coord ambient = (coord) ((AMBIENT*ambientOcclusionCoeff/255.0)/255.0);
	target *= ambient;

	for(unsigned i=0; i<_scene._lights.size(); i++) {

	    Light& light = *_scene._lights[i];

	    // This light's diffuse and specular contribution
	    Pixel dColor = Pixel(); // start with black

	    // We calculate the vector from point to light (in camera space).
	    // The camera-space light coordinates are already precalculated
	    // (in Light::CalculatePositionInCameraSpace, called from main loop)
	    Vector3 pointToLight = light._inCameraSpace;
	    pointToLight -= inCameraSpace;

	    int cntInShadow=0;

	    // Compile-time check (template param)
	    if (lightingMode != NoShadows) {

		// We will use the precalculated (Light::CalculateXformFromCameraToLightSpace)
		// Matrix3 that allows us to go directly from camera space to light space.
		// We need the vector from light to point:
		Vector3 lightToPoint = pointToLight;
		lightToPoint*=-1;

		// ...and we transform it to light space coordinates:
		Vector3 inLightSpace = light._cameraToLightSpace.multiplyRightWith(lightToPoint);

		// In light space, we check the Shadowmap Z-value:
		inLightSpace._x = SHADOWMAPSIZE/2  + SHADOWMAPSIZE*2*inLightSpace._x/inLightSpace._z;
		inLightSpace._y = SHADOWMAPSIZE/2 + SHADOWMAPSIZE*2*inLightSpace._y/inLightSpace._z;
		inLightSpace._z = 1.0f/inLightSpace._z;
		int sx = (int) inLightSpace._x;
		int sy = (int) inLightSpace._y;

		if (lightingMode == ShadowMapping) { // Compile-time check (template param)

		    if ((sx<0) || (sx>=SHADOWMAPSIZE) || (sy<0) || (sy>=SHADOWMAPSIZE))
			// ooops, our shadow map doesn't cover this...
			// (We should emit warning here...)
			continue;

		    if (!(light._shadowBuffer[sy][sx] < (inLightSpace._z+0.001)))
			// in shadow, try next light
			continue;

		} else if (lightingMode == SoftShadowMapping) { // Soft shadows (compile-time check, template param)
		    int basex=sx, basey=sy;
		    for(int d=-1;d<=1;d++) {
			sy = basey + d;
			if ((sy<0) || (sy>=(int)SHADOWMAPSIZE))
			    continue;
			for(int e=-1;e<=1;e++) {
			    sx = basex + e;
			    if ((sx<0) || (sx>=(int)SHADOWMAPSIZE))
				continue;

			    if (light._shadowBuffer[sy][sx] > (inLightSpace._z+0.001))
				cntInShadow++;
			}
		    }
		    // OK, we've now measured how many (cntInShadow) shadow pixels exist around us
		    // cntInShadow/9 is the percentage of shadowing...
		}
	    }

	    // Diffuse color
	    pointToLight.normalize();  // vector from point to light (in camera space)
	    coord intensity = dot(normalInCameraSpace, pointToLight);
	    if (intensity<0.) {
		; // in shadow, let it be in ambient
	    } else {
		Pixel diffuse = material;
		diffuse *= (coord) (DIFFUSE*intensity/255.);   // diffuse set to a maximum of 130/255
		dColor += diffuse;

		// Specular color
		// We will use the half vector: pointToLight + point to camera
		// Since 'point' now has camera space coordinates, -1.0*point is in fact,
		// a vector from point to camera.
		Vector3 pointToCamera = inCameraSpace;
		pointToCamera *= -1.;
		pointToCamera.normalize();

		Vector3 half = pointToLight;
		half += pointToCamera;
		half.normalize();

		coord intensity2 = dot(half, normalInCameraSpace);
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
	    }

	    // Compile-time check (template params)
	    if (lightingMode == SoftShadowMapping) {
		if (cntInShadow)
		    dColor *= (9.0f-cntInShadow)/9.0f;
	    }

	    target += dColor;
	}

	if (target._b>255) target._b = 255;
	if (target._g>255) target._g = 255;
	if (target._r>255) target._r = 255;
    }
};


#endif
