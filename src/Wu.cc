/* 

Blatantly stolen from SDL_gfx, in order to remove lock/unlock during AA lines...

Original from:

SDL_gfxPrimitives - Graphics primitives for SDL surfaces

LGPL (c) A. Schiffler

*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include <SDL.h>

/* -===================- */

#define DEFAULT_ALPHA_PIXEL_ROUTINE
#undef EXPERIMENTAL_ALPHA_PIXEL_ROUTINE

/* ----- Defines for pixel clipping tests */

#define clip_xmin(surface) surface->clip_rect.x
#define clip_xmax(surface) surface->clip_rect.x+surface->clip_rect.w-1
#define clip_ymin(surface) surface->clip_rect.y
#define clip_ymax(surface) surface->clip_rect.y+surface->clip_rect.h-1

/*!
\brief Internal pixel drawing function with alpha blending where input color in in destination format.

Contains two alternative 32 bit alpha blending routines which can be enabled at the source
level with the defines DEFAULT_ALPHA_PIXEL_ROUTINE or EXPERIMENTAL_ALPHA_PIXEL_ROUTINE.
Only the bits up to the surface depth are significant in the color value.

\param dst The surface to draw on.
\param x The horizontal coordinate of the pixel.
\param y The vertical position of the pixel.
\param color The color value of the pixel to draw. 
\param alpha The blend factor to apply while drawing.

\returns Returns 0 on success, -1 on failure.
*/
static int _putPixelAlpha(SDL_Surface *dst, Sint16 x, Sint16 y, Uint32 color, Uint8 alpha)
{
	SDL_PixelFormat *format;
	Uint32 Rmask, Gmask, Bmask, Amask;
	Uint32 Rshift, Gshift, Bshift, Ashift;
	Uint32 R, G, B, A;

	if (dst == NULL)
	{
		return (-1);
	}

	if (x >= clip_xmin(dst) && x <= clip_xmax(dst) && 
		y >= clip_ymin(dst) && y <= clip_ymax(dst)) 
	{

		format = dst->format;

		switch (format->BytesPerPixel) {
	case 1:
		{		/* Assuming 8-bpp */
			if (alpha == 255) {
				*((Uint8 *) dst->pixels + y * dst->pitch + x) = color;
			} else {
				Uint8 *pixel = (Uint8 *) dst->pixels + y * dst->pitch + x;
				SDL_Palette *palette = format->palette;
				SDL_Color *colors = palette->colors;
				SDL_Color dColor = colors[*pixel];
				SDL_Color sColor = colors[color];
				Uint8 dR = dColor.r;
				Uint8 dG = dColor.g;
				Uint8 dB = dColor.b;
				Uint8 sR = sColor.r;
				Uint8 sG = sColor.g;
				Uint8 sB = sColor.b;

				dR = dR + ((sR - dR) * alpha >> 8);
				dG = dG + ((sG - dG) * alpha >> 8);
				dB = dB + ((sB - dB) * alpha >> 8);

				*pixel = SDL_MapRGB(format, dR, dG, dB);
			}
		}
		break;

	case 2:
		{		/* Probably 15-bpp or 16-bpp */
			if (alpha == 255) {
				*((Uint16 *) dst->pixels + y * dst->pitch / 2 + x) = color;
			} else {
				Uint16 *pixel = (Uint16 *) dst->pixels + y * dst->pitch / 2 + x;
				Uint32 dc = *pixel;

				Rmask = format->Rmask;
				Gmask = format->Gmask;
				Bmask = format->Bmask;
				Amask = format->Amask;
				R = ((dc & Rmask) + (((color & Rmask) - (dc & Rmask)) * alpha >> 8)) & Rmask;
				G = ((dc & Gmask) + (((color & Gmask) - (dc & Gmask)) * alpha >> 8)) & Gmask;
				B = ((dc & Bmask) + (((color & Bmask) - (dc & Bmask)) * alpha >> 8)) & Bmask;
				if (Amask) {
					A = ((dc & Amask) + (((color & Amask) - (dc & Amask)) * alpha >> 8)) & Amask;
				} else 
					A = 0;
				*pixel = R | G | B | A;
			}
		}
		break;

	case 3: 
		{		/* Slow 24-bpp mode, usually not used */
			Uint8 Rshift8, Gshift8, Bshift8, Ashift8;
			Uint8 *pixel = (Uint8 *) dst->pixels + y * dst->pitch + x * 3;

			Rshift = format->Rshift;
			Gshift = format->Gshift;
			Bshift = format->Bshift;
			Ashift = format->Ashift;

			Rshift8 = Rshift / 8;
			Gshift8 = Gshift / 8;
			Bshift8 = Bshift / 8;
			Ashift8 = Ashift / 8;

			if (alpha == 255) {
				*(pixel + Rshift8) = color >> Rshift;
				*(pixel + Gshift8) = color >> Gshift;
				*(pixel + Bshift8) = color >> Bshift;
				*(pixel + Ashift8) = color >> Ashift;
			} else {
				Uint8 dR, dG, dB, dA = 0;
				Uint8 sR, sG, sB, sA = 0;

				dR = *((pixel) + Rshift8);
				dG = *((pixel) + Gshift8);
				dB = *((pixel) + Bshift8);
				dA = *((pixel) + Ashift8);

				sR = (color >> Rshift) & 0xff;
				sG = (color >> Gshift) & 0xff;
				sB = (color >> Bshift) & 0xff;
				sA = (color >> Ashift) & 0xff;

				dR = dR + ((sR - dR) * alpha >> 8);
				dG = dG + ((sG - dG) * alpha >> 8);
				dB = dB + ((sB - dB) * alpha >> 8);
				dA = dA + ((sA - dA) * alpha >> 8);

				*((pixel) + Rshift8) = dR;
				*((pixel) + Gshift8) = dG;
				*((pixel) + Bshift8) = dB;
				*((pixel) + Ashift8) = dA;
			}
		}
		break;

#ifdef DEFAULT_ALPHA_PIXEL_ROUTINE

	case 4:
		{		/* Probably :-) 32-bpp */
			if (alpha == 255) {
				*((Uint32 *) dst->pixels + y * dst->pitch / 4 + x) = color;
			} else {
				Uint32 *pixel = (Uint32 *) dst->pixels + y * dst->pitch / 4 + x;
				Uint32 dc = *pixel;

				Rmask = format->Rmask;
				Gmask = format->Gmask;
				Bmask = format->Bmask;
				Amask = format->Amask;

				Rshift = format->Rshift;
				Gshift = format->Gshift;
				Bshift = format->Bshift;
				Ashift = format->Ashift;

				R = ((dc & Rmask) + (((((color & Rmask) - (dc & Rmask)) >> Rshift) * alpha >> 8) << Rshift)) & Rmask;
				G = ((dc & Gmask) + (((((color & Gmask) - (dc & Gmask)) >> Gshift) * alpha >> 8) << Gshift)) & Gmask;
				B = ((dc & Bmask) + (((((color & Bmask) - (dc & Bmask)) >> Bshift) * alpha >> 8) << Bshift)) & Bmask;
				if (Amask) {
					A = ((dc & Amask) + (((((color & Amask) - (dc & Amask)) >> Ashift) * alpha >> 8) << Ashift)) & Amask;
				} else
					A = 0;
				*pixel = R | G | B | A;
			}
		}
		break;
#endif

#ifdef EXPERIMENTAL_ALPHA_PIXEL_ROUTINE

	case 4:{		/* Probably :-) 32-bpp */
		if (alpha == 255) {
			*((Uint32 *) dst->pixels + y * dst->pitch / 4 + x) = color;
		} else {
			Uint32 *pixel = (Uint32 *) dst->pixels + y * dst->pitch / 4 + x;
			Uint32 dR, dG, dB, dA;
			Uint32 dc = *pixel;

			Uint32 surfaceAlpha, preMultR, preMultG, preMultB;
			Uint32 aTmp;

			Rmask = format->Rmask;
			Gmask = format->Gmask;
			Bmask = format->Bmask;
			Amask = format->Amask;

			dR = (color & Rmask);
			dG = (color & Gmask);
			dB = (color & Bmask);
			dA = (color & Amask);

			Rshift = format->Rshift;
			Gshift = format->Gshift;
			Bshift = format->Bshift;
			Ashift = format->Ashift;

			preMultR = (alpha * (dR >> Rshift));
			preMultG = (alpha * (dG >> Gshift));
			preMultB = (alpha * (dB >> Bshift));

			surfaceAlpha = ((dc & Amask) >> Ashift);
			aTmp = (255 - alpha);
			if (A = 255 - ((aTmp * (255 - surfaceAlpha)) >> 8 )) {
				aTmp *= surfaceAlpha;
				R = (preMultR + ((aTmp * ((dc & Rmask) >> Rshift)) >> 8)) / A << Rshift & Rmask;
				G = (preMultG + ((aTmp * ((dc & Gmask) >> Gshift)) >> 8)) / A << Gshift & Gmask;
				B = (preMultB + ((aTmp * ((dc & Bmask) >> Bshift)) >> 8)) / A << Bshift & Bmask;
			}
			*pixel = R | G | B | (A << Ashift & Amask);

		}
		   }
		   break;
#endif
		}
	}

	return (0);
}

