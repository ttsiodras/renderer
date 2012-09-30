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

#ifndef __SCANCONVERTER_H__
#define __SCANCONVERTER_H__

#include <algorithm>

#include <assert.h>

template <class ScanlineDatatype, class GetHorizontalData, int height>
class ScanConverter {
private:
    unsigned* _scanlines;
    ScanlineDatatype* _left;
    ScanlineDatatype* _right;

    void ScanlineAdd(int idx, const ScanlineDatatype& v)
    {
	assert(idx>=0 && idx<height);
	if (!_scanlines[idx]) {
	    _left[idx] = v;
	    _scanlines[idx]++;
	} else if (_scanlines[idx] == 1) {
	    if (GetHorizontalData(_left[idx])() <= GetHorizontalData(v)()) {
		_right[idx] = v;
	    } else {
		_right[idx] = _left[idx];
		_left[idx] = v;
	    }
	    _scanlines[idx]++;
	} else {
	    if (GetHorizontalData(v)() < GetHorizontalData(_left[idx])()) {
		_left[idx] = v;
	    } else if (GetHorizontalData(v)() > GetHorizontalData(_right[idx])()) {
		_right[idx] = v;
	    }
	}
	_minimum = std::min<int>(_minimum, idx);
	_maximum = std::max<int>(_maximum, idx);
    }

public:
    int _minimum, _maximum;
public:
    ScanConverter(
	unsigned* lines,
	ScanlineDatatype* left,
	ScanlineDatatype* right)
	:
	_scanlines(lines),
	_left(left),
	_right(right),
	_minimum(height),
	_maximum(-1)
    {
	// Ah, the joy of coding. Upon startup, we clear up the array of booleans
	// that reports whether a scanline is filled with one or two "endpoints".
	//
	// When GCC is not used with SSE optimizations, the type-unsafe,
	// brute-force memset is the fastest way to do it...
	//
	//memset(reinterpret_cast<void*>(_scanlines), 0, sizeof(unsigned)*height);
	//
	// But when using SSE optimizations, the type-safe STL fill_n algorithm
	// (with a very simple loop-based implementation) is the fastest one!
	//
	// What am I supposed to do to detect this at compile time? :-)
	//
	// By default, leave the type-safe, SSE champion in...
	std::fill_n(_scanlines, height, 0);
    }

    void InnerLoop(
	int y1, int y2,
	const ScanlineDatatype& v1,
	const ScanlineDatatype& v2)
    {
	assert(y1<y2);
	if (y1<0 && y2<0)
	    return;
	if (y1>=height && y2>=height)
	    return;
	ScanlineDatatype vtc = v1;
	ScanlineDatatype d12 = v2; d12 -= v1; d12 /= (coord)(y2-y1);
	if (y1<0) {
	    ScanlineDatatype d = d12;
	    d *= (coord) -y1;
	    vtc += d;
	    y1 = 0;
	}
	y2 = std::min(y2, height-1);
	assert(y1<=y2);
	int steps = y2-y1;
	ScanlineAdd(y1, vtc);
	while(steps--) {
	    y1++;
	    vtc += d12;
	    ScanlineAdd(y1, vtc);
	}
    }
    void ScanConvert(
	int y1,
	const ScanlineDatatype& v1,
	int y2,
	const ScanlineDatatype& v2)
    {
	if (y1 == y2) {
	    if (y1>=0 && y1<height) {
		ScanlineAdd(y1, v1);
		ScanlineAdd(y1, v2);
	    }
	} else {
	    if (y1<y2) {
		InnerLoop(y1, y2, v1, v2);
	    } else {
		InnerLoop(y2, y1, v2, v1);
	    }
	}
    }
};

#endif
