/*----------------------------------------------------------------------
Copyright (C) 2009 Intel Corporation. All Rights Reserved.
Web site: http://visual-computing.intel-research.net

This code may be used in compiled form in any way you desire. This
file may be redistributed unmodified by any means providing it is 
not sold for profit without the authors written consent, and 
providing that this notice and the authors name is included.
If the source code in this file is used in any commercial application 
then a simple email would be nice.

This file is provided 'as is' with no expressed or implied warranty.
-----------------------------------------------------------------------*/

#ifdef _WIN32
#include <windows.h>
#endif

#include <algorithm>
using std::max;

#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

#include <xmmintrin.h>

#define most_significant_color_bits char(0xff ^ ((1<<4)-1))

inline int report(const char* format, ...)
{
  static bool ignore = false;
  if (ignore) return 0;

  va_list argptr;
  va_start(argptr, format);

  char buf[500];
  size_t nbytes = vsprintf(buf, format, argptr);
  assert(nbytes < sizeof(buf));
  #ifdef _WIN32
  OutputDebugString((LPCSTR)buf);
  #endif
  puts(buf);
  return nbytes;
}

inline unsigned short int ssedif(__m128i& c0, __m128i& c1) {
  // return 4x4 bits set to 1 if c0 is significantly different from c1.
  __m128i hibits = _mm_set1_epi8(most_significant_color_bits);
  __m128i d = _mm_sub_epi8(_mm_max_epu8(c0, c1), _mm_min_epu8(c0, c1));
  d = _mm_and_si128(d, hibits);
  d = _mm_cmpeq_epi8(d, _mm_setzero_si128());
  unsigned short int f = _mm_movemask_epi8(d);
  return 0xffff ^ f;
}

//template <class T>
//T max(const T& a, const T& b)
//{
//    return a>b?a:b;
//}

inline int maxcolor(unsigned int c1) {
  // not used.
  int r1 = (c1 >> 16) & 0xff;
  int g1 = (c1 >> 8)  & 0xff;
  int b1 = c1 & 0xff;
  return max(r1, max(g1, b1));
}

inline int sumColor(unsigned int c1) {  
  int r1 = (c1 >> 16) & 0xff;
  int g1 = (c1 >> 8)  & 0xff;
  int b1 = c1 & 0xff;
  return r1 + g1 + b1;
}


inline unsigned int scaleColor(float w, unsigned int color) {
  // not used.
  assert(w <= 1.0f);
  w = 1 - 0.5f*w;
  unsigned char r = (color >> 16) & 0xff;
  unsigned char g = (color >> 8)  & 0xff;
  unsigned char b = color & 0xff;
  r = (unsigned char)(r*w);
  g = (unsigned char)(g*w);
  b = (unsigned char)(b*w);
  return (r << 16) | (g << 8) | b;
}

inline unsigned int mixColor(float w1, unsigned int c1, float w2, unsigned int c2) {
  unsigned char r1 = (c1 >> 16) & 0xff;
  unsigned char g1 = (c1 >> 8)  & 0xff;
  unsigned char b1 = c1 & 0xff;
  unsigned char r2 = (c2 >> 16) & 0xff;
  unsigned char g2 = (c2 >> 8)  & 0xff;
  unsigned char b2 = c2 & 0xff;
  r1 = (unsigned char)(r1*w1 + r2*w2);
  g1 = (unsigned char)(g1*w1 + g2*w2);
  b1 = (unsigned char)(b1*w1 + b2*w2);
  return (r1 << 16) | (g1 << 8) | b1;
}
inline unsigned int mixColor(float w1, unsigned int c1, float w2, unsigned int c2, float w3, unsigned int c3) {
  unsigned char r1 = (c1 >> 16) & 0xff;
  unsigned char g1 = (c1 >> 8)  & 0xff;
  unsigned char b1 = c1 & 0xff;
  unsigned char r2 = (c2 >> 16) & 0xff;
  unsigned char g2 = (c2 >> 8)  & 0xff;
  unsigned char b2 = c2 & 0xff;
  unsigned char r3 = (c3 >> 16) & 0xff;
  unsigned char g3 = (c3 >> 8)  & 0xff;
  unsigned char b3 = c3 & 0xff;
  r1 = (unsigned char)(r1*w1 + r2*w2 + r3*w3);
  g1 = (unsigned char)(g1*w1 + g2*w2 + g3*w3);
  b1 = (unsigned char)(b1*w1 + b2*w2 + b3*w3);
  return (r1 << 16) | (g1 << 8) | b1;
}


