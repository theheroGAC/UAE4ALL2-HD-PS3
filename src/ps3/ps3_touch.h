#ifndef PS3_TOUCH_H
#define PS3_TOUCH_H

#include <SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

void PS3_HandleTouch(SDL_Event *event);
void PS3_FinishSimulatedMouseClicks(void);

#ifdef __cplusplus
}
#endif
#endif
