#include "ps3_input.h"
#ifdef USE_SDL2
#include "sdl2_to_sdl1.h"
#endif

extern int mainMenu_swapAB;
extern int inside_menu;
extern int singleJoycons;

SDLKey getKey(Uint8 button) {

	switch(button) {
		case PAD_UP:
		case LSTICK_UP:
		return SDLK_UP;

		case PAD_DOWN:
		case LSTICK_DOWN:
		return SDLK_DOWN;

		case PAD_RIGHT:
		case LSTICK_RIGHT:
		return SDLK_RIGHT;

		case PAD_LEFT:
		case LSTICK_LEFT:
		return SDLK_LEFT;

		case PAD_START:
		return SDLK_LALT;

		case PAD_SELECT:
		return SDLK_LCTRL;

		case PAD_CROSS:
		if (mainMenu_swapAB) {
			return SDLK_END;
		}
		return SDLK_PAGEDOWN;

		case PAD_CIRCLE:
		if (mainMenu_swapAB) {
			return SDLK_PAGEDOWN;
		}
		return SDLK_END;

		case PAD_TRIANGLE:
		return SDLK_PAGEUP;

		case PAD_SQUARE:
		return SDLK_HOME;

		case PAD_L:
		case PAD_SL_LEFT:
		case PAD_SL_RIGHT:
		return SDLK_RSHIFT;

		case PAD_R:
		case PAD_SR_LEFT:
		case PAD_SR_RIGHT:
		return SDLK_RCTRL;

		default:
		break;
	}

	return 0;
}

int ps3_poll_event(SDL_Event *event) {

	int ret = SDL_PollEvent(event);

	if (event != NULL && event->type == SDL_QUIT)
		ps3_exit(1);

	if(event != NULL && inside_menu) {

		switch (event->type) {

			case SDL_JOYAXISMOTION:
			break;

			case SDL_JOYBUTTONDOWN:
				if (event->jbutton.which == 0 || (singleJoycons && event->jbutton.which == 1))
				{
					event->type = SDL_KEYDOWN;
					event->key.keysym.sym = getKey(event->jbutton.button);
				}
				break;

			case SDL_JOYBUTTONUP:
				if (event->jbutton.which == 0 || (singleJoycons && event->jbutton.which == 1))
				{
					event->type = SDL_KEYUP;
					event->key.keysym.sym = getKey(event->jbutton.button);
				}
				break;

			default:
				break;
		 }
	 }

	 return ret;
}