/*!
\brief Pixel draw with blending enabled if a<255.

\param dst The surface to draw on.
\param x X (horizontal) coordinate of the pixel.
\param y Y (vertical) coordinate of the pixel.
\param color The color value of the pixel to draw (0xRRGGBBAA). 

\returns Returns 0 on success, -1 on failure.
*/
static int pixelColor(SDL_Surface * dst, Sint16 x, Sint16 y, Uint32 color)
{
	Uint8 alpha;
	Uint32 mcolor;
	int result = 0;

	/*
	* Setup color 
	*/
	alpha = color & 0x000000ff;
	mcolor =
		SDL_MapRGBA(dst->format, (color & 0xff000000) >> 24,
		(color & 0x00ff0000) >> 16, (color & 0x0000ff00) >> 8, alpha);

	/*
	* Draw 
	*/
	result = _putPixelAlpha(dst, x, y, mcolor, alpha);

	return (result);
}

/*!
\brief Pixel draw with blending enabled if a<255 - no surface locking.

\param dst The surface to draw on.
\param x X (horizontal) coordinate of the pixel.
\param y Y (vertical) coordinate of the pixel.
\param color The color value of the pixel to draw (0xRRGGBBAA). 

\returns Returns 0 on success, -1 on failure.
*/
static int pixelColorNolock(SDL_Surface * dst, Sint16 x, Sint16 y, Uint32 color)
{
	Uint8 alpha;
	Uint32 mcolor;
	int result = 0;

	/*
	* Setup color 
	*/
	alpha = color & 0x000000ff;
	mcolor =
		SDL_MapRGBA(dst->format, (color & 0xff000000) >> 24,
		(color & 0x00ff0000) >> 16, (color & 0x0000ff00) >> 8, alpha);

	/*
	* Draw 
	*/
	result = _putPixelAlpha(dst, x, y, mcolor, alpha);

	return (result);
}


