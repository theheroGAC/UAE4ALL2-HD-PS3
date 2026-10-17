 /*
	* UAE - The Un*x Amiga Emulator
	*
	* Joystick emulation for Linux and BSD. They share too much code to
	* split this file.
	*
	* Copyright 1997 Bernd Schmidt
	* Copyright 1998 Krister Walfridsson
	*/

#include "sysconfig.h"
#include "sysdeps.h"

#include "config.h"
#include "uae.h"
#include "options.h"
#include "memory-uae.h"
#include "custom.h"
#include "joystick.h"
#include "SDL.h"
#include "menu.h"
#include "menu_config.h"
#include "math.h"

#ifdef USE_UAE4ALL_VKBD
#include "vkbd.h"
static int can_change_vkbd_transparency=1;
#endif

#ifdef ANDROIDSDL
#include <android/log.h>
#endif

#ifdef GP2X
#include "gp2x.h"
#include "xwin.h"
extern int gp2xMouseEmuOn;
extern int gp2xButtonRemappingOn;
#if defined(PANDORA) || defined (ANDROIDSDL)
extern int dpadUp[MAX_NUM_CONTROLLERS];
extern int dpadDown[MAX_NUM_CONTROLLERS];
extern int dpadLeft[MAX_NUM_CONTROLLERS];
extern int dpadRight[MAX_NUM_CONTROLLERS];
extern int stickUp[MAX_NUM_CONTROLLERS];
extern int stickDown[MAX_NUM_CONTROLLERS];
extern int stickLeft[MAX_NUM_CONTROLLERS];
extern int stickRight[MAX_NUM_CONTROLLERS];
extern int buttonA[MAX_NUM_CONTROLLERS];
extern int buttonB[MAX_NUM_CONTROLLERS];
extern int buttonX[MAX_NUM_CONTROLLERS];
extern int buttonY[MAX_NUM_CONTROLLERS];
extern int triggerL[MAX_NUM_CONTROLLERS];
extern int triggerR[MAX_NUM_CONTROLLERS];
#ifdef __PS3__
extern int triggerL2[MAX_NUM_CONTROLLERS];
extern int triggerR2[MAX_NUM_CONTROLLERS];
extern int triggerL3[MAX_NUM_CONTROLLERS];
extern int triggerR3[MAX_NUM_CONTROLLERS];
#endif
extern int buttonSelect[MAX_NUM_CONTROLLERS];
extern int buttonStart[MAX_NUM_CONTROLLERS];
extern int singleJoycons;
extern int mainMenu_numPlayers;
#endif
#if defined(__PSP2__) || defined(__PS3__)
extern int rAnalogX;
extern int rAnalogY;
extern int lAnalogX;
extern int lAnalogY;
extern int mainMenu_leftStickMouse;
extern int mainMenu_touchControls;
extern int mainMenu_deadZone;
extern int mainMenu_mouseDevice;
extern int mainMenu_mouseAcceleration;
extern int mainMenu_mouseSlowFactor;
extern int mainMenu_mouseSlowButton;
extern int mainMenu_mouseFastFactor;
extern int mainMenu_mouseFastButton;
extern int mainMenu_mouseSwapButtons;
int delay2[] = {0, 0, 0};
bool slow_mouse = false;
bool fast_mouse = false;
#endif

extern char launchDir[300];
bool ps3_autofire=false;
int delay=0;
#endif

int nr_joysticks;
bool parport_joystick_enabled = true;
extern int joy2_dir, joy3_dir;
extern int joy2_button, joy3_button;

SDL_Joystick *uae4all_joy0, *uae4all_joy1, *uae4all_joy2, *uae4all_joy3, *uae4all_joy4, *uae4all_joy5, *uae4all_joy6, *uae4all_joy7;
extern SDL_Surface *prSDLScreen;

