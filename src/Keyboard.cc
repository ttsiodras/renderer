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

#include "Keyboard.h"

Keyboard::Keyboard()
{
    _isDown=_isUp=_isLeft=_isRight=_isPgUp=_isPgDown=_isS=_isD=_isE=_isF=_isR=_isH=
	   _isForward=_isBackward=_isLight=_isLight2=_isAbort=0;
    _is0 = _is1 = _is2 = _is3 = _is4 = _is5 = _is6 = _is7 = _is8 = _is9 = 0;
}

Keyboard::~Keyboard()
{
}

void Keyboard::poll(bool bYield)
{
    SDL_Event event;

    if (bYield)
	SDL_Delay(1);
    if(SDL_PollEvent(&event)) {

        switch(event.type) {
        case SDL_KEYDOWN:
            {
                switch(event.key.keysym.sym) {
                case SDLK_UP: _isUp = 1; break;
                case SDLK_DOWN: _isDown = 1; break;
                case SDLK_LEFT: _isLeft = 1; break;
                case SDLK_RIGHT: _isRight = 1; break;
                case SDLK_a: _isForward = 1; break;
                case SDLK_z: _isBackward = 1; break;
                case SDLK_w: _isLight = 1; break;
                case SDLK_q: _isLight2 = 1; break;
                case SDLK_s: _isS = 1; break;
                case SDLK_d: _isD = 1; break;
                case SDLK_f: _isF = 1; break;
                case SDLK_e: _isE = 1; break;
                case SDLK_r: _isR = 1; break;
                case SDLK_h: _isH = 1; break;
                case SDLK_ESCAPE: _isAbort = 1; break;
                case SDLK_PAGEDOWN: _isPgDown = 1; break;
                case SDLK_PAGEUP: _isPgUp = 1; break;
		case SDLK_0: _is0 = 1; break;
		case SDLK_1: _is1 = 1; break;
		case SDLK_2: _is2 = 1; break;
		case SDLK_3: _is3 = 1; break;
		case SDLK_4: _is4 = 1; break;
		case SDLK_5: _is5 = 1; break;
		case SDLK_6: _is6 = 1; break;
		case SDLK_7: _is7 = 1; break;
		case SDLK_8: _is8 = 1; break;
		case SDLK_9: _is9 = 1; break;
                default: break;
                }
            }
            break;
        case SDL_KEYUP:
            {
                switch(event.key.keysym.sym) {
                case SDLK_UP: _isUp = 0; break;
                case SDLK_DOWN: _isDown = 0; break;
                case SDLK_LEFT: _isLeft = 0; break;
                case SDLK_RIGHT: _isRight = 0; break;
                case SDLK_a: _isForward = 0; break;
                case SDLK_z: _isBackward = 0; break;
                case SDLK_w: _isLight = 0; break;
                case SDLK_q: _isLight2 = 0; break;
                case SDLK_s: _isS = 0; break;
                case SDLK_d: _isD = 0; break;
                case SDLK_f: _isF = 0; break;
                case SDLK_e: _isE = 0; break;
                case SDLK_r: _isR = 0; break;
                case SDLK_h: _isH = 0; break;
                case SDLK_ESCAPE: _isAbort = 0; break;
                case SDLK_PAGEDOWN: _isPgDown = 0; break;
                case SDLK_PAGEUP: _isPgUp = 0; break;
		case SDLK_0: _is0 = 0; break;
		case SDLK_1: _is1 = 0; break;
		case SDLK_2: _is2 = 0; break;
		case SDLK_3: _is3 = 0; break;
		case SDLK_4: _is4 = 0; break;
		case SDLK_5: _is5 = 0; break;
		case SDLK_6: _is6 = 0; break;
		case SDLK_7: _is7 = 0; break;
		case SDLK_8: _is8 = 0; break;
		case SDLK_9: _is9 = 0; break;
                default: break;
                }
            }
            break;
	case SDL_QUIT: // Thanks, partisann/reddit
	    exit(1);
	    break;
        default:
            break;
        } // switch(event.type)
    } // if (SDL_PollEvent(...))

    Uint8 btn = SDL_GetMouseState (&_mouse_X, &_mouse_Y);
    _isMouseBtnPressed = 0 != ((btn & SDL_BUTTON(SDL_BUTTON_LEFT)) | (btn & SDL_BUTTON(SDL_BUTTON_RIGHT)));
}
