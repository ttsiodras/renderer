#pragma once

#include <SDL3/SDL.h>

[[gnu::always_inline]] static inline Uint32 fast_SDL_MapRGB(const SDL_PixelFormat format, const Uint8 r, const Uint8 g, const Uint8 b) {
    SDL_assert(format == SDL_PIXELFORMAT_XRGB8888);
    return ((Uint16)r << 16) | ((Uint16)g << 8) | b;
}

[[gnu::always_inline]] static inline void SDL_WM_SetCaption(const char *title, const char *icon)
{
    (void)icon;

    SDL_Window **windows = SDL_GetWindows( NULL );
	if ( windows ) {
	    SDL_SetWindowTitle( windows[0], title );
	    SDL_free( windows );
	}
}
