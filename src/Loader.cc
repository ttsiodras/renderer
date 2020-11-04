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

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <fstream>
#include <cfloat>
#include <map>

#include <string.h>
#include <assert.h>

#include "lib3ds/types.h"
#include "lib3ds/material.h"
#include "lib3ds/mesh.h"
#include "lib3ds/file.h"

#include "3d.h"
#include "Defines.h"
#include "Exceptions.h"

using std::string;

namespace enums {
    enum ColorComponent {
        Red = 0,
        Green = 1,
        Blue = 2
    };
}

using namespace enums;

struct MaterialColors {

    MaterialColors(
        const Lib3dsRgba& ambient, const Lib3dsRgba& diffuse, const Lib3dsRgba& specular, bool twoSided)
        :
        _ambient(ambient), _diffuse(diffuse), _specular(specular), _twoSided(twoSided)
        {}

    template <ColorComponent c>
    unsigned getByteAmbient() const { return unsigned(255.0*_ambient[c]); }

    template <ColorComponent c>
    unsigned getByteDiffuse() const { return unsigned(255.0*_diffuse[c]); }

    template <ColorComponent c>
    unsigned getByteSpecularRed() const { return unsigned(255.0*_specular[c]); }

    const Lib3dsRgba& _ambient;
    const Lib3dsRgba& _diffuse;
    const Lib3dsRgba& _specular;
    bool _twoSided;
};

const coord Scene::MaxCoordAfterRescale = 1.2f;