/*!
\brief Internal function to draw filled rectangle with alpha blending.

Assumes color is in destination format.

\param dst The surface to draw on.
\param x1 X coordinate of the first corner (upper left) of the rectangle.
\param y1 Y coordinate of the first corner (upper left) of the rectangle.
\param x2 X coordinate of the second corner (lower right) of the rectangle.
\param y2 Y coordinate of the second corner (lower right) of the rectangle.
\param color The color value of the rectangle to draw (0xRRGGBBAA). 
\param alpha Alpha blending amount for pixels.

\returns Returns 0 on success, -1 on failure.
*/
static int _filledRectAlpha(SDL_Surface * dst, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Uint32 color, Uint8 alpha)
{
	SDL_PixelFormat *format;
	Uint32 Rmask, Bmask, Gmask, Amask;
	Uint32 Rshift, Bshift, Gshift, Ashift;
	Uint8 sR, sG, sB, sA;
	Uint32 R, G, B, A;
	Sint16 x, y;

	format = dst->format;
	switch (format->BytesPerPixel) {
	case 1:
		{			/* Assuming 8-bpp */
			Uint8 *row, *pixel;
			Uint8 dR, dG, dB;
			SDL_Palette *palette = format->palette;
			SDL_Color *colors = palette->colors;
			sR = colors[color].r;
			sG = colors[color].g;
			sB = colors[color].b;

			for (y = y1; y <= y2; y++) {
				row = (Uint8 *) dst->pixels + y * dst->pitch;
				for (x = x1; x <= x2; x++) {
					pixel = row + x;

					dR = colors[*pixel].r;
					dG = colors[*pixel].g;
					dB = colors[*pixel].b;

					dR = dR + ((sR - dR) * alpha >> 8);
					dG = dG + ((sG - dG) * alpha >> 8);
					dB = dB + ((sB - dB) * alpha >> 8);

					*pixel = SDL_MapRGB(format, dR, dG, dB);
				}
			}
		}
		break;

	case 2:
		{			/* Probably 15-bpp or 16-bpp */
			Uint16 *row, *pixel;
			Uint32 dR, dG, dB, dA;
			Rmask = format->Rmask;
			Gmask = format->Gmask;
			Bmask = format->Bmask;
			Amask = format->Amask;

			dR = (color & Rmask); 
			dG = (color & Gmask);
			dB = (color & Bmask);
			dA = (color & Amask);

			for (y = y1; y <= y2; y++) {
				row = (Uint16 *) dst->pixels + y * dst->pitch / 2;
				for (x = x1; x <= x2; x++) {
					pixel = row + x;

					R = ((*pixel & Rmask) + ((dR - (*pixel & Rmask)) * alpha >> 8)) & Rmask;
					G = ((*pixel & Gmask) + ((dG - (*pixel & Gmask)) * alpha >> 8)) & Gmask;
					B = ((*pixel & Bmask) + ((dB - (*pixel & Bmask)) * alpha >> 8)) & Bmask;
					if (Amask)
					{
						A = ((*pixel & Amask) + ((dA - (*pixel & Amask)) * alpha >> 8)) & Amask;
						*pixel = R | G | B | A;
					} else {
						*pixel = R | G | B;
					}
				}
			}
		}
		break;

	case 3:
		{			/* Slow 24-bpp mode, usually not used */
			Uint8 *row, *pix;
			Uint8 dR, dG, dB, dA;
			Uint8 Rshift8, Gshift8, Bshift8, Ashift8;

			Rshift = format->Rshift;
			Gshift = format->Gshift;
			Bshift = format->Bshift;
			Ashift = format->Ashift;

			Rshift8 = Rshift / 8;
			Gshift8 = Gshift / 8;
			Bshift8 = Bshift / 8;
			Ashift8 = Ashift / 8;

			sR = (color >> Rshift) & 0xff;
			sG = (color >> Gshift) & 0xff;
			sB = (color >> Bshift) & 0xff;
			sA = (color >> Ashift) & 0xff;

			for (y = y1; y <= y2; y++) {
				row = (Uint8 *) dst->pixels + y * dst->pitch;
				for (x = x1; x <= x2; x++) {
					pix = row + x * 3;

					dR = *((pix) + Rshift8);
					dG = *((pix) + Gshift8);
					dB = *((pix) + Bshift8);
					dA = *((pix) + Ashift8);

					dR = dR + ((sR - dR) * alpha >> 8);
					dG = dG + ((sG - dG) * alpha >> 8);
					dB = dB + ((sB - dB) * alpha >> 8);
					dA = dA + ((sA - dA) * alpha >> 8);

					*((pix) + Rshift8) = dR;
					*((pix) + Gshift8) = dG;
					*((pix) + Bshift8) = dB;
					*((pix) + Ashift8) = dA;
				}
			}
		}
		break;

#ifdef DEFAULT_ALPHA_PIXEL_ROUTINE
	case 4:
		{			/* Probably :-) 32-bpp */
			Uint32 *row, *pixel;
			Uint32 dR, dG, dB, dA;

			Rmask = format->Rmask;
			Gmask = format->Gmask;
			Bmask = format->Bmask;
			Amask = format->Amask;

			Rshift = format->Rshift;
			Gshift = format->Gshift;
			Bshift = format->Bshift;
			Ashift = format->Ashift;

			dR = (color & Rmask);
			dG = (color & Gmask);
			dB = (color & Bmask);
			dA = (color & Amask);

			for (y = y1; y <= y2; y++) {
				row = (Uint32 *) dst->pixels + y * dst->pitch / 4;
				for (x = x1; x <= x2; x++) {
					pixel = row + x;

					R = ((*pixel & Rmask) + ((((dR - (*pixel & Rmask)) >> Rshift) * alpha >> 8) << Rshift)) & Rmask;
					G = ((*pixel & Gmask) + ((((dG - (*pixel & Gmask)) >> Gshift) * alpha >> 8) << Gshift)) & Gmask;
					B = ((*pixel & Bmask) + ((((dB - (*pixel & Bmask)) >> Bshift) * alpha >> 8) << Bshift)) & Bmask;
					if (Amask)
					{
						A = ((*pixel & Amask) + ((((dA - (*pixel & Amask)) >> Ashift) * alpha >> 8) << Ashift)) & Amask;
						*pixel = R | G | B | A;
					} else {
						*pixel = R | G | B;
					}
				}
			}
		}
		break;
#endif

#ifdef EXPERIMENTAL_ALPHA_PIXEL_ROUTINE
	case 4:{			/* Probably :-) 32-bpp */
		Uint32 *row, *pixel;
		Uint32 dR, dG, dB, dA;
		Uint32 dc;
		Uint32 surfaceAlpha, preMultR, preMultG, preMultB;
		Uint32 aTmp;

		Rmask = format->Rmask;
		Gmask = format->Gmask;
		Bmask = format->Bmask;
		Amask = format->Amask;

		dR = (color & Rmask);
		dG = (color & Gmask);
		dB = (color & Bmask);
		dA = (color & Amask);

		Rshift = format->Rshift;
		Gshift = format->Gshift;
		Bshift = format->Bshift;
		Ashift = format->Ashift;

		preMultR = (alpha * (dR >> Rshift));
		preMultG = (alpha * (dG >> Gshift));
		preMultB = (alpha * (dB >> Bshift));

		for (y = y1; y <= y2; y++) {
			row = (Uint32 *) dst->pixels + y * dst->pitch / 4;
			for (x = x1; x <= x2; x++) {
				pixel = row + x;
				dc = *pixel;

				surfaceAlpha = ((dc & Amask) >> Ashift);
				aTmp = (255 - alpha);
				if (A = 255 - ((aTmp * (255 - surfaceAlpha)) >> 8 )) {
					aTmp *= surfaceAlpha;
					R = (preMultR + ((aTmp * ((dc & Rmask) >> Rshift)) >> 8)) / A << Rshift & Rmask;
					G = (preMultG + ((aTmp * ((dc & Gmask) >> Gshift)) >> 8)) / A << Gshift & Gmask;
					B = (preMultB + ((aTmp * ((dc & Bmask) >> Bshift)) >> 8)) / A << Bshift & Bmask;
				}
				*pixel = R | G | B | (A << Ashift & Amask);

			}
		}
		   }
		   break;
#endif

	}

	return (0);
}

