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

#include <SDL3/SDL.h>
#include <iostream>
#include <cstdlib>
#include <limits>
#include <fstream>
#include <vector>
#include <unistd.h>
#include <memory.h>

#include "Keyboard.h"
#include "Defines.h"

#define WID	SHADOWMAPSIZE
#define HEI	SHADOWMAPSIZE

using namespace std;

typedef float coord;

int main(int, char *[])
{
    SDL_Window *window;
    SDL_Surface *surface;
    Uint32 *offsets;
    Uint8 *buffer;

    offsets = new Uint32[HEI];

    for(int i=0; i<HEI; i++)
        offsets[i] = i*WID*3;

    window = SDL_CreateWindow("Shadow Renderer", WID, HEI, 0);
    if ( !window ) {
        cerr << "Couldn't initialize SDL: " <<  SDL_GetError() << endl;
        exit(0);
    }

    // Clean up on exit
    atexit(SDL_Quit);

    surface = SDL_GetWindowSurface(window);
    if (!surface) {
        cerr << "Couldn't set video mode: " << SDL_GetError() << endl;
        exit(0);
    }


    buffer = (Uint8 *) surface->pixels;
    memset(buffer, 64, WID*HEI*4);

    union {
	coord away; // this used to be -numeric_limits<coord>::max())
	unsigned filler; // but was switched to this huge negative float value
    } u;
    u.filler = 0xFEFEFEFE; // this makes "away" a huge negative float

    fstream shadow1("shadow", ios::binary | ios::in);
    coord fmax = numeric_limits<coord>::min();
    coord fmin = numeric_limits<coord>::max();
    vector<coord> line(WID);
    while(shadow1) {
        shadow1.read(reinterpret_cast<char *>(&line[0]), WID*sizeof(coord));
        for(unsigned i=0; i<WID; i++) {
            if (line[i] == u.away)
                continue;
            line[i] = 1.0/line[i];
            if (line[i] > fmax) fmax = line[i];
            if (line[i] < fmin) fmin = line[i];
        }
    }
    unsigned j=0;
    fstream shadow2("shadow", ios::binary | ios::in);
    while(shadow2 && j<HEI) {
        shadow2.read(reinterpret_cast<char *>(&line[0]), WID*sizeof(coord));
        for(unsigned i=0; i<WID; i++) {
            Uint8 color;
            if (line[i] == u.away)
                color = 0;
            else {
                line[i] = 1.0/line[i];
                color = (Uint8) (255.0-255.0*(line[i]-fmin)/(fmax-fmin));
            }
            buffer[SHADOWMAPSIZE*4*(SHADOWMAPSIZE-1-i)+j*4+0] = color;
            buffer[SHADOWMAPSIZE*4*(SHADOWMAPSIZE-1-i)+j*4+1] = color;
            buffer[SHADOWMAPSIZE*4*(SHADOWMAPSIZE-1-i)+j*4+2] = color;
            buffer[SHADOWMAPSIZE*4*(SHADOWMAPSIZE-1-i)+j*4+3] = 0xff;
        }
        j++;
    }

    cout << "Min: " << fmin << " - Max: " << fmax << endl;
    SDL_UpdateWindowSurface(window);

    Keyboard keys;
    keys.poll();
    while(!keys._isAbort) {
        keys.poll();
    }

    delete [] offsets;
    return 0;
}
