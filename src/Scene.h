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

#ifndef __scene_h__
#define __scene_h__

#include <list>

#include "Base3d.h"
#include "BVH.h"

struct Light;
struct Screen;
struct Camera;

struct Scene {
    static const coord MaxCoordAfterRescale;

    std::vector<Vertex>    _vertices;
    std::vector<Triangle>  _triangles;
    std::vector<Light*>	   _lights;

    // Bounding Volume Hierarchy
    BVHNode *_pSceneBVH;

    // Cache-friendly version of the Bounding Volume Hierarchy data
    // (32 bytes per CacheFriendlyBVHNode, i.e. one CPU cache line)
    unsigned _triIndexListNo;
    int *_triIndexList;
    unsigned _pCFBVH_No;
    CacheFriendlyBVHNode *_pCFBVH;

    Scene()
	:
	_pSceneBVH(NULL),
	_triIndexListNo(0),
	_triIndexList(NULL),
	_pCFBVH_No(0),
	_pCFBVH(NULL)
	{}

    // Load object
    void load(const char *filename);

    // Update triangle normals
    void fix_normals(void);

    // Cache-friendly version of the Bounding Volume Hierarchy data
    // (creation functions)
    void PopulateCacheFriendlyBVH(
	Triangle *pFirstTriangle,
	BVHNode *root,
	unsigned& idxBoxes,
	unsigned& idxTriList);
    void CreateCFBVH();

    // Creates BVH and Cache-friendly version of BVH
    void UpdateBoundingVolumeHierarchy(const char *filename, bool forceRecalc=false);

    void renderPoints(const Camera&, Screen&, bool asTriangles = true);
    void renderWireframe(const Camera&, Screen&);
    void renderAmbient(const Camera&, Screen&);
    void renderGouraud(const Camera&, Screen&);
    void renderPhong(const Camera&, Screen&);
    void renderPhongAndShadowed(const Camera&, Screen&);
    void renderPhongAndSoftShadowed(const Camera&, Screen&);

    // Since it may be aborted for taking too long, this returns "boolCompletedOK"
    bool renderRaytracer(Camera&, Screen&, bool antiAlias = false);
};

#endif