/*!
\brief Draw filled rectangle of RGBA color with alpha blending.

\param dst The surface to draw on.
\param x1 X coordinate of the first corner (upper left) of the rectangle.
\param y1 Y coordinate of the first corner (upper left) of the rectangle.
\param x2 X coordinate of the second corner (lower right) of the rectangle.
\param y2 Y coordinate of the second corner (lower right) of the rectangle.
\param color The color value of the rectangle to draw (0xRRGGBBAA). 

\returns Returns 0 on success, -1 on failure.
*/
static int filledRectAlpha(SDL_Surface * dst, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Uint32 color)
{
	Uint8 alpha;
	Uint32 mcolor;
	int result = 0;

	/*
	* Setup color 
	*/
	alpha = color & 0x000000ff;
	mcolor =
		SDL_MapRGBA(dst->format, (color & 0xff000000) >> 24,
		(color & 0x00ff0000) >> 16, (color & 0x0000ff00) >> 8, alpha);

	/*
	* Draw 
	*/
	result = _filledRectAlpha(dst, x1, y1, x2, y2, mcolor, alpha);

	return (result);
}

/*!
\brief Internal function to draw horizontal line of RGBA color with alpha blending.

\param dst The surface to draw on.
\param x1 X coordinate of the first point (i.e. left) of the line.
\param x2 X coordinate of the second point (i.e. right) of the line.
\param y Y coordinate of the points of the line.
\param color The color value of the line to draw (0xRRGGBBAA). 

\returns Returns 0 on success, -1 on failure.
*/
static int _HLineAlpha(SDL_Surface * dst, Sint16 x1, Sint16 x2, Sint16 y, Uint32 color)
{
	return (filledRectAlpha(dst, x1, y, x2, y, color));
}

/*!
\brief Internal function to draw vertical line of RGBA color with alpha blending.

\param dst The surface to draw on.
\param x X coordinate of the points of the line.
\param y1 Y coordinate of the first point (top) of the line.
\param y2 Y coordinate of the second point (bottom) of the line.
\param color The color value of the line to draw (0xRRGGBBAA). 

\returns Returns 0 on success, -1 on failure.
*/
static int _VLineAlpha(SDL_Surface * dst, Sint16 x, Sint16 y1, Sint16 y2, Uint32 color)
{
	return (filledRectAlpha(dst, x, y1, x, y2, color));
}

/*!
\brief Pixel draw with blending enabled and using alpha weight on color - no locking.

\param dst The surface to draw on.
\param x The horizontal coordinate of the pixel.
\param y The vertical position of the pixel.
\param color The color value of the pixel to draw (0xRRGGBBAA). 
\param weight The weight multiplied into the alpha value of the pixel.

\returns Returns 0 on success, -1 on failure.
*/
static int pixelColorWeightNolock(SDL_Surface * dst, Sint16 x, Sint16 y, Uint32 color, Uint32 weight)
{
	Uint32 a;

	/*
	* Get alpha 
	*/
	a = (color & (Uint32) 0x000000ff);

	/*
	* Modify Alpha by weight 
	*/
	a = ((a * weight) >> 8);

	return (pixelColorNolock(dst, x, y, (color & (Uint32) 0xffffff00) | (Uint32) a));
}