inline int findSeparationLine(int& x0, int& x1, unsigned int* fb0, int fc, int xstart, int xend, int stepx) {

  if (xstart >= xend) return 0;

  // Looking for x0.
  x0 = -1;
  if (stepx > 1) {
    while (true) {
      if (fb0[xstart] & fc) {
        x0 = xstart;
        break;
      }
      xstart += stepx;
      if (xstart > xend) {
        return 0;
      }
    }
  } else {
    while (xstart & 3) {
      if (fb0[xstart] & (1<<31)) {
       x0 = xstart;
       goto findx1;
      }
      xstart++;
    }
    while (true) {
      __m128 f4 = *(__m128*)(fb0 + xstart);
      int f = _mm_movemask_ps(f4);
      if (f) {
        xstart += (f&1)? 0 : (f&2)? 1 : (f&4)? 2 : 3;
        assert(fb0[xstart] & (1<<31));
        x0 = xstart;
        break;
      }
      xstart += 4;
      if (xstart >= xend) 
        return 0;
    }
  }

  // Looking for x1.
 findx1:
  int len = 1;
  xstart += stepx;
  while (xstart <= xend && (fb0[xstart] & fc)) {
    len++;
    xstart += stepx;
  }
  x1 = xstart - stepx;
  return len;
}


inline float getSplitHeight(unsigned int* fb, int l, int, int icb, int icm, int ipb, int ipm) {
  // Height basically defines (fake) silhouette, which is then used for color blending.
  int cc = sumColor(fb[icb]);
  int cu = sumColor(fb[icm]);
  int pc = sumColor(fb[ipb]);
  int pu = sumColor(fb[ipm]);
  return float(l*(pc-cu)  + (cc-cu) - (pc-pu))/(l*((cc-cu) + (pc-pu)) + (cc-cu) - (pc-pu));
}


inline void blendThinLine(unsigned int* fbi, unsigned int* fb0, int fc, int x0, int x1, int stepx, int befor, int after) {
  // Not in the main version.
  // Weights for = blending (3 rows).
  const float weightc = 0.5f;
  const float weighto = 0.5f*(1 - weightc);

  do {
    if (fb0[x0 + befor] & fc) {
      fbi[x0] = mixColor(weightc, fbi[x0],
                         weighto, fbi[x0 + befor],
                         weighto, fbi[x0 + after]);
    }
    x0 += stepx;
  } while (x0 <= x1);
}


inline void computeUpperBounds(int& s0, int& s1, float& h0, float& h1, 
                                unsigned int* fb0, int fc, 
                                int x0, int x1, int len, int stepx, int befor, int after, int sz) {
  // Execute two loops for upper row/column.
  s0 = s1 = -1;
  int nsteps = 0;
  int xi = x0;
  int t0 = -1, t1 = -1;
  // h/v bits: fc is 'current flag', fo is 'other flag'.
  int fo = fc ^ ((1<<31)|(1<<30));
  do {
    if ((fb0[xi] & fo) && (fb0[xi + befor] & fc)) {
      h0 = getSplitHeight(fb0, len - nsteps, sz, xi + stepx, xi + stepx + after, xi + befor, xi);
      if (0 < h0 && h0 < 1) {
        s0 = xi + stepx;
        break;
      }
    }
    if ((fb0[xi] & fo) && t0 == -1)
      t0 = xi;
    xi += stepx;
    nsteps++;
  } while (xi < x1);
  if (s0 == -1 && t0 != -1) {
    h0 = 0.5f;
    s0 = t0 + stepx;
  }

  if (x1 + stepx >= sz) {
    if (fb0[x1] & fo) t1 = x1;
    x1 -= stepx;
  }

  xi = x1;
  do {
    if ((fb0[xi] & fo) && (fb0[xi + stepx + befor] & fc)) {
      h1 = getSplitHeight(fb0, nsteps, sz, xi + stepx, xi + stepx + befor, xi + after, xi);
      if (0 < h1 && h1 < 1) {
        s1 = xi;
        break;
      }
    }
    if ((fb0[xi] & fo) && t1 == -1)
      t1 = xi;
    xi -= stepx;
    nsteps++;
  } while (xi > x0);
  if (s1 == -1 && t1 != -1) {
    h1 = 0.5f;
    s1 = t1;
  }
}