void read_joystick(int nr, unsigned int *dir, int *button)
{
#ifndef MAX_AUTOEVENTS
		int x_axis, y_axis;
		int left = 0, right = 0, top = 0, bot = 0, upRight=0, downRight=0, upLeft=0, downLeft=0, x=0, y=0, a=0, b=0;
		int len, i, num;

		int port = nr;
		int input_index = 0;
		SDL_Joystick *joy;

		*dir = 0;
		*button = 0;

		int player_slot;
		if (port < 0 || port >= MAX_NUM_CONTROLLERS)
			return;
#if defined(__PSP2__) || defined(__PS3__)
		if (singleJoycons)
			player_slot = port;
		else
#endif
		if (port == 0 || port == 1)
			player_slot = (port == ((mainMenu_joyPort == 1) ? 1 : 0)) ? 0 : 1;
		else
			player_slot = port;

#if defined(__PSP2__) || defined(__PS3__)
		if (!singleJoycons && nr_joysticks <= 1 && nr == 0 &&
			!(mainMenu_mouseEmulation && mainMenu_mouseDevice != 2))
			return;
		if (singleJoycons && (port == 1 && nr_joysticks < 2))
			return;
		if (singleJoycons || port >= 2)
			input_index = port;
		if (input_index < 0 || input_index >= MAX_NUM_CONTROLLERS)
			return;

		if (!singleJoycons && port < 2)
			joy = uae4all_joy0;
		else
		{
			switch (port)
			{
				case 0: joy = uae4all_joy0; break;
				case 1: joy = uae4all_joy1; break;
				case 2: joy = uae4all_joy2; break;
				case 3: joy = uae4all_joy3; break;
				case 4: joy = uae4all_joy4; break;
				case 5: joy = uae4all_joy5; break;
				case 6: joy = uae4all_joy6; break;
				case 7: joy = uae4all_joy7; break;
				default: joy = NULL; break;
			}
		}
#else
		switch (port)
		{
			case 0: joy = uae4all_joy0; break;
			case 1: joy = uae4all_joy1; break;
			case 2: joy = uae4all_joy2; break;
			case 3: joy = uae4all_joy3; break;
			default: joy = uae4all_joy1; break;
		}
#endif

		if (nr == 1)
			nr = 0;
		else if (nr == 0)
			nr = 1;

#ifdef USE_UAE4ALL_VKBD
	int usingRegularControls = (singleJoycons && !(buttonStart[input_index] && triggerR[input_index]) && !vkbd_mode) ||
		((!mainMenu_customControls) && ((nr == 1 && mainMenu_joyPort == 2) || (nr == 0 && mainMenu_joyPort == 1)) && !(buttonStart[input_index] && triggerR[input_index]) && !vkbd_mode);
#else
	int usingRegularControls = (singleJoycons && !(buttonStart[input_index] && triggerR[input_index])) ||
		((!mainMenu_customControls) && ((nr == 1 && mainMenu_joyPort == 2) || (nr == 0 && mainMenu_joyPort == 1)) && !(buttonStart[input_index] && triggerR[input_index]));
#endif
#if !defined(__PSP2__) && !defined(__PS3__)
	SDL_JoystickUpdate();
#endif
	int mouseScale = mainMenu_mouseMultiplier * 8 * 16;
	mouseScale /= 100;

#if !defined(__PSP2__) && !defined(__PS3__) && defined(USE_UAE4ALL_VKBD)
	if (!vkbd_mode && ((mainMenu_customControls && mainMenu_custom_dpad==2) || gp2xMouseEmuOn || (triggerL[input_index] && !triggerR[input_index] && !gp2xButtonRemappingOn)))
#else
#if ( defined(__PSP2__) || defined(__PS3__) ) && defined(USE_UAE4ALL_VKBD)
	if (!vkbd_mode && ((mainMenu_customControls && mainMenu_custom_dpad==2) || gp2xMouseEmuOn))
#else
#if defined(__PSP2__) || defined(__PS3__)
	if ((mainMenu_customControls && mainMenu_custom_dpad==2) || gp2xMouseEmuOn)
#else
	if (((mainMenu_customControls && mainMenu_custom_dpad==2) || gp2xMouseEmuOn || (triggerL[input_index] && !triggerR[input_index] && !gp2xButtonRemappingOn)))
#endif
#endif
#endif
	{
		if (buttonY[input_index]) {
			mouseScale = mainMenu_mouseMultiplier * 16;
			mouseScale /= 100;
		}
#if defined(PANDORA) || defined(ANDROIDSDL)
		if (dpadLeft[input_index])
#else
		if (SDL_JoystickGetButton(joy, GP2X_BUTTON_LEFT))
#endif
		{
			lastmx -= mouseScale;
			newmousecounters=1;
		}
#if !( defined(PANDORA) || defined(ANDROIDSDL) )
		if (SDL_JoystickGetButton(joy, GP2X_BUTTON_UPLEFT))
		{
			lastmx -= mouseScale;
			lastmy -= mouseScale;
			newmousecounters=1;
		}
		if (SDL_JoystickGetButton(joy, GP2X_BUTTON_DOWNLEFT))
		{
			lastmx -= mouseScale;
			lastmy += mouseScale;
			newmousecounters=1;
		}
#endif
		if (dpadRight[input_index])
		{
			lastmx += mouseScale;
			newmousecounters=1;
		}
		if (dpadUp[input_index])
		{
			lastmy -= mouseScale;
			newmousecounters=1;
		}
		if (dpadDown[input_index])
		{
			lastmy += mouseScale;
			newmousecounters=1;
		}
	}
	else if (!triggerR[input_index]  && !(mainMenu_customControls && mainMenu_custom_dpad==0) && usingRegularControls)
	{
#if !defined(AROS) && !defined(__PSP2__) && !defined(__PS3__)
		if (dpadRight || SDL_JoystickGetAxis(joy, 0) > 0) right=1;
		if (dpadLeft || SDL_JoystickGetAxis(joy, 0) < 0) left=1;
		if (dpadUp || SDL_JoystickGetAxis(joy, 1) < 0) top=1;
		if (dpadDown || SDL_JoystickGetAxis(joy, 1) > 0) bot=1;
#else
		if (dpadRight[input_index]) right=1;
		if (dpadLeft[input_index]) left=1;
		if (dpadUp[input_index]) top=1;
		if (dpadDown[input_index]) bot=1;
#endif

		if (mainMenu_joyConf)
		{
#ifdef USE_UAE4ALL_VKBD
			if (((buttonX[input_index] && mainMenu_jump > -1) ) && !vkbd_mode)
#else
			if (((buttonX[0] && mainMenu_jump > -1) || SDL_JoystickGetButton(joy, mainMenu_jump)))
#endif
				top = 1;
		}
	}

	if (usingRegularControls && !(gp2xMouseEmuOn) && !(gp2xButtonRemappingOn))
	{
		#ifdef __PS3__
		bool fire1_pressed = (buttonX[input_index] != 0);
#else
		bool fire1_pressed = ((mainMenu_button1==GP2X_BUTTON_B && buttonA[input_index]) || (mainMenu_button1==GP2X_BUTTON_X && buttonX[input_index]) || (mainMenu_button1==GP2X_BUTTON_Y && buttonY[input_index])) != 0;
#endif
		if (mainMenu_autofire > 0)
		{
			int period = (mainMenu_autofire == 1) ? 9 : (mainMenu_autofire == 2 ? 6 : 3);
			if (mainMenu_autofireMode == 1)
			{
				if ((delay % period) == 0)
					*button = 1;
				else
					*button = 0;
				delay++;
			}
			else
			{
				if (fire1_pressed)
				{
					if ((delay % period) == 0)
						*button = 1;
					else
						*button = 0;
					delay++;
				}
				else
				{
					*button = 0;
					delay = 0;
				}
			}
		}
		else
		{
			*button = fire1_pressed ? 1 : 0;
			delay = 0;
		}

		#ifdef __PS3__
		if (buttonB[input_index])
			*button |= (1 << 1);
#else
		if (buttonB[input_index])
			*button |= (1 << 1);
#endif
	}

#if defined(__PSP2__) || defined(__PS3__)
#ifdef USE_UAE4ALL_VKBD
			if (nr == 1 && !(buttonStart[input_index] && triggerR[input_index]) && !vkbd_mode)
#else
			if (nr == 1 && !(buttonStart[input_index] && triggerR[input_index]))
#endif
	{
		fast_mouse=false;
		slow_mouse=false;
		if(mainMenu_customControls)
		{
			for (int i=0; i<nr_joysticks; i++)
			{
				if(
					(
						(mainMenu_custom_A[i]==-25 && buttonA[i]) ||
						(mainMenu_custom_B[i]==-25 && buttonB[i]) ||
						(mainMenu_custom_X[i]==-25 && buttonX[i]) ||
						(mainMenu_custom_Y[i]==-25 && buttonY[i]) ||
						(mainMenu_custom_L[i]==-25 && triggerL[i]) ||
						(mainMenu_custom_R[i]==-25 && triggerR[i])
					) ||
#ifdef __PS3__
					(
						(mainMenu_custom_L2[i]==-25 && triggerL2[i]) ||
						(mainMenu_custom_R2[i]==-25 && triggerR2[i]) ||
						(mainMenu_custom_L3[i]==-25 && triggerL3[i]) ||
						(mainMenu_custom_R3[i]==-25 && triggerR3[i])
					) ||
#endif
					(
						(mainMenu_custom_dpad == 0) &&
						(
							(mainMenu_custom_up[i]==-25 && dpadUp[i]) ||
							(mainMenu_custom_down[i]==-25 && dpadDown[i]) ||
							(mainMenu_custom_left[i]==-25 && dpadLeft[i]) ||
							(mainMenu_custom_right[i]==-25 && dpadRight[i]))
						)
					)
				{
					slow_mouse=true;
					break;
				}
			}
		}
#ifdef __PS3__
		if (!slow_mouse && mainMenu_mouseSlowButton > 0)
		{
			for (int i=0; i<nr_joysticks; i++)
			{
				bool pressed = false;
				switch (mainMenu_mouseSlowButton)
				{
					case 1: pressed = (triggerR2[i] != 0); break;
					case 2: pressed = (triggerL2[i] != 0); break;
					case 3: pressed = (triggerR[i] != 0); break;
					case 4: pressed = (triggerL[i] != 0); break;
					case 5: pressed = (triggerR3[i] != 0); break;
					case 6: pressed = (triggerL3[i] != 0); break;
					case 7: pressed = (buttonY[i] != 0); break;
					case 8: pressed = (buttonX[i] != 0); break;
					default: break;
				}
				if (pressed)
				{
					slow_mouse=true;
					break;
				}
			}
		}
#endif
		if(mainMenu_customControls)
		{
			for (int i=0; i<nr_joysticks; i++)
			{
				if(
					(
						(mainMenu_custom_A[i]==-26 && buttonA[i]) ||
						(mainMenu_custom_B[i]==-26 && buttonB[i]) ||
						(mainMenu_custom_X[i]==-26 && buttonX[i]) ||
						(mainMenu_custom_Y[i]==-26 && buttonY[i]) ||
						(mainMenu_custom_L[i]==-26 && triggerL[i]) ||
						(mainMenu_custom_R[i]==-26 && triggerR[i])
					) ||
#ifdef __PS3__
					(
						(mainMenu_custom_L2[i]==-26 && triggerL2[i]) ||
						(mainMenu_custom_R2[i]==-26 && triggerR2[i]) ||
						(mainMenu_custom_L3[i]==-26 && triggerL3[i]) ||
						(mainMenu_custom_R3[i]==-26 && triggerR3[i])
					) ||
#endif
					(
						(mainMenu_custom_dpad == 0) &&
						(
							(mainMenu_custom_up[i]==-26 && dpadUp[i]) ||
							(mainMenu_custom_down[i]==-26 && dpadDown[i]) ||
							(mainMenu_custom_left[i]==-26 && dpadLeft[i]) ||
							(mainMenu_custom_right[i]==-26 && dpadRight[i]))
						)
					)
				{
					fast_mouse=true;
					break;
				}
			}
		}
#ifdef __PS3__
		if (!fast_mouse && mainMenu_mouseFastButton > 0)
		{
			for (int i=0; i<nr_joysticks; i++)
			{
				bool pressed = false;
				switch (mainMenu_mouseFastButton)
				{
					case 1: pressed = (triggerR2[i] != 0); break;
					case 2: pressed = (triggerL2[i] != 0); break;
					case 3: pressed = (triggerR[i] != 0); break;
					case 4: pressed = (triggerL[i] != 0); break;
					case 5: pressed = (triggerR3[i] != 0); break;
					case 6: pressed = (triggerL3[i] != 0); break;
					case 7: pressed = (buttonY[i] != 0); break;
					case 8: pressed = (buttonX[i] != 0); break;
					default: break;
				}
				if (pressed)
				{
					fast_mouse=true;
					break;
				}
			}
		}
#endif
		if (fast_mouse)
		{
			switch (mainMenu_mouseFastFactor)
			{
				case 0: mouseScale = (mouseScale * 3) / 2; break;
				case 1: mouseScale *= 2; break;
				case 2: mouseScale *= 3; break;
				case 3: mouseScale *= 4; break;
				case 4: mouseScale *= 5; break;
				default: mouseScale *= 3; break;
			}
		}
		if (slow_mouse)
		{
			switch (mainMenu_mouseSlowFactor)
			{
				case 0: mouseScale /= 2; break;
				case 1: mouseScale /= 4; break;
				case 2: mouseScale /= 8; break;
				case 3: mouseScale /= 16; break;
				default: mouseScale /= 8; break;
			}
			if (mouseScale < 1) mouseScale = 1;
		}

		if (mainMenu_mouseEmulation && mainMenu_mouseDevice != 2)
		{
			if (mainMenu_mouseDevice == 3)
			{
				if (dpadLeft[0])
				{
					lastmx -= mouseScale;
					newmousecounters = 1;
				}
				if (dpadRight[0])
				{
					lastmx += mouseScale;
					newmousecounters = 1;
				}
				if (dpadUp[0])
				{
					lastmy -= mouseScale;
					newmousecounters = 1;
				}
				if (dpadDown[0])
				{
					lastmy += mouseScale;
					newmousecounters = 1;
				}
			}
			else
			{
				float analogX=0.0f;
				float analogY=0.0f;
				float deadZone=(float) mainMenu_deadZone;
				float magnitude=0.0f;

				if (mainMenu_mouseDevice == 1 || (mainMenu_leftStickMouse
#ifdef __PS3__
				&& (!singleJoycons)
#endif
				))
				{
					analogX=(float) lAnalogX;
					analogY=(float) lAnalogY;
				}
				else
				{
					analogX=(float) rAnalogX;
					analogY=(float) rAnalogY;
				}
				const float maxAxis = 32767.0f;
				magnitude=sqrt(analogX*analogX+analogY*analogY);
				if (magnitude > deadZone && magnitude > 0.001f && deadZone < maxAxis)
				{
					float absAnalogX = fabs(analogX);
					float absAnalogY = fabs(analogY);
					float maxX = maxAxis;
					float maxY = maxAxis;
					if (absAnalogX > absAnalogY && absAnalogX > 0.001f){
						maxX = maxAxis;
						maxY = (maxAxis * analogY) / absAnalogX;
					}else if (absAnalogY > 0.001f){
						maxX = (maxAxis * analogX) / absAnalogY;
						maxY = maxAxis;
					}
					float maximum = sqrt(maxX * maxX + maxY * maxY);
					if (maximum > 1.25f * maxAxis) maximum = 1.25f * maxAxis;
					if (maximum < magnitude) maximum = magnitude;

					float denom = maximum - deadZone;
					float normMag = (denom > 0.001f) ? ((magnitude - deadZone) / denom) : 0.0f;
					if (normMag < 0.0f) normMag = 0.0f;
					if (normMag > 1.0f) normMag = 1.0f;
					float curveMag = normMag;
					switch (mainMenu_mouseAcceleration)
					{
						case 1:
							curveMag = normMag * (0.6f + 0.4f * normMag);
							break;
						case 2:
							curveMag = normMag * normMag;
							break;
						case 3:
							curveMag = normMag * normMag * normMag;
							break;
						default:
							curveMag = normMag;
							break;
					}

					float scalingFactor = (maximum / magnitude) * curveMag;
					analogX = (analogX * scalingFactor);
					analogY = (analogY * scalingFactor);

					float clampingFactor = 1.0f / maxAxis;
					absAnalogX = fabs(analogX);
					absAnalogY = fabs(analogY);
					if (absAnalogX > maxAxis || absAnalogY > maxAxis){
						if (absAnalogX > absAnalogY)
							clampingFactor = 1.0f / absAnalogX;
						else
							clampingFactor = 1.0f / absAnalogY;
					}

					analogX *= clampingFactor;
					analogY *= clampingFactor;

					lastmx += (int) (analogX * mouseScale);
					lastmy += (int) (analogY * mouseScale);
					newmousecounters=1;
				}
			}
		}
		else
		{
			newmousecounters=0;
		}
	}
#endif

#ifdef USE_UAE4ALL_VKBD
	if(mainMenu_customControls && !(buttonStart[0] && triggerR[0]) && !vkbd_mode)
#else
	if(mainMenu_customControls && !(buttonStart[0] && triggerR[0]))
#endif
	{
		for (int i=0; i<nr_joysticks; i++)
		{
			int u, d, l, r;
			switch (nr)
			{
				case 0:
					u = -9;
					d = -10;
					l = -11;
					r = -12;
					break;
				case 1:
					u = -5;
					d = -6;
					l = -7;
					r = -8;
					break;
				case 2:
					u = -15;
					d = -16;
					l = -17;
					r = -18;
					break;
				case 3:
					u = -21;
					d = -22;
					l = -23;
					r = -24;
					break;
				default:
					break;
			}
			if((mainMenu_custom_A[i]==u && buttonA[i]) || (mainMenu_custom_B[i]==u && buttonB[i]) || (mainMenu_custom_X[i]==u && buttonX[i]) || (mainMenu_custom_Y[i]==u && buttonY[i]) || (mainMenu_custom_L[i]==u && triggerL[i]) || (mainMenu_custom_R[i]==u && triggerR[i])
#ifdef __PS3__
			|| (mainMenu_custom_L2[i]==u && triggerL2[i]) || (mainMenu_custom_R2[i]==u && triggerR2[i])
			|| (mainMenu_custom_L3[i]==u && triggerL3[i]) || (mainMenu_custom_R3[i]==u && triggerR3[i])
#endif
			)
				top = 1;
			else if(mainMenu_custom_dpad == 0)
			{
				if((mainMenu_custom_up[i]==u && dpadUp[i]) || (mainMenu_custom_down[i]==u && dpadDown[i]) || (mainMenu_custom_left[i]==u && dpadLeft[i]) || (mainMenu_custom_right[i]==u && dpadRight[i])
				|| (mainMenu_custom_stickup[i]==u && stickUp[i]) || (mainMenu_custom_stickdown[i]==u && stickDown[i]) || (mainMenu_custom_stickleft[i]==u && stickLeft[i]) || (mainMenu_custom_stickright[i]==u && stickRight[i]))
					top = 1;
			}

			if((mainMenu_custom_A[i]==d && buttonA[i]) || (mainMenu_custom_B[i]==d && buttonB[i]) || (mainMenu_custom_X[i]==d && buttonX[i]) || (mainMenu_custom_Y[i]==d && buttonY[i]) || (mainMenu_custom_L[i]==d && triggerL[i]) || (mainMenu_custom_R[i]==d && triggerR[i])
#ifdef __PS3__
			|| (mainMenu_custom_L2[i]==d && triggerL2[i]) || (mainMenu_custom_R2[i]==d && triggerR2[i])
			|| (mainMenu_custom_L3[i]==d && triggerL3[i]) || (mainMenu_custom_R3[i]==d && triggerR3[i])
#endif
			)
				bot = 1;
			else if(mainMenu_custom_dpad == 0)
			{
				if((mainMenu_custom_up[i]==d && dpadUp[i]) || (mainMenu_custom_down[i]==d && dpadDown[i]) || (mainMenu_custom_left[i]==d && dpadLeft[i]) || (mainMenu_custom_right[i]==d && dpadRight[i])
				|| (mainMenu_custom_stickup[i]==d && stickUp[i]) || (mainMenu_custom_stickdown[i]==d && stickDown[i]) || (mainMenu_custom_stickleft[i]==d && stickLeft[i]) || (mainMenu_custom_stickright[i]==d && stickRight[i]))
					bot = 1;
			}

			if((mainMenu_custom_A[i]==l && buttonA[i]) || (mainMenu_custom_B[i]==l && buttonB[i]) || (mainMenu_custom_X[i]==l && buttonX[i]) || (mainMenu_custom_Y[i]==l && buttonY[i]) || (mainMenu_custom_L[i]==l && triggerL[i]) || (mainMenu_custom_R[i]==l && triggerR[i])
#ifdef __PS3__
			|| (mainMenu_custom_L2[i]==l && triggerL2[i]) || (mainMenu_custom_R2[i]==l && triggerR2[i])
			|| (mainMenu_custom_L3[i]==l && triggerL3[i]) || (mainMenu_custom_R3[i]==l && triggerR3[i])
#endif
			)
				left = 1;
			else if(mainMenu_custom_dpad == 0)
			{
				if((mainMenu_custom_up[i]==l && dpadUp[i]) || (mainMenu_custom_down[i]==l && dpadDown[i]) || (mainMenu_custom_left[i]==l && dpadLeft[i]) || (mainMenu_custom_right[i]==l && dpadRight[i])
				|| (mainMenu_custom_stickup[i]==l && stickUp[i]) || (mainMenu_custom_stickdown[i]==l && stickDown[i]) || (mainMenu_custom_stickleft[i]==l && stickLeft[i]) || (mainMenu_custom_stickright[i]==l && stickRight[i]))
					left = 1;
			}

			if((mainMenu_custom_A[i]==r && buttonA[i]) || (mainMenu_custom_B[i]==r && buttonB[i]) || (mainMenu_custom_X[i]==r && buttonX[i]) || (mainMenu_custom_Y[i]==r && buttonY[i]) || (mainMenu_custom_L[i]==r && triggerL[i]) || (mainMenu_custom_R[i]==r && triggerR[i])
#ifdef __PS3__
			|| (mainMenu_custom_L2[i]==r && triggerL2[i]) || (mainMenu_custom_R2[i]==r && triggerR2[i])
			|| (mainMenu_custom_L3[i]==r && triggerL3[i]) || (mainMenu_custom_R3[i]==r && triggerR3[i])
#endif
			)
				right = 1;
			else if(mainMenu_custom_dpad == 0)
			{
				if((mainMenu_custom_up[i]==r && dpadUp[i]) || (mainMenu_custom_down[i]==r && dpadDown[i]) || (mainMenu_custom_left[i]==r && dpadLeft[i]) || (mainMenu_custom_right[i]==r && dpadRight[i])
				|| (mainMenu_custom_stickup[i]==r && stickUp[i]) || (mainMenu_custom_stickdown[i]==r && stickDown[i]) || (mainMenu_custom_stickleft[i]==r && stickLeft[i]) || (mainMenu_custom_stickright[i]==r && stickRight[i]))
					right = 1;
			}
		}
	}

	if(!gp2xMouseEmuOn && !gp2xButtonRemappingOn)
	{
#ifdef USE_UAE4ALL_VKBD
		if(mainMenu_customControls && !(buttonStart[0] && triggerR[0]) && !vkbd_mode)
#else
		if(mainMenu_customControls && !(buttonStart[0] && triggerR[0]))
#endif
		{
			for (int i=0; i<nr_joysticks; i++)
			{
				int b1, b2;
				switch (nr)
				{
					case 0:
						b1 = -1;
						b2 = -2;
						break;
					case 1:
						b1 = -3;
						b2 = -4;
						break;
					case 2:
						b1 = -13;
						b2 = -14;
						break;
					case 3:
						b1 = -19;
						b2 = -20;
						break;
					default:
						break;
				}
				if((mainMenu_custom_A[i]==b1 && buttonA[i]) || (mainMenu_custom_B[i]==b1 && buttonB[i]) || (mainMenu_custom_X[i]==b1 && buttonX[i]) || (mainMenu_custom_Y[i]==b1 && buttonY[i]) || (mainMenu_custom_L[i]==b1 && triggerL[i]) || (mainMenu_custom_R[i]==b1 && triggerR[i])
#ifdef __PS3__
				|| (mainMenu_custom_L2[i]==b1 && triggerL2[i]) || (mainMenu_custom_R2[i]==b1 && triggerR2[i])
				|| (mainMenu_custom_L3[i]==b1 && triggerL3[i]) || (mainMenu_custom_R3[i]==b1 && triggerR3[i])
#endif
				)
					*button = 1;
				else if(mainMenu_custom_dpad == 0)
				{
					if((mainMenu_custom_up[i]==b1 && dpadUp[i]) || (mainMenu_custom_down[i]==b1 && dpadDown[i]) || (mainMenu_custom_left[i]==b1 && dpadLeft[i]) || (mainMenu_custom_right[i]==b1 && dpadRight[i])
					|| (mainMenu_custom_stickup[i]==b1 && stickUp[i]) || (mainMenu_custom_stickdown[i]==b1 && stickDown[i]) || (mainMenu_custom_stickleft[i]==b1 && stickLeft[i]) || (mainMenu_custom_stickright[i]==b1 && stickRight[i]))
						*button = 1;
				}
				if((mainMenu_custom_A[i]==b2 && buttonA[i]) || (mainMenu_custom_B[i]==b2 && buttonB[i]) || (mainMenu_custom_X[i]==b2 && buttonX[i]) || (mainMenu_custom_Y[i]==b2 && buttonY[i]) || (mainMenu_custom_L[i]==b2 && triggerL[i]) || (mainMenu_custom_R[i]==b2 && triggerR[i])
#ifdef __PS3__
				|| (mainMenu_custom_L2[i]==b2 && triggerL2[i]) || (mainMenu_custom_R2[i]==b2 && triggerR2[i])
				|| (mainMenu_custom_L3[i]==b2 && triggerL3[i]) || (mainMenu_custom_R3[i]==b2 && triggerR3[i])
#endif
				)
					*button |= 1 << 1;
				else if(mainMenu_custom_dpad == 0)
				{
					if((mainMenu_custom_up[i]==b2 && dpadUp[i]) || (mainMenu_custom_down[i]==b2 && dpadDown[i]) || (mainMenu_custom_left[i]==b2 && dpadLeft[i]) || (mainMenu_custom_right[i]==b2 && dpadRight[i])
					|| (mainMenu_custom_stickup[i]==b2 && stickUp[i]) || (mainMenu_custom_stickdown[i]==b2 && stickDown[i]) || (mainMenu_custom_stickleft[i]==b2 && stickLeft[i]) || (mainMenu_custom_stickright[i]==b2 && stickRight[i]))
						*button |= 1 << 1;
				}
			}
			if (mainMenu_autofire > 0)
			{
				int period = (mainMenu_autofire == 1) ? 9 : (mainMenu_autofire == 2 ? 6 : 3);
				if (mainMenu_autofireMode == 1)
				{
					if ((delay % period) == 0)
						*button |= 1;
					else
						*button &= ~1;
					delay++;
				}
				else
				{
					if (*button & 1)
					{
						if ((delay % period) != 0)
							*button &= ~1;
						delay++;
					}
					else
					{
						delay = 0;
					}
				}
			}
			else if (!(*button & 1))
			{
				delay = 0;
			}
		}
	}

#ifdef USE_UAE4ALL_VKBD
	if (vkbd_mode && nr == 1)
	{

#if defined(__PSP2__) || defined(__PS3__)
		if (rAnalogY < -1024*10)
#else
		if (buttonStart[0] && (dpadUp[0] || top))
#endif
		{
			vkbd_displace_up();
		}
#if defined(__PSP2__) || defined(__PS3__)
		else if (rAnalogY > 1024*10)
#else
		else if (buttonStart[0] && (dpadDown[0] || bot))
#endif
		{
			vkbd_displace_down();
		}
#if defined(__PSP2__) || defined(__PS3__)
		else if (rAnalogX < -1024*10)
#else
		else if (buttonStart[0] && (dpadLeft[0] || left))
#endif
		{
			if (can_change_vkbd_transparency)
			{
				vkbd_transparency_up();
				can_change_vkbd_transparency=0;
			}
		}
#if defined(__PSP2__) || defined(__PS3__)
		else if (rAnalogX > 1024*10)
#else
		else if (buttonStart[0] && (dpadRight[0] || right))
#endif
		{
			if (can_change_vkbd_transparency)
			{
				vkbd_transparency_down();
				can_change_vkbd_transparency=0;
			}
		}
		else
		{
			can_change_vkbd_transparency=1;
			if (left || dpadLeft[0])
				vkbd_move |= VKBD_LEFT;
			else
			{
				vkbd_move &= ~VKBD_LEFT;
				if (right || dpadRight[0])
					vkbd_move |= VKBD_RIGHT;
				else
					vkbd_move &= ~VKBD_RIGHT;
			}
			if (top || dpadUp[0])
				vkbd_move |= VKBD_UP;
			else
			{
				vkbd_move &= ~VKBD_UP;
				if (bot || dpadDown[0])
					vkbd_move |= VKBD_DOWN;
				else
					vkbd_move &= ~VKBD_DOWN;
			}
#if defined(__PSP2__) || defined(__PS3__)
#ifdef __PS3__
			if ((!mainMenu_swapAB && buttonX[0]) || (mainMenu_swapAB && buttonB[0]))
#else
			if (buttonX[0])
#endif
			{
				vkbd_move=VKBD_BUTTON;
				buttonX[0] = 0;
				*button = 0;
			}
			else if (buttonY[0])
			{
				vkbd_move=VKBD_BUTTON_SHIFT;
				buttonY[0] = 0;
				*button = 0;
			}
			else if (buttonA[0])
			{
				vkbd_move=VKBD_BUTTON_BACKSPACE;
				buttonA[0] = 0;
				*button = 0;
			}
#ifdef __PS3__
			else if ((!mainMenu_swapAB && buttonB[0]) || (mainMenu_swapAB && buttonX[0]))
#else
			else if (buttonB[0])
#endif
			{
				vkbd_move=VKBD_BUTTON_RESET_STICKY;
				buttonB[0] = 0;
				*button = 0;
			}
#else
			if (*button || buttonX[0] )
			{
				vkbd_move=VKBD_BUTTON;
				buttonX[0] = 0;
				*button = 0;
			}
#endif
			else {
				if (vkbd_touch_x == -1 && vkbd_touch_y == -1)
				{
					for (int i=0; i<NUM_STICKY; i++)
					{
						vkbd_sticky_key[i].can_switch=true;
					}
				}
			}
		}
	}
	else
#endif
	{
#if defined(__PSP2__) || defined(__PS3__)
		if (!singleJoycons && nr_joysticks > 1 && !mainMenu_customControls && ((nr == 0 && mainMenu_joyPort == 2) || (nr == 1 && mainMenu_joyPort == 1)) && player_slot < mainMenu_numPlayers)
		{
			int joynum = singleJoycons ? input_index : 1;
			int delay_slot = singleJoycons ? input_index : joynum - 1;
			if (nr == 2 || nr == 3)
			{
				joynum = nr;
				delay_slot = joynum - 1;
			}
			if (dpadRight[joynum])
				right = 1;
			else if (dpadLeft[joynum])
				left = 1;
			if (dpadUp[joynum])
				top = 1;
			else if (dpadDown[joynum])
				bot = 1;
			#ifdef __PS3__
			bool fire1_pressed = (buttonX[joynum] != 0);
#else
			bool fire1_pressed = ((mainMenu_button1==GP2X_BUTTON_B && buttonA[joynum]) || (mainMenu_button1==GP2X_BUTTON_X && buttonX[joynum]) || (mainMenu_button1==GP2X_BUTTON_Y && buttonY[joynum])) != 0;
#endif
			if (mainMenu_autofire > 0)
			{
				int period = (mainMenu_autofire == 1) ? 9 : (mainMenu_autofire == 2 ? 6 : 3);
				if (mainMenu_autofireMode == 1)
				{
					if ((delay2[delay_slot] % period) == 0)
						*button |= 0x01;
					delay2[delay_slot]++;
				}
				else
				{
					if (fire1_pressed)
					{
						if ((delay2[delay_slot] % period) == 0)
							*button |= 0x01;
						delay2[delay_slot]++;
					}
					else
					{
						delay2[delay_slot] = 0;
					}
				}
			}
			else
			{
				if (fire1_pressed)
					*button |= 0x01;
				delay2[delay_slot] = 0;
			}
			#ifdef __PS3__
			if (buttonB[joynum])
				*button |= (0x01 << 1);
#else
			if (buttonB[joynum])
				*button |= (0x01 << 1);
#endif
		}
#endif
		int output_enabled = 1;
		if (player_slot >= mainMenu_numPlayers)
		{
			*dir = 0;
			*button = 0;
			output_enabled = 0;
		}

		if (output_enabled && (nr == 0 || nr == 1))
		{
			if (singleJoycons && nr_joysticks < 2)
				output_enabled = 0;
			else if (nr == 0)
			{
				if (!singleJoycons && mainMenu_joyPort != 1 &&
				    !(nr_joysticks > 1 && !mainMenu_customControls &&
				      mainMenu_joyPort == 2 && player_slot < mainMenu_numPlayers))
					output_enabled = 0;
			}
			else if (!singleJoycons && mainMenu_joyPort != 2)
			{
				if (!(nr == 1 && nr_joysticks > 1 && !mainMenu_customControls &&
				      mainMenu_joyPort == 1 && player_slot < mainMenu_numPlayers))
					output_enabled = 0;
			}
		}

		if (output_enabled && (nr == 0 || nr == 1))
		{
			if (left)
				top = !top;
			if (right)
				bot = !bot;
			*dir = bot | (right << 1) | (top << 8) | (left << 9);
		}

#ifdef __PS3__
	if (mainMenu_mouseSwapButtons && nr == 0)
	{
		int b0 = *button & 1;
		int b1 = (*button >> 1) & 1;
		*button = (*button & ~3) | (b0 << 1) | b1;
	}
#endif

#if !defined(__PSP2__) && !defined(__PS3__)
	if (!mainMenu_customControls)
	{
		if((nr == 0 && mainMenu_joyPort == 2) || (nr == 1 && mainMenu_joyPort == 1))
		{
			*dir = 0;
			*button = 0;
		}
	}


#endif
#endif
}
}