/*!
\brief Draw horizontal line with blending.

\param dst The surface to draw on.
\param x1 X coordinate of the first point (i.e. left) of the line.
\param x2 X coordinate of the second point (i.e. right) of the line.
\param y Y coordinate of the points of the line.
\param color The color value of the line to draw (0xRRGGBBAA). 

\returns Returns 0 on success, -1 on failure.
*/
static int hlineColor(SDL_Surface * dst, Sint16 x1, Sint16 x2, Sint16 y, Uint32 color)
{
	Sint16 left, right, top, bottom;
	Uint8 *pixel, *pixellast;
	int dx;
	int pixx, pixy;
	Sint16 xtmp;
	int result;
	Uint8 *colorptr;
	Uint8 color3[3];

	/*
	* Check visibility of clipping rectangle
	*/
	if ((dst->clip_rect.w==0) || (dst->clip_rect.h==0)) {
		return(0);
	}

	/*
	* Swap x1, x2 if required to ensure x1<=x2
	*/
	if (x1 > x2) {
		xtmp = x1;
		x1 = x2;
		x2 = xtmp;
	}

	/*
	* Get clipping boundary and
	* check visibility of hline 
	*/
	left = dst->clip_rect.x;
	if (x2<left) {
		return(0);
	}
	right = dst->clip_rect.x + dst->clip_rect.w - 1;
	if (x1>right) {
		return(0);
	}
	top = dst->clip_rect.y;
	bottom = dst->clip_rect.y + dst->clip_rect.h - 1;
	if ((y<top) || (y>bottom)) {
		return (0);
	}

	/*
	* Clip x 
	*/
	if (x1 < left) {
		x1 = left;
	}
	if (x2 > right) {
		x2 = right;
	}

	/*
	* Calculate width difference
	*/
	dx = x2 - x1;

	/*
	* Alpha check 
	*/
	if ((color & 255) == 255) {

		/*
		* No alpha-blending required 
		*/

		/*
		* Setup color 
		*/
		colorptr = (Uint8 *) & color;
		if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
			color = SDL_MapRGBA(dst->format, colorptr[0], colorptr[1], colorptr[2], colorptr[3]);
		} else {
			color = SDL_MapRGBA(dst->format, colorptr[3], colorptr[2], colorptr[1], colorptr[0]);
		}

		/*
		* More variable setup 
		*/
		pixx = dst->format->BytesPerPixel;
		pixy = dst->pitch;
		pixel = ((Uint8 *) dst->pixels) + pixx * (int) x1 + pixy * (int) y;

		/*
		* Draw 
		*/
		switch (dst->format->BytesPerPixel) {
	case 1:
		memset(pixel, color, dx + 1);
		break;
	case 2:
		pixellast = pixel + dx + dx;
		for (; pixel <= pixellast; pixel += pixx) {
			*(Uint16 *) pixel = color;
		}
		break;
	case 3:
		pixellast = pixel + dx + dx + dx;
		if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
			color3[0] = (color >> 16) & 0xff;
			color3[1] = (color >> 8) & 0xff;
			color3[2] = color & 0xff;
		} else {
			color3[0] = color & 0xff;
			color3[1] = (color >> 8) & 0xff;
			color3[2] = (color >> 16) & 0xff;
		}
		for (; pixel <= pixellast; pixel += pixx) {
			memcpy(pixel, color3, 3);
		}
		break;
	default:		/* case 4 */
		dx = dx + dx;
		pixellast = pixel + dx + dx;
		for (; pixel <= pixellast; pixel += pixx) {
			*(Uint32 *) pixel = color;
		}
		break;
		}

		/*
		* Set result code 
		*/
		result = 0;

	} else {

		/*
		* Alpha blending blit 
		*/
		result = _HLineAlpha(dst, x1, x1 + dx, y, color);
	}

	return (result);
}

/*!
\brief Draw vertical line with blending.

\param dst The surface to draw on.
\param x X coordinate of the points of the line.
\param y1 Y coordinate of the first point (i.e. top) of the line.
\param y2 Y coordinate of the second point (i.e. bottom) of the line.
\param color The color value of the line to draw (0xRRGGBBAA). 

\returns Returns 0 on success, -1 on failure.
*/
static int vlineColor(SDL_Surface * dst, Sint16 x, Sint16 y1, Sint16 y2, Uint32 color)
{
	Sint16 left, right, top, bottom;
	Uint8 *pixel, *pixellast;
	int dy;
	int pixx, pixy;
	Sint16 h;
	Sint16 ytmp;
	int result;
	Uint8 *colorptr;

	/*
	* Check visibility of clipping rectangle
	*/
	if ((dst->clip_rect.w==0) || (dst->clip_rect.h==0)) {
		return(0);
	}

	/*
	* Swap y1, y2 if required to ensure y1<=y2
	*/
	if (y1 > y2) {
		ytmp = y1;
		y1 = y2;
		y2 = ytmp;
	}

	/*
	* Get clipping boundary and
	* check visibility of vline 
	*/
	left = dst->clip_rect.x;
	right = dst->clip_rect.x + dst->clip_rect.w - 1;
	if ((x<left) || (x>right)) {
		return (0);
	}    
	top = dst->clip_rect.y;
	if (y2<top) {
		return(0);
	}
	bottom = dst->clip_rect.y + dst->clip_rect.h - 1;
	if (y1>bottom) {
		return(0);
	}

	/*
	* Clip x 
	*/
	if (y1 < top) {
		y1 = top;
	}
	if (y2 > bottom) {
		y2 = bottom;
	}

	/*
	* Calculate height
	*/
	h = y2 - y1;

	/*
	* Alpha check 
	*/
	if ((color & 255) == 255) {

		/*
		* No alpha-blending required 
		*/

		/*
		* Setup color 
		*/
		colorptr = (Uint8 *) & color;
		if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
			color = SDL_MapRGBA(dst->format, colorptr[0], colorptr[1], colorptr[2], colorptr[3]);
		} else {
			color = SDL_MapRGBA(dst->format, colorptr[3], colorptr[2], colorptr[1], colorptr[0]);
		}

		/*
		* More variable setup 
		*/
		dy = h;
		pixx = dst->format->BytesPerPixel;
		pixy = dst->pitch;
		pixel = ((Uint8 *) dst->pixels) + pixx * (int) x + pixy * (int) y1;
		pixellast = pixel + pixy * dy;

		/*
		* Draw 
		*/
		switch (dst->format->BytesPerPixel) {
	case 1:
		for (; pixel <= pixellast; pixel += pixy) {
			*(Uint8 *) pixel = color;
		}
		break;
	case 2:
		for (; pixel <= pixellast; pixel += pixy) {
			*(Uint16 *) pixel = color;
		}
		break;
	case 3:
		for (; pixel <= pixellast; pixel += pixy) {
			if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
				pixel[0] = (color >> 16) & 0xff;
				pixel[1] = (color >> 8) & 0xff;
				pixel[2] = color & 0xff;
			} else {
				pixel[0] = color & 0xff;
				pixel[1] = (color >> 8) & 0xff;
				pixel[2] = (color >> 16) & 0xff;
			}
		}
		break;
	default:		/* case 4 */
		for (; pixel <= pixellast; pixel += pixy) {
			*(Uint32 *) pixel = color;
		}
		break;
		}

		/*
		* Set result code 
		*/
		result = 0;

	} else {

		/*
		* Alpha blending blit 
		*/

		result = _VLineAlpha(dst, x, y1, y1 + h, color);

	}

	return (result);
}

