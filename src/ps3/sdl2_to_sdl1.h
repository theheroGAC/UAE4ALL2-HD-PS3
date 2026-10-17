#ifdef USE_SDL2
#ifndef SDL2_TO_SDL1_H
#define SDL2_TO_SDL1_H

#include <SDL.h>

extern SDL_Surface *prSDLScreen;

#define SDL_HWSURFACE 0
#define SDL_DOUBLEBUF 0
#define SDL_CreateThread(x,y) SDL_CreateThread(x,"uae_thread",y)
#define SDLKey SDL_Keycode
#define SDLKey SDL_Keycode
#define SDLK_KP0 SDLK_KP_0
#define SDLK_KP1 SDLK_KP_1
#define SDLK_KP2 SDLK_KP_2
#define SDLK_KP3 SDLK_KP_3
#define SDLK_KP4 SDLK_KP_4
#define SDLK_KP5 SDLK_KP_5
#define SDLK_KP6 SDLK_KP_6
#define SDLK_KP7 SDLK_KP_7
#define SDLK_KP8 SDLK_KP_8
#define SDLK_KP9 SDLK_KP_9
#define SDLK_LMETA SDLK_LGUI
#define SDLK_RMETA SDLK_RGUI

#define SDL_keysym SDL_Keysym
#define SDL_DisplayFormat(x) SDL_ConvertSurface(x,prSDLScreen->format,0)
#define SDL_SetAlpha(x,y,z) SDL_SetSurfaceAlphaMod(x,z); SDL_SetSurfaceBlendMode(x, SDL_BLENDMODE_BLEND);
#define SDL_SoftStretch(a,b,c,d) SDL_BlitScaled(a,b,c,d);

SDL_Surface *SDL_SetVideoMode(int w, int h, int bpp, int flags);
void SDL_SetVideoModeScaling(int x, int y, float sw, float sh);
void SDL_SetVideoModeBilinear(int value);
void SDL_Flip(SDL_Surface *surface);
void SDL_SetVideoModeSync(int value);
void updateResolution();
int isDocked();
void ps3_get_game_viewport(int *x, int *y, int *w, int *h);
void ps3_reset_autocrop();
void ps3_osd_show(const char *text, int duration_ms);
extern int ps3_fast_forward;
extern int ps3_turbo_toggle;

#ifdef SDL_LoadBMP
#undef SDL_LoadBMP
#endif
SDL_Surface *ps3_load_bmp(const char *file);
#define SDL_LoadBMP(f) ps3_load_bmp(f)
#endif

#endif