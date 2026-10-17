#ifndef PS3_INPUT_H
#define PS3_INPUT_H

#include <SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

int ps3_poll_event(SDL_Event *event);
#define PSP2_PollEvent ps3_poll_event

void ps3_exit(int quit_via_home);

#define PAD_LEFT 0
#define PAD_DOWN 1
#define PAD_RIGHT 2
#define PAD_UP 3
#define PAD_START 4
#define PAD_R3 5
#define PAD_L3 6
#define PAD_SELECT 7
#define PAD_SQUARE 8
#define PAD_CROSS 9
#define PAD_CIRCLE 10
#define PAD_TRIANGLE 11
#define PAD_R 12
#define PAD_L 13
#define PAD_R2 14
#define PAD_L2 15
#define LSTICK_LEFT 16
#define LSTICK_UP 17
#define LSTICK_RIGHT 18
#define LSTICK_DOWN 19
#define RSTICK_LEFT 20
#define RSTICK_UP 21
#define RSTICK_RIGHT 22
#define RSTICK_DOWN 23
#define PAD_SL_LEFT 24
#define PAD_SR_LEFT 25
#define PAD_SL_RIGHT 26
#define PAD_SR_RIGHT 27
#define BUTTON_NONE 254

#ifdef __cplusplus
}
#endif

#endif