inline void computeLowerBounds(int& s0, int& s1, float& h0, float& h1, 
                                unsigned int* fb0, int fc,
                                int x0, int x1, int len, int stepx, int, int after, int sz) {
  // Execute two loops for lower row/column.
  s0 = s1 = -1;
  int nsteps = 0;
  int xi = x0;
  int t0 = -1, t1 = -1;
  // h/v bits: fc is 'current flag', fo is 'other flag'.
  int fo = fc ^ ((1<<31)|(1<<30));
  do {
    int xia = xi + after;
    if ((fb0[xia] & fo) && (fb0[xia] & fc)) {
      if (xia + after < sz)
        h0 = getSplitHeight(fb0, len - nsteps, sz, xia + stepx, xi + stepx, xia + after, xia);
      else
        h0 = 0.5f;
      if (0 < h0 && h0 < 1) {
        s0 = xi + stepx;
        break;
      }
    }
    if ((fb0[xia] & fo) && t0 == -1)
      t0 = xi;
    xi += stepx;
    nsteps++;
  } while (xi < x1);
  if (s0 == -1 && t0 != -1) {
    h0 = 0.5f;
    s0 = t0 + stepx;
  }

  if (x1 + stepx >= sz) {
    if (fb0[x1] & fo) t1 = x1;
    x1 -= stepx;
  }

  xi = x1;
  do {
    int xia = xi + after;
    if ((fb0[xia] & fo) && (fb0[xia + stepx] & fo)) {
      if (xia + after < sz)
        h1 = getSplitHeight(fb0, nsteps, sz, xia + stepx, xia + after + stepx, xi, xia);
      else
        h1 = 0.5f;
      if (0 < h1 && h1 < 1) {
        s1 = xi;
        break;
      }
    }
    if ((fb0[xia] & fo) && t1 == -1)
      t1 = xi;
    xi -= stepx;
    nsteps++;
  } while (xi > x0);
  if (s1 == -1 && t1 != -1) {
    h1 = 0.5f;
    s1 = t1;
  }
}


void blendInterval(unsigned int* fbi, int x0, int x1, float h0, float h1, int stepx, int other, int, bool ushape) {
  float dh0 = 2*(1-h0)*stepx/(x1 - x0 + stepx);
  float dh1 = 2*(1-h1)*stepx/(x1 - x0 + stepx);
  int shift = other < 0? -other : 0;
  x0 += shift;
  x1 += shift;

  int middle = (x0 + x1)/2;

  // First half (L shape).
  float area = h0 + 0.5f*dh0;
  if (h0 == 0) {
    x0 += 1 + (x1 - x0)/stepx;
    area = dh1;
  } else {
    do {
      fbi[x0] = mixColor(area, fbi[x0], 1-area, fbi[x0 + other]);
      area += dh0;
      x0 += stepx;
      assert(area <= 1.0001 || x0 >= middle);
    } while (x0 < middle);

    // Middle point.
    if (x0 == middle) {
      // Odd sizes -- process 2 pixels.
      fbi[x0] = mixColor((1-dh0/8), fbi[x0], dh0/8, fbi[x0 + other]);
      if (!ushape) fbi[x0 + other] = mixColor(dh1/8, fbi[x0], (1-dh1/8), fbi[x0 + other]);
      x0 += stepx;
      area = dh1;
    } else {
      area = 0.5f*dh1;
    }
  }

  if (h1 == 0) return;

  if (ushape) {
    area = 1 - area;
    dh1 = -dh1;
  }

  // For U-shapes, only one row/column is updated.
  shift = ushape? 0 : other;

  // Last half (another L shape).
  do {
    fbi[x0 + shift] = mixColor(area, fbi[x0], 1-area, fbi[x0 + other]);
    area += dh1;
    x0 += stepx;
    assert((0 <= area && area <= 1) || x0 > x1);
  } while (x0 <= x1);

}


