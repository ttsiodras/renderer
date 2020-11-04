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

#ifndef __KEYBOARD_H__
#define __KEYBOARD_H__

#include <SDL.h>

struct Keyboard {
    Uint32 _isDown, _isUp, _isLeft, _isRight;
    Uint32 _isForward, _isBackward, _isLight, _isLight2;
    Uint32 _isAbort, _isPgUp, _isPgDown;
    Uint32 _isS, _isD, _isE, _isF, _isR, _isH;
    Uint32 _is0, _is1, _is2, _is3, _is4, _is5, _is6, _is7, _is8, _is9;

    bool _isMouseBtnPressed;
    int _mouse_X, _mouse_Y;

    Keyboard();
    ~Keyboard();

    void poll(bool bYield = true);
};

#endif