/* --------- Clipping routines for line */

/* Clipping based heavily on code from                       */
/* http://www.ncsa.uiuc.edu/Vis/Graphics/src/clipCohSuth.c   */

#define CLIP_LEFT_EDGE   0x1
#define CLIP_RIGHT_EDGE  0x2
#define CLIP_BOTTOM_EDGE 0x4
#define CLIP_TOP_EDGE    0x8
#define CLIP_INSIDE(a)   (!a)
#define CLIP_REJECT(a,b) (a&b)
#define CLIP_ACCEPT(a,b) (!(a|b))

/*!
\brief Internal clip-encoding routine.

Calculates a segement-based clipping encoding for a point against a rectangle.

\param x X coordinate of point.
\param y Y coordinate of point.
\param left X coordinate of left edge of the rectangle.
\param top Y coordinate of top edge of the rectangle.
\param right X coordinate of right edge of the rectangle.
\param bottom Y coordinate of bottom edge of the rectangle.
*/
static int _clipEncode(Sint16 x, Sint16 y, Sint16 left, Sint16 top, Sint16 right, Sint16 bottom)
{
	int code = 0;

	if (x < left) {
		code |= CLIP_LEFT_EDGE;
	} else if (x > right) {
		code |= CLIP_RIGHT_EDGE;
	}
	if (y < top) {
		code |= CLIP_TOP_EDGE;
	} else if (y > bottom) {
		code |= CLIP_BOTTOM_EDGE;
	}
	return code;
}

/*!
\brief Clip line to a the clipping rectangle of a surface.

\param dst Target surface to draw on.
\param x1 Pointer to X coordinate of first point of line.
\param y1 Pointer to Y coordinate of first point of line.
\param x2 Pointer to X coordinate of second point of line.
\param y2 Pointer to Y coordinate of second point of line.
*/
static int _clipLine(SDL_Surface * dst, Sint16 * x1, Sint16 * y1, Sint16 * x2, Sint16 * y2)
{
	Sint16 left, right, top, bottom;
	int code1, code2;
	int draw = 0;
	Sint16 swaptmp;
	float m;

	/*
	* Get clipping boundary 
	*/
	left = dst->clip_rect.x;
	right = dst->clip_rect.x + dst->clip_rect.w - 1;
	top = dst->clip_rect.y;
	bottom = dst->clip_rect.y + dst->clip_rect.h - 1;

	while (1) {
		code1 = _clipEncode(*x1, *y1, left, top, right, bottom);
		code2 = _clipEncode(*x2, *y2, left, top, right, bottom);
		if (CLIP_ACCEPT(code1, code2)) {
			draw = 1;
			break;
		} else if (CLIP_REJECT(code1, code2))
			break;
		else {
			if (CLIP_INSIDE(code1)) {
				swaptmp = *x2;
				*x2 = *x1;
				*x1 = swaptmp;
				swaptmp = *y2;
				*y2 = *y1;
				*y1 = swaptmp;
				swaptmp = code2;
				//code2 = code1;  // reported by scan-build as useless
				code1 = swaptmp;
			}
			if (*x2 != *x1) {
				m = (*y2 - *y1) / (float) (*x2 - *x1);
			} else {
				m = 1.0f;
			}
			if (code1 & CLIP_LEFT_EDGE) {
				*y1 += (Sint16) ((left - *x1) * m);
				*x1 = left;
			} else if (code1 & CLIP_RIGHT_EDGE) {
				*y1 += (Sint16) ((right - *x1) * m);
				*x1 = right;
			} else if (code1 & CLIP_BOTTOM_EDGE) {
				if (*x2 != *x1) {
					*x1 += (Sint16) ((bottom - *y1) / m);
				}
				*y1 = bottom;
			} else if (code1 & CLIP_TOP_EDGE) {
				if (*x2 != *x1) {
					*x1 += (Sint16) ((top - *y1) / m);
				}
				*y1 = top;
			}
		}
	}

	return draw;
}


/* ----- Line */

/* Non-alpha line drawing code adapted from routine          */
/* by Pete Shinners, pete@shinners.org                       */
/* Originally from pygame, http://pygame.seul.org            */

#define ABS(a) (((a)<0) ? -(a) : (a))