//inline
void MLAA(unsigned int* fbi, unsigned int* fb0, int resX, int resY, int job, int threadID) {
  // Morphological AA --
  // symmetric processing based on color difference.

  //static int offcheck = -1;
  
  bool local_scratch_memory = fb0 == NULL;
  if (local_scratch_memory) fb0 = (unsigned int*)_mm_malloc(resX*resY*sizeof(unsigned int), 16);

  // Weights for = blending (3 rows).
  //const float weightc = 0.5f;
  //const float weighto = 0.5f*(1 - weightc);

  // Extract flags.
  int noaa = 0, use_half = 0, visualize = 0, afterblend_thin_lines = 0;

  int fc, resx, resy, stepy, stepx, yfrst, ylast;

  const int rows_per_job = 8;

  // I am so lazy...
  //assert(resX%rows_per_job == 0 && resY%rows_per_job == 0);

  const int n_find_fragment_jobs = (resY/rows_per_job);
  const int n_hscan_jobs = (resY/rows_per_job) + ((resY%rows_per_job)? 1:0);
  const int n_vscan_jobs = (resX/rows_per_job) + ((resX%rows_per_job)? 1:0);
  const int njobs = n_find_fragment_jobs + n_hscan_jobs + n_vscan_jobs;
  
 next_job:
  int jobindex = job++;
  if (jobindex >= njobs) goto finish;

  if (noaa && !visualize)
    goto finish;

  if (jobindex >= n_find_fragment_jobs) {

    if (visualize) {
      if (jobindex == n_find_fragment_jobs) {
        // Visualization mode.
        for (int yi = 0; yi < resY; yi++) {
          int y0 = yi*resX;
          for (int xi = 0; xi < resX; xi++) {
            int ci = y0 + xi;
            int h = fb0[ci] & (1<<31);
            int v = fb0[ci] & (1<<30);
      
            if  (h & v) 
              fbi[ci] = 0x000000ff; // blue
            else if (h) 
              fbi[ci] = 0x0000ff00; // green
            else if (v) 
              fbi[ci] = 0x00ff0000; // red
          }
        }

      }
      goto finish;
    }
    
    goto main_blending_loop;
  }

  // ======================================================================
  // Find fragments.
  // Original framebuffer (fbi) is copied to a new location (fb0),
  // while simultaneously
  // identifying and storing information about separation lines (as 2 bits
  // for H/V lines in alpha channel).
  // Let's call this copied buffer 'input'.
  // ======================================================================
  yfrst = jobindex * rows_per_job;
  ylast = yfrst + rows_per_job;
  //report("FF%i %i: %i - %i\n", threadID, jobindex, yfrst, ylast);
  for (int yi = yfrst; yi < ylast; yi += 4) {
    int y0 = yi*resX;
    int ylast = yi==resY-4? 3*resX : 4*resX;
    for (int xi = 0; xi < resX; xi += 4) {
      int ci = y0 + xi;
      __m128i c0 = *(__m128i*)(fbi + ci + 0*resX);
      __m128i c1 = *(__m128i*)(fbi + ci + 1*resX);
      __m128i c2 = *(__m128i*)(fbi + ci + 2*resX);
      __m128i c3 = *(__m128i*)(fbi + ci + 3*resX);
      __m128i c4 = *(__m128i*)(fbi + ci + ylast);
      unsigned short int h0, h1, h2, h3; // horizontal masks
      h0 = ssedif(c0, c1);
      h1 = ssedif(c1, c2);
      h2 = ssedif(c2, c3);
      h3 = ssedif(c3, c4);
      //if (xi == -256 && yi == 128)
      //  cout << endl << "H " << dec << yi << "\t" << xi << "\t" << hex << (h0?1:0) << (h1?1:0) << (h2?1:0) << (h3?1:0) << "\t" << c0 << c1 << c2 << c3 << c4 << endl;
      _MM_TRANSPOSE4_PS((__m128&)c0, (__m128&)c1, (__m128&)c2, (__m128&)c3);
      if (xi == resX-4) {
        c4 = c3;
      } else {
        c4 = _mm_setr_epi32(*(int*)(fbi + ci+4 + 0*resX), *(int*)(fbi + ci+4 + 1*resX), *(int*)(fbi + ci+4 + 2*resX), *(int*)(fbi + ci+4 + 3*resX));
      }
      // transposed rows/columns
      // 00 10 20 30
      // 01 11 21 31
      // 02 12 22 32
      // 03 13 23 33
      unsigned short int v0, v1, v2, v3; // vertical masks
      v0 = ssedif(c0, c1);
      v1 = ssedif(c1, c2);
      v2 = ssedif(c2, c3);
      v3 = ssedif(c3, c4);
      //if (xi == -256 && yi == 128)
      //  cout << "V " << dec << yi << "\t" << xi << "\t" << hex << (v0?1:0) << (v1?1:0) << (v2?1:0) << (v3?1:0) << "\t" << c0 << c1 << c2 << c3 << c4 << endl;

      // Store h/v flags.
      fb0[ci+0 + 0*resX] = fbi[ci+0 + 0*resX] | ((h0 & (0xf<< 0))? (1<<31) : 0) | ((v0 & (0xf<< 0))? (1<<30) : 0);
      fb0[ci+1 + 0*resX] = fbi[ci+1 + 0*resX] | ((h0 & (0xf<< 4))? (1<<31) : 0) | ((v1 & (0xf<< 0))? (1<<30) : 0);
      fb0[ci+2 + 0*resX] = fbi[ci+2 + 0*resX] | ((h0 & (0xf<< 8))? (1<<31) : 0) | ((v2 & (0xf<< 0))? (1<<30) : 0);
      fb0[ci+3 + 0*resX] = fbi[ci+3 + 0*resX] | ((h0 & (0xf<<12))? (1<<31) : 0) | ((v3 & (0xf<< 0))? (1<<30) : 0);
      fb0[ci+0 + 1*resX] = fbi[ci+0 + 1*resX] | ((h1 & (0xf<< 0))? (1<<31) : 0) | ((v0 & (0xf<< 4))? (1<<30) : 0);
      fb0[ci+1 + 1*resX] = fbi[ci+1 + 1*resX] | ((h1 & (0xf<< 4))? (1<<31) : 0) | ((v1 & (0xf<< 4))? (1<<30) : 0);
      fb0[ci+2 + 1*resX] = fbi[ci+2 + 1*resX] | ((h1 & (0xf<< 8))? (1<<31) : 0) | ((v2 & (0xf<< 4))? (1<<30) : 0);
      fb0[ci+3 + 1*resX] = fbi[ci+3 + 1*resX] | ((h1 & (0xf<<12))? (1<<31) : 0) | ((v3 & (0xf<< 4))? (1<<30) : 0);
      fb0[ci+0 + 2*resX] = fbi[ci+0 + 2*resX] | ((h2 & (0xf<< 0))? (1<<31) : 0) | ((v0 & (0xf<< 8))? (1<<30) : 0);
      fb0[ci+1 + 2*resX] = fbi[ci+1 + 2*resX] | ((h2 & (0xf<< 4))? (1<<31) : 0) | ((v1 & (0xf<< 8))? (1<<30) : 0);
      fb0[ci+2 + 2*resX] = fbi[ci+2 + 2*resX] | ((h2 & (0xf<< 8))? (1<<31) : 0) | ((v2 & (0xf<< 8))? (1<<30) : 0);
      fb0[ci+3 + 2*resX] = fbi[ci+3 + 2*resX] | ((h2 & (0xf<<12))? (1<<31) : 0) | ((v3 & (0xf<< 8))? (1<<30) : 0);
      fb0[ci+0 + 3*resX] = fbi[ci+0 + 3*resX] | ((h3 & (0xf<< 0))? (1<<31) : 0) | ((v0 & (0xf<<12))? (1<<30) : 0);
      fb0[ci+1 + 3*resX] = fbi[ci+1 + 3*resX] | ((h3 & (0xf<< 4))? (1<<31) : 0) | ((v1 & (0xf<<12))? (1<<30) : 0);
      fb0[ci+2 + 3*resX] = fbi[ci+2 + 3*resX] | ((h3 & (0xf<< 8))? (1<<31) : 0) | ((v2 & (0xf<<12))? (1<<30) : 0);
      fb0[ci+3 + 3*resX] = fbi[ci+3 + 3*resX] | ((h3 & (0xf<<12))? (1<<31) : 0) | ((v3 & (0xf<<12))? (1<<30) : 0);
    }
  }
  goto next_job;

  // ======================================================================
  // Blend rows and columns.
  // All decisions about what to blend and blending weights are computed
  // using the input buffer, while blended colors are extracted and stored
  // back to the original buffer ('fbi'). Basically, it is 'in place'
  // blending and some pixels will be blended multiple times. Figure 9e of
  // MLAA paper shows such pixels as blue. This 'double blend' alone does
  // not cause any visible artifacts, even though different processing
  // order might result in slightly different color (see the next
  // item). Some pixels might be blended up to 4 times, resulting in extra
  // blending. Processing such areas with high spatial frequencies with
  // MLAA is not very enlightening though, and extra blending is lesser of
  // possible evils. In my first implementation, I had used 'longest
  // separation line' criterion to blend either H or V lines, but then
  // abandoned it as it is simply not noticeable.
  // ======================================================================
 main_blending_loop:
  {

    //report("BL%i %i: ", threadID, jobindex);

    // Order of processing: first all h-lines, then v-lines. Processing each
    // line consists in identifying which pixels to blend and blending
    // weights. Accordingly, it might result in changing color of two
    // immediately adjacent lines as well. To avoid any inconsistencies, I
    // combine all lines in blocks and process blocks in interleaved
    // fashion. That is, first thread asks for a job, it gets block #1,
    // second thread gets block #3, third thread - #5. After all odd
    // horizontal blocks are assigned, I switch to even h-blocks and then to
    // odd/even v-blocks. Frequencies due to line blocking are not visible in
    // the final image, even for simplest shapes, for which these
    // inconsistencies should have pop out if they were noticable.

    jobindex -= n_find_fragment_jobs;
    int scanjobs;
    if (jobindex < n_hscan_jobs) {
      // horizontal lines
      //scan = 0;
      fc = 1<<31; resx = resX; resy = resY; stepy = resX; stepx = 1;
      scanjobs = n_hscan_jobs;
    } else {
      // vertical lines
      jobindex -= n_hscan_jobs;
      //scan = 1;
      fc = 1<<30; resx = resY; resy = resX; stepy = 1; stepx = resX;
      scanjobs = n_vscan_jobs;
    }
    int yodd;
    if (jobindex >= scanjobs/2) {
      jobindex -= scanjobs/2;
      yodd = 1;
    } else {
      yodd = 0;
    }
    yfrst = (2 * jobindex + yodd) * rows_per_job * stepy;
    ylast = yfrst + rows_per_job * stepy;
    if (ylast >= resy*stepy) ylast = resy*stepy - stepy;

    // report("%i - %i (%i)\n", yfrst, ylast, scan);
    
    // yc is current row/column offset in [yfrst,ylast] interval
    int befor = yfrst? -stepy : 0;
    int after = stepy;
    for (int yc = yfrst; yc < ylast; yc += stepy, befor = -stepy) {

      // Loop variable xc is linear offset;
      // x-coordinate is computed as xc%resX, y as xc/resX.
      // Going forward.

      // Patterns we would like to recognize
      // (similar colors represented by the same character; x,y,z for any):

      // 1. mix as \ or /
      // AAAAAc (longer one if x == a)
      // Abzzzz
      // xyyyyy

      // 2. mix as -
      // AAAAAc
      // AbzzAz
      // xyyyyy

      // 3. mix as = (if some x != b)
      // cxxxxy
      // aBBBBe
      // dxxxxy

      int x0, x1, len; // []
      int xend = yc + (resx-1)*stepx;
      int xstart = yc;
      while ((len = findSeparationLine(x0, x1, fb0, fc, xstart, xend, stepx))) {
        assert(x1 - x0 == (len-1)*stepx);

        if (len == 1) {
          // One cell.
          const float weightc = 7.0f/8;
          assert(0 <= x0 && x0 < resX*resY);
          fbi[x0] = mixColor(weightc, fbi[x0], 1-weightc, fbi[x0 + after]);
          assert(0 <= x0 + after && x0 + after < resX*resY);
          fbi[x0 + after] = mixColor(1-weightc, fbi[x0], weightc, fbi[x0 + after]);

        } else {

          if (x0 == yc) {
            x0 += stepx;
            len--;
          }

          if (afterblend_thin_lines)
            blendThinLine(fbi, fb0, fc, x0 - stepx, x1, stepx, befor, after);

          // Color images are different from B&W in a sense that there is no
          // certainty in all decisions. Separation lines are chosen based on some
          // given similarity measure and preferring one groups of pixels over
          // another is a tough choice. Equation 3 in MLAA paper is designed to
          // resolve possible ambiguities in favor of smoother output image with
          // higher entropy (it is implemented as getSplitHeight). That is, even if
          // there are two connected L shapes, they will not be necessarily
          // processed. This will happen only if height values are in (~physically
          // realistic) [0,1] interval. Accordingly, it tilts the decision process
          // in favor of choices with better color stitching and also helps avoid
          // meaningless blends in highly textures areas.
          // 
          // Shapes are not identified explicitly as such, and only emerge when
          // there are orthogonal separation lines. So, while scanning a given row
          // or column of pixels, we look for longest separation lines. For each
          // such line we then look separately on each of two adjacent
          // rows/columns. Let's say we're processing a horizontal row and have
          // found a separation line with pixels p0,p2,p2,... pn.
          // 
          // For the top adjacent row, we execute two cycles (one from p0 to pn and
          // another from pn to p0). It is done in computeUpperBounds function. In
          // each cycle, we look on orthogonal separation lines (vertical ones in
          // this case) and solve equation 3. Once we found h values in [0,1] we
          // immediately break the current loop.
          // 
          // Then we execute 2 loops for the bottom row (computeLowerBounds).
          // 
          // As a result of these 4 cycles, we could identify from 0 to 4 lines,
          // orthogonal to the given one. Once this is done, *all* potential shape
          // combinations are considered for color blending. In an extreme case (4
          // orthogonal lines), there could be 2 Z shapes and 2 U shapes (top and
          // bottom). In my current implementation, U shapes are processed only if
          // there are no Z or L shapes at all. It is stem from an observation that
          // Z shapes are usually more clearly defining a trend. There are no
          // preferences between similar shapes. That is, if there are 2 Z shapes,
          // they both processed. If there are 2 U shapes (and no Z), they will be
          // both processed as well.
          // 
          // In most cases though there is only one shape identified during
          // execution of these 4 loops.

          int   ui0, ui1;
          float uh0, uh1;
          computeUpperBounds(ui0, ui1, uh0, uh1, 
                             fb0, fc,
                             x0 - stepx, x1, len, stepx, befor, after, resX*resY);
          int   li0, li1;
          float lh0, lh1;
          computeLowerBounds(li0, li1, lh0, lh1, 
                             fb0, fc,
                             x0 - stepx, x1, len, stepx, befor, after, resX*resY);


          // computeUpperBounds/computeLowerBounds have executed 4 independent
          // loops (2 for upper and 2 for lower bounds). However, during each loop
          // no assumption was made about what shape will be eventually
          // discovered. The only criterion to break the loop is that found h value
          // is in [0,1] interval. These loops will produce 4 potential orthogonal
          // separation line (OSL) candidates (some of which can be empty). *After*
          // these loops are finished, potential Z/U/L shapes are processed below.

          bool done = false;
          if (ui0 != -1 && li1 != -1 && ui0 < li1) {
            if (use_half) uh0 = lh1 = 0.5f;
            blendInterval(fbi, ui0, li1, uh0, lh1, stepx, after, resX*resY, false);
            done = true;
          }
          if (li0 != -1 && ui1 != -1 && li0 < ui1) {
            if (use_half) lh0 = uh1 = 0.5f;
            blendInterval(fbi, li0, ui1, lh0, uh1, stepx, befor, resX*resY, false);
            done = true;
          }
          if (!done) {
            if (ui0 != -1 && ui1 != -1 && ui0 < ui1) {
              if (use_half) uh0 = uh1 = 0.5f;
              blendInterval(fbi, ui0, ui1, uh0, uh1, stepx, after, resX*resY, true);
            }
            if (li0 != -1 && li1 != -1 && li0 < li1) {
              if (use_half) lh0 = lh1 = 0.5f;
              blendInterval(fbi, li0, li1, lh0, lh1, stepx, befor, resX*resY, true);
            }
          }
        }

        xstart = x1 + stepx;
      }
    } // yc
  } // blending
  goto next_job;

 finish:

  if (threadID == 0) {
    if (local_scratch_memory) {
      _mm_free(fb0);
    }
  }

}
