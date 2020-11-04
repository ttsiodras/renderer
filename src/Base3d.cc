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

#include <cfloat>

#include <SDL.h>

#include "3d.h"
#include "Screen.h"

Triangle::Triangle(
    const Vertex *vertexA,
    const Vertex *vertexB,
    const Vertex *vertexC,
    unsigned r, unsigned g, unsigned b,
    Mesh *pMesh,
    bool twosided, bool triNormalProvided, Vector3 triNormal)
    :
    _vertexA(vertexA), _vertexB(vertexB), _vertexC(vertexC),

    _center((vertexA->_x + vertexB->_x + vertexC->_x)/3.0f,
	    (vertexA->_y + vertexB->_y + vertexC->_y)/3.0f,
	    (vertexA->_z + vertexB->_z + vertexC->_z)/3.0f),

    _colorf((float)r,(float)g,(float)b), // For use in all other cases
    _color(SDL_MapRGB(Screen::_surface->format, r,g,b)), // For use with DrawPixel
    _twoSided(twosided),
    _bottom(FLT_MAX,FLT_MAX,FLT_MAX), // Will be updated after centering in Loader
    _top(-FLT_MAX,-FLT_MAX,-FLT_MAX), // Will be updated after centering in Loader
    _pMesh(pMesh)
{
    if (!triNormalProvided) {
	_normal = Vector3(
	    (vertexA->_normal._x + vertexB->_normal._x + vertexC->_normal._x)/3.0f,
	    (vertexA->_normal._y + vertexB->_normal._y + vertexC->_normal._y)/3.0f,
	    (vertexA->_normal._z + vertexB->_normal._z + vertexC->_normal._z)/3.0f);
	_normal.normalize();
    } else {
	_normal = triNormal;
    }
}