/*!
\brief Draw line with alpha blending.

\param dst The surface to draw on.
\param x1 X coordinate of the first point of the line.
\param y1 Y coordinate of the first point of the line.
\param x2 X coordinate of the second point of the line.
\param y2 Y coordinate of the second point of the line.
\param color The color value of the line to draw (0xRRGGBBAA). 

\returns Returns 0 on success, -1 on failure.
*/
static int lineColor(SDL_Surface * dst, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Uint32 color)
{
	int pixx, pixy;
	int x, y;
	int dx, dy;
	int ax, ay;
	int sx, sy;
	int swaptmp;
	Uint8 *pixel;
	Uint8 *colorptr;

	/*
	* Clip line and test if we have to draw 
	*/
	if (!(_clipLine(dst, &x1, &y1, &x2, &y2))) {
		return (0);
	}

	/*
	* Test for special cases of straight lines or single point 
	*/
	if (x1 == x2) {
		if (y1 < y2) {
			return (vlineColor(dst, x1, y1, y2, color));
		} else if (y1 > y2) {
			return (vlineColor(dst, x1, y2, y1, color));
		} else {
			return (pixelColor(dst, x1, y1, color));
		}
	}
	if (y1 == y2) {
		if (x1 < x2) {
			return (hlineColor(dst, x1, x2, y1, color));
		} else if (x1 > x2) {
			return (hlineColor(dst, x2, x1, y1, color));
		}
	}

	/*
	* Variable setup 
	*/
	dx = x2 - x1;
	dy = y2 - y1;
	sx = (dx >= 0) ? 1 : -1;
	sy = (dy >= 0) ? 1 : -1;

	/*
	* Check for alpha blending 
	*/
	if ((color & 255) == 255) {

		/*
		* No alpha blending - use fast pixel routines 
		*/

		/*
		* Setup color 
		*/
		colorptr = (Uint8 *) & color;
		if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
			color = SDL_MapRGBA(dst->format, colorptr[0], colorptr[1], colorptr[2], colorptr[3]);
		} else {
			color = SDL_MapRGBA(dst->format, colorptr[3], colorptr[2], colorptr[1], colorptr[0]);
		}

		/*
		* More variable setup 
		*/
		dx = sx * dx + 1;
		dy = sy * dy + 1;
		pixx = dst->format->BytesPerPixel;
		pixy = dst->pitch;
		pixel = ((Uint8 *) dst->pixels) + pixx * (int) x1 + pixy * (int) y1;
		pixx *= sx;
		pixy *= sy;
		if (dx < dy) {
			swaptmp = dx;
			dx = dy;
			dy = swaptmp;
			swaptmp = pixx;
			pixx = pixy;
			pixy = swaptmp;
		}

		/*
		* Draw 
		*/
		x = 0;
		y = 0;
		switch (dst->format->BytesPerPixel) {
	case 1:
		for (; x < dx; x++, pixel += pixx) {
			*pixel = color;
			y += dy;
			if (y >= dx) {
				y -= dx;
				pixel += pixy;
			}
		}
		break;
	case 2:
		for (; x < dx; x++, pixel += pixx) {
			*(Uint16 *) pixel = color;
			y += dy;
			if (y >= dx) {
				y -= dx;
				pixel += pixy;
			}
		}
		break;
	case 3:
		for (; x < dx; x++, pixel += pixx) {
			if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
				pixel[0] = (color >> 16) & 0xff;
				pixel[1] = (color >> 8) & 0xff;
				pixel[2] = color & 0xff;
			} else {
				pixel[0] = color & 0xff;
				pixel[1] = (color >> 8) & 0xff;
				pixel[2] = (color >> 16) & 0xff;
			}
			y += dy;
			if (y >= dx) {
				y -= dx;
				pixel += pixy;
			}
		}
		break;
	default:		/* case 4 */
		for (; x < dx; x++, pixel += pixx) {
			*(Uint32 *) pixel = color;
			y += dy;
			if (y >= dx) {
				y -= dx;
				pixel += pixy;
			}
		}
		break;
		}

	} else {

		/*
		* Alpha blending required - use single-pixel blits 
		*/

		ax = ABS(dx) << 1;
		ay = ABS(dy) << 1;
		x = x1;
		y = y1;
		if (ax > ay) {
			int d = ay - (ax >> 1);

			while (x != x2) {
				pixelColorNolock (dst, x, y, color);
				if (d > 0 || (d == 0 && sx == 1)) {
					y += sy;
					d -= ax;
				}
				x += sx;
				d += ay;
			}
		} else {
			int d = ax - (ay >> 1);

			while (y != y2) {
				pixelColorNolock (dst, x, y, color);
				if (d > 0 || ((d == 0) && (sy == 1))) {
					x += sx;
					d -= ay;
				}
				y += sy;
				d += ax;
			}
		}
		pixelColorNolock (dst, x, y, color);

	}

	return (0);
}

/* AA Line */

#define AAlevels 256
#define AAbits 8

