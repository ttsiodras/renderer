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

#ifndef __screen_h__
#define __screen_h__

#include <memory>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include <SDL.h>

#include "Types.h"
#include "Base3d.h"
#include "Fillers.h"
#include "ScanConverter.h"
#include "OnlineHelpKeys.h"

#include "MLAA.h"

#ifdef SIMD_SSE
#include <emmintrin.h>
#endif

struct Camera;

template <int BytesPerPixel>
void DrawPixelBasic(int y, int x, Uint32 color);

struct Screen
{
    static SDL_Surface *_surface;
    coord _Zbuffer[HEIGHT][WIDTH];
    Mesh* _meshBuffer[HEIGHT][WIDTH];
    const struct Scene& _scene;
    typedef void (*DrawPixelFun)(int y, int x, Uint32 color);
    static DrawPixelFun DrawPixel;

 public:

    Screen( const struct Scene& scene)
	:
	_scene(scene)
    {
	if ( SDL_Init(SDL_INIT_VIDEO) < 0 ) {
	    std::cerr << "Couldn't initialize SDL: " <<  SDL_GetError() << std::endl;
	    exit(0);
	}

	// Clean up on exit
	atexit(SDL_Quit);

	// We ask for 32bit surface...
	_surface = SDL_SetVideoMode( WIDTH, HEIGHT, 32, SDL_HWSURFACE | SDL_DOUBLEBUF | SDL_HWACCEL | SDL_ASYNCBLIT);
	if (!_surface)
	    // ...and if that fails, we settle for a software-emulation of 16bit
	    _surface = SDL_SetVideoMode( WIDTH, HEIGHT, 16, SDL_SWSURFACE | SDL_DOUBLEBUF);

	if (!_surface) {
	    std::cerr << "Couldn't set video mode: " << SDL_GetError() << std::endl;
	    exit(0);
	}

	if ( SDL_MUSTLOCK(_surface) ) {
	    if ( SDL_LockSurface(_surface) < 0 ) {
		std::cerr << "Couldn't lock _surface: " << SDL_GetError() << std::endl;
		exit(0);
	    }
	}

	//
	// The original form of DrawPixel (DrawPixelBasic) is doing
	// a 'switch' at run-time, reading the bytes per pixel of the surface,
	// and dispatching.
	//
	// To avoid this cost, I turned it into a template, and store
	// a function pointer (DrawPixel) to the proper "plotter" -
	// and being a template, the switch in DrawPixelBasic evaporates.
	//
	// Speed is a most important attribute - for a renderer...
	//
	std::cout << "Selected pixel plotter: ";
	switch(_surface->format->BytesPerPixel) {
	case 1: DrawPixel = &DrawPixelBasic<1>; std::cout << "1" << std::endl; break;
	case 2: DrawPixel = &DrawPixelBasic<2>; std::cout << "2" << std::endl; break;
	case 3: DrawPixel = &DrawPixelBasic<3>; std::cout << "3" << std::endl; break;
	case 4: DrawPixel = &DrawPixelBasic<4>; std::cout << "4" << std::endl; break;
	default:
	    std::cerr << "Can't pre-compile pixel plotter..." << std::endl;
	    exit(0);
	}

	ClearScreen();
	ClearZbuffer();
    }

    ~Screen()
    {
	if ( SDL_MUSTLOCK(_surface) )
	    SDL_UnlockSurface(_surface);
    }

    void ClearScreen() {
	Uint32 color = SDL_MapRGB(_surface->format, 0, 0, 0);
	SDL_FillRect(_surface, NULL, color);
    }

    void ClearZbuffer() {
	memset(reinterpret_cast<void*>(&_Zbuffer[0][0]), 0x0, sizeof(_Zbuffer));
	memset(reinterpret_cast<void*>(&_meshBuffer[0][0]), 0x0, sizeof(_meshBuffer));
    }