void Scene::load(const char *filename)
{
    if (filename[0] == '@' && filename[1] == 'p') {     // Platform
        _vertices.reserve(4);
        _vertices.push_back(Vertex( 0.5, -0.5, 0.,  0.,0.,1.));
        _vertices.push_back(Vertex( 0.5,  0.5, 0.,  0.,0.,1.));
        _vertices.push_back(Vertex(-0.5,  0.5, 0.,  0.,0.,1.));
        _vertices.push_back(Vertex(-0.5, -0.5, 0.,  0.,0.,1.));
        _triangles.reserve(2);
        _triangles.push_back(Triangle(&_vertices[0], &_vertices[1], &_vertices[2], 255,0,0, _catchallMesh));
        _triangles.push_back(Triangle(&_vertices[0], &_vertices[2], &_vertices[3], 255,0,0, _catchallMesh));
        return;

    }

    const char *dt = strrchr(filename, '.');
    if (dt) {
        dt++;
        if (!strcmp(dt, "tri")) {

            // Simple binary format:
            //
            // Basically:
            //    <magic (uint32)>  TRI_MAGIC, TRI_MAGICNORMAL or missing
            //    one or more of these blocks:
            //        no_of_vertices (uint32)
            //        x1,y1,z1 (float coordinates of vertex)
            //        ...
            //        xn,yn,zn (float coordinates of vertex)
            //        (magic == TRI_MAGICNORMAL)? nx1, ny1, nz1 (float coordinates of normal)
            //        no_of_triangles (uint32)
            //        idx,idx,idx (uint32 indices into the vertex array)
            //        (magic == TRI_MAGIC | TRI_MAGICNORMAL)? r,g,b (floats)
            //        ...
            //        idx,idx,idx (uint32 indices into the vertex array)
            //        (magic == TRI_MAGIC | TRI_MAGICNORMAL)? r,g,b (floats)
            FILE *fp = fopen(filename, "rb");
            if (!fp)
                THROW((string("File '") + string(filename) + string("' not found!")).c_str());

            Uint32 totalPoints = 0, totalTris = 0;
            Uint32 magic;

            fread(&magic, 1, sizeof(Uint32), fp);
            if (magic != TRI_MAGIC && magic != TRI_MAGICNORMAL) {
                // No magic, just vertices and points (no normals, no colors)
                fseek(fp, 0, SEEK_SET);
            }

            // Calculate total number of vertices in order to reserve the vectors memory
            unsigned currentOffset = ftell(fp);
            unsigned currentTotalPoints = 0;
            unsigned currentTotalTris = 0;
            while(1) {
                unsigned temp;
                fread(&temp, 1, sizeof(Uint32), fp);
                if (feof(fp))
                    break;
                currentTotalPoints += temp;
                fseek(fp, temp*(magic==TRI_MAGICNORMAL?24:12), SEEK_CUR);
                fread(&temp, 1, sizeof(Uint32), fp);
                if (feof(fp))
                    break;
                currentTotalTris += temp;
                fseek(fp, temp*24, SEEK_CUR);
            }

            // Reserve the space, now that you know
            if (magic == TRI_MAGICNORMAL || magic == TRI_MAGIC)
                _vertices.reserve(currentTotalPoints);
            else
                // when we have no normals, we'll need extra space for fix_normals()...
                _vertices.reserve(currentTotalTris*3);
            _triangles.reserve(currentTotalTris);

            // Now load them inside the std::vectors...
            fseek(fp, currentOffset, SEEK_SET);
            do {
                Uint32 noOfPoints;
                fread(&noOfPoints, 1, sizeof(Uint32), fp);
                if (feof(fp))
                    break;

                for(Uint32 i=0; i<noOfPoints; i++) {
                    float x,y,z;
                    fread(&x,1,4,fp); if (feof(fp)) { THROW("Malformed 3D file"); }
                    fread(&y,1,4,fp); if (feof(fp)) { THROW("Malformed 3D file"); }
                    fread(&z,1,4,fp); if (feof(fp)) { THROW("Malformed 3D file"); }

                    float nx=0.,ny=0.,nz=0.;
                    if (magic == TRI_MAGICNORMAL) {
                        fread(&nx,1,4,fp); if (feof(fp)) { THROW("Malformed 3D file"); }
                        fread(&ny,1,4,fp); if (feof(fp)) { THROW("Malformed 3D file"); }
                        fread(&nz,1,4,fp); if (feof(fp)) { THROW("Malformed 3D file"); }
                    } else {
                        nx = ny = nz = 0; // Will be calculated in fix_normals()
                    }
                    assert(_vertices.size() < _vertices.capacity());
                    _vertices.push_back(Vertex(x,y,z, nx,ny,nz));
                }

                Uint32 noOfTris;
                fread(&noOfTris, 1, sizeof(Uint32), fp);
                if (feof(fp)) { THROW("Malformed 3D file"); }

                for(Uint32 i=0; i<noOfTris; i++) {
                    Uint32 idx1,idx2,idx3;
                    fread(&idx1,1,4,fp); if (feof(fp)) { THROW("Malformed 3D file"); }
                    fread(&idx2,1,4,fp); if (feof(fp)) { THROW("Malformed 3D file"); }
                    fread(&idx3,1,4,fp); if (feof(fp)) { THROW("Malformed 3D file"); }
                    if (idx1>=(totalPoints+noOfPoints)) { THROW("Malformed 3D file (idx1)"); }
                    if (idx2>=(totalPoints+noOfPoints)) { THROW("Malformed 3D file (idx2)"); }
                    if (idx3>=(totalPoints+noOfPoints)) { THROW("Malformed 3D file (idx3)"); }

                    float r,g,b;
                    if (magic == TRI_MAGIC || magic == TRI_MAGICNORMAL) {
                        fread(&r,1,4,fp); if (feof(fp)) { THROW("Malformed 3D file"); }
                        fread(&g,1,4,fp); if (feof(fp)) { THROW("Malformed 3D file"); }
                        fread(&b,1,4,fp); if (feof(fp)) { THROW("Malformed 3D file"); }
                        r*=255.; g*=255.; b*=255.;
                    } else {
                        r = g = b = 255.0; // No colors? White, then... :-(
                    }

                    _triangles.push_back(
                        Triangle(
                            &_vertices[idx1], &_vertices[idx2], &_vertices[idx3],
                            unsigned(r),unsigned(g),unsigned(b), _catchallMesh));
                }

                totalPoints += noOfPoints;
                totalTris   += noOfTris;
            } while(!feof(fp));

            fclose(fp);
            if (magic != TRI_MAGICNORMAL)
                fix_normals();

        } else if (!strcmp(dt, "ra2")) {
            // http://ompf.org/forum/viewtopic.php?t=64
            FILE *fp = fopen(filename, "rb");
            if (!fp)
                THROW((string("File '") + string(filename) + string("' not found!")).c_str());

            Uint32 totalPoints = 0, totalTriangles = 0;

            fseek(fp, 0, SEEK_END);
            totalTriangles = ftell(fp)/36;
            totalPoints = 3*totalTriangles;
            fseek(fp, 0, SEEK_SET);

            _vertices.reserve(totalPoints);
            _triangles.reserve(totalTriangles);

            // Now load them inside the std::vectors...
            for(Uint32 i=0; i<totalPoints; i++) {
                float x,y,z;
                fread(&y,1,4,fp); if (feof(fp)) { fclose(fp); THROW("Malformed 3D file"); }
                fread(&z,1,4,fp); if (feof(fp)) { fclose(fp); THROW("Malformed 3D file"); }
                fread(&x,1,4,fp); if (feof(fp)) { fclose(fp); THROW("Malformed 3D file"); }

                float nx=0.,ny=0.,nz=0.;
                assert(_vertices.size() < _vertices.capacity());
                _vertices.push_back(Vertex(x,y,z, nx,ny,nz));
            }

            for(Uint32 i=0; i<totalTriangles; i++) {
                Uint32 idx1,idx2,idx3;
                idx1 = 3*i + 0;
                if (getenv("RA2")!=NULL) {
                    idx2 = 3*i + 2;
                    idx3 = 3*i + 1;
                } else {
                    idx2 = 3*i + 1;
                    idx3 = 3*i + 2;
                }
                if (idx1>=(totalPoints)) { fclose(fp); THROW("Malformed 3D file (idx1)"); }
                if (idx2>=(totalPoints)) { fclose(fp); THROW("Malformed 3D file (idx2)"); }
                if (idx3>=(totalPoints)) { fclose(fp); THROW("Malformed 3D file (idx3)"); }

                float r,g,b;
                r = g = b = 255.0; // No colors? White, then... :-(

                _triangles.push_back(
                    Triangle(
                        &_vertices[idx1], &_vertices[idx2], &_vertices[idx3],
                        unsigned(r),unsigned(g),unsigned(b), _catchallMesh));
            }
            fclose(fp);
            fix_normals();
        } else if (!strcmp(dt, "3ds") || !strcmp(dt, "3DS")) {
            int i = 0;
            Lib3dsFile* p3DS = lib3ds_file_load(filename);
            if (!p3DS)
                THROW("Lib3DS couldn't load this .3ds file");
            lib3ds_file_eval(p3DS, 0);

            Lib3dsDword currentTotalTris = 0;
            Lib3dsMesh *pMesh = p3DS->meshes;
            if (!pMesh)
                THROW("This .3ds file has no meshes")
            std::map<Lib3dsMesh*, Lib3dsVector*> normals;
            while(pMesh) {
                currentTotalTris   += pMesh->faces;
                Lib3dsVector *pMeshNormals = new Lib3dsVector[3*pMesh->faces];
                lib3ds_mesh_calculate_normals(pMesh, pMeshNormals);
                normals[pMesh] = pMeshNormals;
                pMesh = pMesh->next;
            }
            _vertices.reserve(3*currentTotalTris);
            _triangles.reserve(currentTotalTris);
            std::map<string, MaterialColors> colors;
            Lib3dsMaterial *pMaterial = p3DS->materials;
            while(pMaterial) {
                colors.insert(
                    std::map<string, MaterialColors>::value_type(
                        string(pMaterial->name),
                        MaterialColors(pMaterial->ambient, pMaterial->diffuse,  pMaterial->specular, pMaterial->two_sided!=0)));
                pMaterial = pMaterial->next;
            }
            pMesh = p3DS->meshes;
            int currentTotalPoints = 0;
            while(pMesh) {
                Mesh *p_tmpMesh = new Mesh();
                if (!pMesh->pointL) { pMesh=pMesh->next; continue; }
                if (!pMesh->faceL)  { pMesh=pMesh->next; continue; }
                for(i=0; i<(int)pMesh->faces; i++) {
                    std::map<string, MaterialColors>::iterator pMat = colors.find(string(pMesh->faceL[i].material));
                    unsigned r, g, b;
                    if (pMat != colors.end()) {
                        r = pMat->second.getByteDiffuse<Red>();
                        g = pMat->second.getByteDiffuse<Green>();
                        b = pMat->second.getByteDiffuse<Blue>();
                    } else {
                        r = g = b = 255;
                    }
                    for (int k=0; k<3; k++) {
                        int pointIdx = pMesh->faceL[i].points[k];
                        Vector3 nr(
                            normals[pMesh][3*i+k][0],
                            normals[pMesh][3*i+k][1],
                            normals[pMesh][3*i+k][2]);
                        assert(_vertices.size() < _vertices.capacity());
                        _vertices.push_back(
                            Vertex(
                                /*p3DS->master_scale* */pMesh->pointL[pointIdx].pos[0],
                                /*p3DS->master_scale* */pMesh->pointL[pointIdx].pos[1],
                                /*p3DS->master_scale* */pMesh->pointL[pointIdx].pos[2],
                                nr._x,
                                nr._y,
                                nr._z));
                    }
                    assert(_triangles.size() < _triangles.capacity());
                    _triangles.push_back(
                        Triangle(
                            &_vertices[currentTotalPoints + 3*i],
                            &_vertices[currentTotalPoints + 3*i + 1],
                            &_vertices[currentTotalPoints + 3*i + 2],
                            r, g, b,
                            p_tmpMesh,
                            pMat->second._twoSided,
                            true,
                            Vector3(pMesh->faceL[i].normal[0],
                                    pMesh->faceL[i].normal[1],
                                    pMesh->faceL[i].normal[2]) ));
                }
                currentTotalPoints += 3*pMesh->faces;
                pMesh = pMesh->next;
            }
        } else if (!strcmp(dt, "PLY") || !strcmp(dt, "ply")) {
            // Only shadevis generated objects, not full blown parser!
            std::ifstream file(filename, std::ios::in);
            if (!file) {
                THROW((string("Missing ")+string(filename)).c_str());
            }
            string line;
            unsigned totalVertices=0, totalTriangles=0, lineNo=0;
            bool inside = false;
            while(getline(file, line)) {
                lineNo++;
                if (!inside) {
                    if (line.substr(0, 14) == "element vertex") {
                        std::istringstream str(line);
                        string word1;
                        str >> word1;
                        str >> word1;
                        str >> totalVertices;
                        _vertices.reserve(totalVertices);
                    } else if (line.substr(0, 12) == "element face") {
                        std::istringstream str(line);
                        string word1;
                        str >> word1;
                        str >> word1;
                        str >> totalTriangles;
                    } else if (line.substr(0, 10) == "end_header")
                        inside = true;
                } else {
                    if (totalVertices) {
                        totalVertices--;
                        float x,y,z;
                        unsigned ambientOcclusionCoeff;
                        std::istringstream str(line);
                        str >> x >> y >> z >> ambientOcclusionCoeff;
                        _vertices.push_back(Vertex(x,y,z,0.,0.,0.,ambientOcclusionCoeff));
                    } else if (totalTriangles) {
                        totalTriangles--;
                        unsigned dummy, idx1, idx2, idx3;
                        std::istringstream str(line);
                        if (str >> dummy >> idx1 >> idx2 >> idx3)
                        {
                            unsigned r, g, b;
                            if (str >> r >> g >> b)
                                ;
                            else {
                                r = 255; g = 255; b = 255;
                            }
                            _triangles.push_back(
                                Triangle(
                                    &_vertices[idx1], &_vertices[idx2], &_vertices[idx3],
                                    r, g, b, _catchallMesh));
                        }
                    }
                }
            }
            fix_normals();
        } else
            THROW("Unknown extension (only .tri .3ds or .ply accepted)");
    } else
        THROW("No extension in filename (only .tri .3ds or .ply accepted)");

    std::cout << "Vertexes: " << _vertices.size();
    std::cout << " Triangles: " << _triangles.size() << std::endl;

    // Center scene at world's center

    Vector3 minp(FLT_MAX,FLT_MAX,FLT_MAX), maxp(-FLT_MAX,-FLT_MAX,-FLT_MAX);
    for(unsigned i=0; i<_triangles.size(); i++) {
        minp.assignSmaller(*_triangles[i]._vertexA);
        minp.assignSmaller(*_triangles[i]._vertexB);
        minp.assignSmaller(*_triangles[i]._vertexC);
        maxp.assignBigger(*_triangles[i]._vertexA);
        maxp.assignBigger(*_triangles[i]._vertexB);
        maxp.assignBigger(*_triangles[i]._vertexC);
    }
    Vector3 origCenter = Vector3(
        (maxp._x+minp._x)/2,
        (maxp._y+minp._y)/2,
        (maxp._z+minp._z)/2);

    minp -= origCenter;
    maxp -= origCenter;

    // Scale scene so max(abs x,y,z coordinates) = MaxCoordAfterRescale

    coord maxi = 0;
    maxi = std::max(maxi, (coord) fabs(minp._x));
    maxi = std::max(maxi, (coord) fabs(minp._y));
    maxi = std::max(maxi, (coord) fabs(minp._z));
    maxi = std::max(maxi, (coord) fabs(maxp._x));
    maxi = std::max(maxi, (coord) fabs(maxp._y));
    maxi = std::max(maxi, (coord) fabs(maxp._z));

    for(unsigned i=0; i<_vertices.size(); i++) {
        _vertices[i] -= origCenter;
        _vertices[i] *= MaxCoordAfterRescale/maxi;
    }
    for(unsigned i=0; i<_triangles.size(); i++) {
        _triangles[i]._center -= origCenter;
        _triangles[i]._center *= MaxCoordAfterRescale/maxi;
    }
    // Update triangle bounding boxes (used by BVH)
    for(unsigned i=0; i<_triangles.size(); i++) {
        _triangles[i]._bottom.assignSmaller(*_triangles[i]._vertexA);
        _triangles[i]._bottom.assignSmaller(*_triangles[i]._vertexB);
        _triangles[i]._bottom.assignSmaller(*_triangles[i]._vertexC);
        _triangles[i]._top.assignBigger(*_triangles[i]._vertexA);
        _triangles[i]._top.assignBigger(*_triangles[i]._vertexB);
        _triangles[i]._top.assignBigger(*_triangles[i]._vertexC);
    }
    // Pre-compute triangle intersection data (used by raytracer)
    for(unsigned i=0; i<_triangles.size(); i++) {
        Triangle& triangle = _triangles[i];

        // Algorithm for triangle intersection is taken from Roman Kuchkuda's paper.
        // edge vectors
        Vector3 vc1=*triangle._vertexB; vc1-=*triangle._vertexA;
        Vector3 vc2=*triangle._vertexC; vc2-=*triangle._vertexB;
        Vector3 vc3=*triangle._vertexA; vc3-=*triangle._vertexC;

        // plane of triangle
        triangle._normal = cross(vc1, vc2); 
        Vector3 alt1 = cross(vc2, vc3);
        if (alt1.length() > triangle._normal.length()) triangle._normal = alt1;
        Vector3 alt2 = cross(vc3, vc1);
        if (alt2.length() > triangle._normal.length()) triangle._normal = alt2;
        triangle._normal.normalize();
        triangle._d = dot(triangle._normal, *triangle._vertexA);

        // edge planes
        triangle._e1 = cross(triangle._normal, vc1); 
        triangle._e1.normalize();
        triangle._d1 = dot(triangle._e1, *triangle._vertexA);
        triangle._e2 = cross(triangle._normal, vc2); 
        triangle._e2.normalize();
        triangle._d2 = dot(triangle._e2, *triangle._vertexB);
        triangle._e3 = cross(triangle._normal, vc3); 
        triangle._e3.normalize();
        triangle._d3 = dot(triangle._e3, *triangle._vertexC);
    }
}

void Scene::fix_normals(void)
{
    for(unsigned j=0; j<_triangles.size(); j++) {
        Vector3 worldPointA = *_triangles[j]._vertexA;
        Vector3 worldPointB = *_triangles[j]._vertexB;
        Vector3 worldPointC = *_triangles[j]._vertexC;
        Vector3 AB = worldPointB;
        AB -= worldPointA;
        Vector3 AC = worldPointC;
        AC -= worldPointA;
        Vector3 cr = cross(AB, AC);
        cr.normalize();
        _triangles[j]._normal = cr;
        const_cast<Vertex*>(_triangles[j]._vertexA)->_normal += cr;
        const_cast<Vertex*>(_triangles[j]._vertexB)->_normal += cr;
        const_cast<Vertex*>(_triangles[j]._vertexC)->_normal += cr;
    }
    for(unsigned j=0; j<_triangles.size(); j++) {
        const_cast<Vertex*>(_triangles[j]._vertexA)->_normal.normalize();
        const_cast<Vertex*>(_triangles[j]._vertexB)->_normal.normalize();
        const_cast<Vertex*>(_triangles[j]._vertexC)->_normal.normalize();
    }
}