/*!
\brief Internal function to draw anti-aliased line with alpha blending and endpoint control.

This implementation of the Wu antialiasing code is based on Mike Abrash's
DDJ article which was reprinted as Chapter 42 of his Graphics Programming
Black Book, but has been optimized to work with SDL and utilizes 32-bit
fixed-point arithmetic by A. Schiffler. The endpoint control allows the
supression to draw the last pixel useful for rendering continous aa-lines
with alpha<255.

\param dst The surface to draw on.
\param x1 X coordinate of the first point of the aa-line.
\param y1 Y coordinate of the first point of the aa-line.
\param x2 X coordinate of the second point of the aa-line.
\param y2 Y coordinate of the second point of the aa-line.
\param color The color value of the aa-line to draw (0xRRGGBBAA).
\param draw_endpoint Flag indicating if the endpoint should be drawn; draw if non-zero.

\returns Returns 0 on success, -1 on failure.
*/
static int _aalineColor(SDL_Surface * dst, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Uint32 color, int draw_endpoint)
{
	Sint32 xx0, yy0, xx1, yy1;
	int result;
	Uint32 intshift, erracc, erradj;
	Uint32 erracctmp, wgt;
	int dx, dy, tmp, xdir, y0p1, x0pxdir;

	/*
	* Check visibility of clipping rectangle
	*/
	if ((dst->clip_rect.w==0) || (dst->clip_rect.h==0)) {
		return(0);
	}

	/*
	* Clip line and test if we have to draw 
	*/
	if (!(_clipLine(dst, &x1, &y1, &x2, &y2))) {
		return (0);
	}

	/*
	* Keep on working with 32bit numbers 
	*/
	xx0 = x1;
	yy0 = y1;
	xx1 = x2;
	yy1 = y2;

	/*
	* Reorder points if required 
	*/
	if (yy0 > yy1) {
		tmp = yy0;
		yy0 = yy1;
		yy1 = tmp;
		tmp = xx0;
		xx0 = xx1;
		xx1 = tmp;
	}

	/*
	* Calculate distance 
	*/
	dx = xx1 - xx0;
	dy = yy1 - yy0;

	/*
	* Check for special cases 
	*/
	if (dx == 0) {
		/*
		* Vertical line 
		*/
		if (draw_endpoint)
		{
			return (vlineColor(dst, x1, y1, y2, color));
		} else {
			if (dy>0) {
				return (vlineColor(dst, x1, yy0, yy0+dy, color));
			} else {
				return (pixelColor(dst, x1, y1, color));
			}
		}
	} else if (dy == 0) {
		/*
		* Horizontal line 
		*/
		if (draw_endpoint)
		{
			return (hlineColor(dst, x1, x2, y1, color));
		} else {
			if (dx>0) {
				return (hlineColor(dst, xx0, xx0+dx, y1, color));
			} else {
				return (pixelColor(dst, x1, y1, color));
			}
		}
	} else if ((dx == dy) && (draw_endpoint)) {
		/*
		* Diagonal line (with endpoint)
		*/
		return (lineColor(dst, x1, y1, x2, y2, color));
	}

	/*
	* Adjust for negative dx and set xdir 
	*/
	if (dx >= 0) {
		xdir = 1;
	} else {
		xdir = -1;
		dx = (-dx);
	}

	/*
	* Line is not horizontal, vertical or diagonal (with endpoint)
	*/
	result = 0;

	/*
	* Zero accumulator 
	*/
	erracc = 0;

	/*
	* # of bits by which to shift erracc to get intensity level 
	*/
	intshift = 32 - AAbits;

	/*
	* Mask used to flip all bits in an intensity weighting 
	*/
	//wgtcompmask = AAlevels - 1;

	/*
	* Draw the initial pixel in the foreground color 
	*/
	result |= pixelColorNolock(dst, x1, y1, color);

	/*
	* x-major or y-major? 
	*/
	if (dy > dx) {

		/*
		* y-major.  Calculate 16-bit fixed point fractional part of a pixel that
		* X advances every time Y advances 1 pixel, truncating the result so that
		* we won't overrun the endpoint along the X axis 
		*/
		/*
		* Not-so-portable version: erradj = ((Uint64)dx << 32) / (Uint64)dy; 
		*/
		erradj = ((dx << 16) / dy) << 16;

		/*
		* draw all pixels other than the first and last 
		*/
		x0pxdir = xx0 + xdir;
		while (--dy) {
			erracctmp = erracc;
			erracc += erradj;
			if (erracc <= erracctmp) {
				/*
				* rollover in error accumulator, x coord advances 
				*/
				xx0 = x0pxdir;
				x0pxdir += xdir;
			}
			yy0++;		/* y-major so always advance Y */

			/*
			* the AAbits most significant bits of erracc give us the intensity
			* weighting for this pixel, and the complement of the weighting for
			* the paired pixel. 
			*/
			wgt = (erracc >> intshift) & 255;
			result |= pixelColorWeightNolock (dst, xx0, yy0, color, 255 - wgt);
			result |= pixelColorWeightNolock (dst, x0pxdir, yy0, color, wgt);
		}

	} else {

		/*
		* x-major line.  Calculate 16-bit fixed-point fractional part of a pixel
		* that Y advances each time X advances 1 pixel, truncating the result so
		* that we won't overrun the endpoint along the X axis. 
		*/
		/*
		* Not-so-portable version: erradj = ((Uint64)dy << 32) / (Uint64)dx; 
		*/
		erradj = ((dy << 16) / dx) << 16;

		/*
		* draw all pixels other than the first and last 
		*/
		y0p1 = yy0 + 1;
		while (--dx) {

			erracctmp = erracc;
			erracc += erradj;
			if (erracc <= erracctmp) {
				/*
				* Accumulator turned over, advance y 
				*/
				yy0 = y0p1;
				y0p1++;
			}
			xx0 += xdir;	/* x-major so always advance X */
			/*
			* the AAbits most significant bits of erracc give us the intensity
			* weighting for this pixel, and the complement of the weighting for
			* the paired pixel. 
			*/
			wgt = (erracc >> intshift) & 255;
			result |= pixelColorWeightNolock (dst, xx0, yy0, color, 255 - wgt);
			result |= pixelColorWeightNolock (dst, xx0, y0p1, color, wgt);
		}
	}

	/*
	* Do we have to draw the endpoint 
	*/
	if (draw_endpoint) {
		/*
		* Draw final pixel, always exactly intersected by the line and doesn't
		* need to be weighted. 
		*/
		result |= pixelColorNolock (dst, x2, y2, color);
	}

	return (result);
}

/*!
\brief Ddraw anti-aliased line with alpha blending.

\param dst The surface to draw on.
\param x1 X coordinate of the first point of the aa-line.
\param y1 Y coordinate of the first point of the aa-line.
\param x2 X coordinate of the second point of the aa-line.
\param y2 Y coordinate of the second point of the aa-line.
\param color The color value of the aa-line to draw (0xRRGGBBAA).

\returns Returns 0 on success, -1 on failure.
*/
int my_aalineColor(SDL_Surface * dst, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Uint32 color)
{
	return (_aalineColor(dst, x1, y1, x2, y2, color, 1));
}

/*!
\brief Draw anti-aliased line with alpha blending.

\param dst The surface to draw on.
\param x1 X coordinate of the first point of the aa-line.
\param y1 Y coordinate of the first point of the aa-line.
\param x2 X coordinate of the second point of the aa-line.
\param y2 Y coordinate of the second point of the aa-line.
\param r The red value of the aa-line to draw. 
\param g The green value of the aa-line to draw. 
\param b The blue value of the aa-line to draw. 
\param a The alpha value of the aa-line to draw.

\returns Returns 0 on success, -1 on failure.
*/
int my_aalineRGBA(SDL_Surface * dst, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
	return (_aalineColor
		(dst, x1, y1, x2, y2, ((Uint32) r << 24) | ((Uint32) g << 16) | ((Uint32) b << 8) | (Uint32) a, 1));
}