    void ShowScreen(bool raytracerOutput=false, bool doMLAA=true) 
    {
#ifdef MLAA_ENABLED
	if (doMLAA && _surface->format->BytesPerPixel==4 && NULL==getenv("NOMLAA"))
	    MLAA((unsigned int*)_surface->pixels, NULL, _surface->pitch/4, HEIGHT);
#else
	(void)doMLAA;
#endif
	if (!raytracerOutput) {

	    // Hack, to add the "Press H for help" when not benchmarking
	    extern bool g_benchmark;
	    if (!g_benchmark) {
		unsigned char *pData = onlineHelpKeysImage;
		for(int h=0; h<OHELPH; h++)
		    for(int w=0; w<OHELPW; w++) {
			pData++;
			unsigned char c = *pData++;
			pData++;
			if (c<200)
			    DrawPixel(
				h + 20,
				WIDTH-20-OHELPW + w,
				SDL_MapRGB(_surface->format, 255-c, 255-c, 255-c));
		    }
	    }
	}

	if ( SDL_MUSTLOCK(_surface) ) {
	    SDL_UnlockSurface(_surface);
	}
	if (raytracerOutput)
	    SDL_UpdateRect(_surface, 0, 0, 0, 0);
	else
	    SDL_Flip(_surface);
	if ( SDL_MUSTLOCK(_surface) ) {
	    if ( SDL_LockSurface(_surface) < 0 ) {
		std::cerr << "Couldn't lock _surface: " << SDL_GetError() << std::endl;
		exit(0);
	    }
	}
    }


    // The family of Plot-ers: Ambient, Gouraud, Phong and PhongShadowed
    template <typename InterpolatedType, typename TriangleCarrier>
    void Plot(int y, int x, const InterpolatedType& v, const TriangleCarrier& tri, const Camera& eye);

    // A functor used by the ScanConverter:
    // It provides the interpolated X coordinate from an InterpolatedType
    template <typename InterpolatedType>
    class AccessProjectionX {
	const InterpolatedType& _p;
    public:
	AccessProjectionX(const InterpolatedType& p):_p(p) {}
	coord operator()() const { return _p._projx; }
    };

    enum {
	NoCheckXRange = 0,
	CheckXRange = 1
    };

    // The family of members that check the ZBuffer, and call the appropriate Plot-er
    template <bool doXRangeChecks, typename InterpolatedType, typename TriangleCarrier>
    void CheckZBufferAndMaybePlot(
	int y,
	int x,
	const InterpolatedType& v,
	const TriangleCarrier& tri,
	const Camera& camera)
    {
	// doXRangeChecks is set in compile-time, so GCC 
	// will remove these two lines for CheckZBufferAndMaybePlot<NoCheckXRange>
	if (doXRangeChecks && (x<0 || x>=WIDTH))
	    return;

	assert(y>=0 && y<HEIGHT && x>=0 && x<WIDTH);
	// If the Z-Buffer says this pixel maps closer to the screen than any previous ones...
	if (_Zbuffer[y][x] < v._z) {
	    // then update the Z-Buffer
	    _Zbuffer[y][x] = v._z;
            _meshBuffer[y][x] = tri._pMesh;
	    // ...and Plot it.
	    Plot(y, x, v, tri, camera);
	}
    }

    // 10% faster than calling floor(x+0.5f)
    inline int myfloor(coord val) {
	if (val<0.) return int(val-0.5f);
	return int(val+0.5f);
    }