void init_joystick(void)
{
	int i;
#if defined(PANDORA) || defined(ANDROIDSDL)
	for (i = 0; i < MAX_NUM_CONTROLLERS; i++)
	{
		dpadUp[i] = dpadDown[i] = dpadLeft[i] = dpadRight[i] = 0;
		stickUp[i] = stickDown[i] = stickLeft[i] = stickRight[i] = 0;
		buttonA[i] = buttonB[i] = buttonX[i] = buttonY[i] = 0;
		triggerL[i] = triggerR[i] = 0;
#ifdef __PS3__
		triggerL2[i] = triggerR2[i] = triggerL3[i] = triggerR3[i] = 0;
#endif
		buttonSelect[i] = buttonStart[i] = 0;
	}
#endif

	nr_joysticks = SDL_NumJoysticks();
#ifdef __PS3__
	{
		int max_joy = (mainMenu_numPlayers > 1) ? mainMenu_numPlayers : 1;
		if (nr_joysticks > max_joy)
			nr_joysticks = max_joy;
	}
#endif
	if (nr_joysticks > 0)
		uae4all_joy0 = SDL_JoystickOpen(0);
	if (nr_joysticks > 1)
		uae4all_joy1 = SDL_JoystickOpen(1);
	else
		uae4all_joy1 = NULL;
	if (nr_joysticks > 2)
		uae4all_joy2 = SDL_JoystickOpen(2);
	else
		uae4all_joy2 = NULL;
	if (nr_joysticks > 3)
		uae4all_joy3 = SDL_JoystickOpen(3);
	else
		uae4all_joy3 = NULL;
	if (nr_joysticks > 4)
		uae4all_joy4 = SDL_JoystickOpen(4);
	else
		uae4all_joy4 = NULL;
	if (nr_joysticks > 5)
		uae4all_joy5 = SDL_JoystickOpen(5);
	else
		uae4all_joy5 = NULL;
	if (nr_joysticks > 6)
		uae4all_joy6 = SDL_JoystickOpen(6);
	else
		uae4all_joy6 = NULL;
	if (nr_joysticks > 7)
		uae4all_joy7 = SDL_JoystickOpen(7);
	else
		uae4all_joy7 = NULL;
	if (nr_joysticks > MAX_NUM_CONTROLLERS)
		nr_joysticks = MAX_NUM_CONTROLLERS;
}

void close_joystick(void)
{
	if (nr_joysticks > 0)
		SDL_JoystickClose (uae4all_joy0);
	if (nr_joysticks > 1)
		SDL_JoystickClose (uae4all_joy1);
	if (nr_joysticks > 2)
		SDL_JoystickClose (uae4all_joy2);
	if (nr_joysticks > 3)
		SDL_JoystickClose (uae4all_joy3);
	if (nr_joysticks > 4)
		SDL_JoystickClose (uae4all_joy4);
	if (nr_joysticks > 5)
		SDL_JoystickClose (uae4all_joy5);
	if (nr_joysticks > 6)
		SDL_JoystickClose (uae4all_joy6);
	if (nr_joysticks > 7)
		SDL_JoystickClose (uae4all_joy7);
}