    template <typename InterpolatedType, typename TriangleCarrier>
    void RasterizeTriangle(
	const TriangleCarrier& tri,
	const Camera& camera,
	unsigned *lines,
	InterpolatedType *left,
	InterpolatedType *right)
    {
	ScanConverter<
	    InterpolatedType,
	    AccessProjectionX<InterpolatedType>,
	    HEIGHT>
	    scanner(lines, left, right);

	// Scan convert the three triangle edges (line segments)
	// into the left and right arrays of InterpolatedType[HEIGHT]
	scanner.ScanConvert(tri.ay, tri.xformedA, tri.by, tri.xformedB);
	scanner.ScanConvert(tri.ay, tri.xformedA, tri.cy, tri.xformedC);
	scanner.ScanConvert(tri.by, tri.xformedB, tri.cy, tri.xformedC);

	// For each scanline that the ScanConverter filled-in...
	for(int i=scanner._minimum; i<=scanner._maximum; i++) {
	    if (lines[i] == 1) {
		// If only one pixel was touched, Plot it (checking X-range: yes)
		CheckZBufferAndMaybePlot<CheckXRange>(
		    i, myfloor(left[i]._projx), left[i], tri, camera);
	    } else {
		// We have a horizontal span of pixels... Check if it is clipped:
		int x1 = myfloor(left[i]._projx);  if (x1>=WIDTH) continue;
		int x2 = myfloor(right[i]._projx); if (x2<0) continue;
		// It is not, count the horizontal steps we will interpolate over:
		int steps = abs(x2-x1);
		if (!steps) {
		    // If left and right are actually on top of each other,
		    // then Plot the single pixel (checking X-range: yes)
		    CheckZBufferAndMaybePlot<CheckXRange>(
			i, myfloor(left[i]._projx), left[i], tri, camera);
		} else {
		    // No, we have a span of pixels... interpolate over them:
		    InterpolatedType start = left[i]; InterpolatedType dLR = right[i];
		    dLR -= start; dLR /= (coord)steps;
		    if (x1<0) {
			// The left-most pixel is outside the screen (on its left)
			// Jump over all the clipped pixels:
			InterpolatedType jump = dLR; jump *= (coord)-x1;
			start += jump;
			steps -= (-x1); // since we jump over, we'll do less steps
			x1 = 0;		// and we'll start from the left screen edge
		    }
		    if (x2>=WIDTH) {
			// The right-most pixel is outside the screen (on its right)
			// Don't calculate any more steps after we reach the right edge.
			steps -= (x2-WIDTH+1);
		    }
		    // Plot the left-most one (checking X-range: no, since we're already clipped)
		    CheckZBufferAndMaybePlot<NoCheckXRange>(
			i, x1, start, tri, camera);
		    while(steps--) {
			// Interpolate over the span...
			x1++;
			start += dLR;
			// ... and Plot (checking X-range: no, since we're already clipped)
			CheckZBufferAndMaybePlot<NoCheckXRange>(
			    i, x1, start, tri, camera);
		    }
		}
	    }
	}
    }

};

// Almost verbatim copy from www.libsdl.org "Introduction" section
//
// This is the original form of DrawPixel. It is doing a 'switch'
// at run-time, reading the bytes per pixel of the surface,
// and dispatching.
//
// To avoid this cost, I turned it into a template, and store
// a function pointer (see Screen class above, member 'DrawPixel')
// to the proper "plotter".
//
// This being a template, the switch on BytesPerPixel evaporates!
//
// Speed is a most important attribute - for a renderer...
//
template <int BytesPerPixel>
void DrawPixelBasic(int y, int x, Uint32 color)
{
    SDL_Surface*& _surface = Screen::_surface;

    // template-based - so this switch is done at compile-time
    switch (BytesPerPixel) {
    //switch (_surface->format->BytesPerPixel) {
	case 1: { /* Assuming 8-bpp */
	    Uint8 *bufp;

	    bufp = (Uint8 *)_surface->pixels + y*_surface->pitch + x;
	    *bufp = color;
	}
	break;

	case 2: { /* Probably 15-bpp or 16-bpp */
	    Uint16 *bufp;

	    bufp = (Uint16 *)_surface->pixels + y*_surface->pitch/2 + x;
	    *bufp = color;
	}
	break;

	case 3: { /* Slow 24-bpp mode, usually not used */
	    Uint8 *bufp;

	    bufp = (Uint8 *)_surface->pixels + y*_surface->pitch + x;
	    *(bufp+_surface->format->Rshift/8) =
		((color & _surface->format->Rmask) >> _surface->format->Rshift) << _surface->format->Rloss;
	    *(bufp+_surface->format->Gshift/8) =
		((color & _surface->format->Gmask) >> _surface->format->Gshift) << _surface->format->Gloss;
	    *(bufp+_surface->format->Bshift/8) =
		((color & _surface->format->Bmask) >> _surface->format->Bshift) << _surface->format->Bloss;
	}
	break;

	case 4: { /* Probably 32-bpp */
	    Uint32 *bufp;

	    bufp = (Uint32 *)_surface->pixels + y*_surface->pitch/4 + x;
	    #ifndef SIMD_SSE2
	    *bufp = color;
	    #else
	    // There is no reason to do the following write via the CPU cache
	    // the screen pixel will not be read again - so use the SSE2
	    // "streaming" instruction, that sidesteps the cache and writes
	    // directly to main memory (speedup).
	    _mm_stream_si32((int*)bufp, (int)color);
	    #endif
	}
	break;
    }
}

#endif
