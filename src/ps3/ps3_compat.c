#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include <SDL.h>

#include "ps3_compat.h"
#include "ps3_kbd.h"
#include "ps3_diag.h"

#include <sysutil/osk.h>

AppletOperationMode appletGetOperationMode(void) {
	return AppletOperationMode_Console;
}

u32 hidGetTouchScreenStates(HidTouchScreenState *state, u32 count) {
	(void)count;
	if (state)
		memset(state, 0, sizeof(*state));
	return 0;
}

void hidSetNpadJoyHoldType(HidNpadJoyHoldType type) {
	(void)type;
}

void hidSetNpadJoyAssignmentModeSingleByDefault(HidNpadIdType id) {
	(void)id;
}

void hidSetNpadJoyAssignmentModeDual(HidNpadIdType id) {
	(void)id;
}

HidNpadStyleTag hidGetNpadStyleSet(HidNpadIdType id) {
	(void)id;
	return HidNpadStyleTag_FullJoy;
}

void hidMergeSingleJoyAsDualJoy(HidNpadIdType idSingle, HidNpadIdType idDual) {
	(void)idSingle;
	(void)idDual;
}

#include "ps3_input.h"

extern SDL_Joystick *uae4all_joy0;

static SDL_Joystick *ps3_joy_handle(void) {
	if (uae4all_joy0)
		return uae4all_joy0;
	static SDL_Joystick *joy = NULL;
	if (!joy) {
		if (SDL_WasInit(SDL_INIT_JOYSTICK) == 0) {
			SDL_InitSubSystem(SDL_INIT_JOYSTICK);
		}
		if (SDL_NumJoysticks() > 0) {
			joy = SDL_JoystickOpen(0);
		}
	}
	return joy;
}

void padInitializeDefault(PadState *pad) {
	if (pad)
		memset(pad, 0, sizeof(*pad));
}

static u64 ps3_read_buttons(void) {
	u64 now = 0;
	SDL_Joystick *joy = ps3_joy_handle();

	if (!joy)
		return 0;

	SDL_JoystickUpdate();

	if (SDL_JoystickGetButton(joy, PAD_UP)) now |= HidNpadButton_Up;
	if (SDL_JoystickGetButton(joy, PAD_DOWN)) now |= HidNpadButton_Down;
	if (SDL_JoystickGetButton(joy, PAD_LEFT)) now |= HidNpadButton_Left;
	if (SDL_JoystickGetButton(joy, PAD_RIGHT)) now |= HidNpadButton_Right;

	if (SDL_JoystickGetButton(joy, PAD_CROSS)) now |= HidNpadButton_A;
	if (SDL_JoystickGetButton(joy, PAD_CIRCLE)) now |= HidNpadButton_B;
	if (SDL_JoystickGetButton(joy, PAD_SQUARE)) now |= HidNpadButton_X;
	if (SDL_JoystickGetButton(joy, PAD_TRIANGLE)) now |= HidNpadButton_Y;

	if (SDL_JoystickGetButton(joy, PAD_L)) now |= HidNpadButton_L;
	if (SDL_JoystickGetButton(joy, PAD_R)) now |= HidNpadButton_R;
	if (SDL_JoystickGetButton(joy, PAD_L2)) now |= HidNpadButton_ZL;
	if (SDL_JoystickGetButton(joy, PAD_R2)) now |= HidNpadButton_ZR;
	if (SDL_JoystickGetButton(joy, PAD_L3)) now |= HidNpadButton_LStick;
	if (SDL_JoystickGetButton(joy, PAD_R3)) now |= HidNpadButton_RStick;
	if (SDL_JoystickGetButton(joy, PAD_SELECT)) now |= HidNpadButton_Minus;
	if (SDL_JoystickGetButton(joy, PAD_START)) now |= HidNpadButton_Plus;

	Sint16 axis_x = SDL_JoystickGetAxis(joy, 0);
	Sint16 axis_y = SDL_JoystickGetAxis(joy, 1);
	if (axis_y < -16000) now |= HidNpadButton_Up;
	if (axis_y > 16000) now |= HidNpadButton_Down;
	if (axis_x < -16000) now |= HidNpadButton_Left;
	if (axis_x > 16000) now |= HidNpadButton_Right;

	return now;
}

void padUpdate(PadState *pad) {
	if (pad)
		pad->buttons = ps3_read_buttons();
}

u64 padGetButtons(const PadState *pad) {
	return pad ? pad->buttons : 0;
}

HidAnalogStickState padGetStickPos(const PadState *pad, u32 stick) {
	HidAnalogStickState out;
	SDL_Joystick *p = ps3_joy_handle();

	out.x = 0;
	out.y = 0;

	if (!p)
		return out;

	if (stick == 0) {
		out.x = SDL_JoystickGetAxis(p, 0);
		out.y = -SDL_JoystickGetAxis(p, 1);
	} else {
		out.x = SDL_JoystickGetAxis(p, 2);
		out.y = -SDL_JoystickGetAxis(p, 3);
	}
	return out;
}

Result swkbdCreate(SwkbdConfig *kbd, u32 unk) {
	(void)unk;
	if (!kbd)
		return -1;
	memset(kbd, 0, sizeof(*kbd));
	return 0;
}

void swkbdConfigMakePresetDefault(SwkbdConfig *kbd) {
	(void)kbd;
}

void swkbdConfigSetGuideText(SwkbdConfig *kbd, const char *guide) {
	(void)kbd;
	(void)guide;
}

void swkbdConfigSetHeaderText(SwkbdConfig *kbd, const char *header) {
	if (!kbd || !header)
		return;
	snprintf(kbd->text, sizeof(kbd->text), "%s", header);
}

void swkbdConfigSetInitialText(SwkbdConfig *kbd, const char *text) {
	(void)kbd;
	(void)text;
}

void swkbdConfigSetStringLenMax(SwkbdConfig *kbd, size_t max) {
	(void)kbd;
	(void)max;
}

static void utf8_from_utf16(char *dst, size_t dst_size, const uint16_t *src, size_t src_len) {
	size_t o = 0;
	size_t i = 0;
	while (src && src[i] && i < src_len && o + 4 < dst_size) {
		uint32_t c = src[i];
		if (c >= 0xD800 && c <= 0xDBFF && src[i + 1] >= 0xDC00 && src[i + 1] <= 0xDFFF) {
			c = 0x10000 + ((c - 0xD800) << 10) + (src[i + 1] - 0xDC00);
			i++;
		}
		if (c < 0x80) {
			dst[o++] = (char)c;
		} else if (c < 0x800) {
			dst[o++] = (char)(0xC0 | (c >> 6));
			dst[o++] = (char)(0x80 | (c & 0x3F));
		} else if (c < 0x10000) {
			dst[o++] = (char)(0xE0 | (c >> 12));
			dst[o++] = (char)(0x80 | ((c >> 6) & 0x3F));
			dst[o++] = (char)(0x80 | (c & 0x3F));
		} else {
			dst[o++] = (char)(0xF0 | (c >> 18));
			dst[o++] = (char)(0x80 | ((c >> 12) & 0x3F));
			dst[o++] = (char)(0x80 | ((c >> 6) & 0x3F));
			dst[o++] = (char)(0x80 | (c & 0x3F));
		}
		i++;
	}
	dst[o] = '\0';
}

Result swkbdShow(SwkbdConfig *kbd, char *buf, size_t len) {
	oskParam param;
	oskInputFieldInfo info;
	oskCallbackReturnParam result;
	uint16_t message[128];
	uint16_t startText[512];
	uint16_t buffer[513];
	const char *title;
	size_t i;
	int rc;

	if (!kbd || !buf || len == 0)
		return -1;

	buf[0] = '\0';
	title = kbd->text[0] ? kbd->text : "";

	memset(&param, 0, sizeof(param));
	param.allowedPanels = OSK_PANEL_TYPE_DEFAULT;
	param.firstViewPanel = OSK_PANEL_TYPE_DEFAULT;

	memset(message, 0, sizeof(message));
	memset(startText, 0, sizeof(startText));
	memset(buffer, 0, sizeof(buffer));

	for (i = 0; i < sizeof(message) / sizeof(message[0]) - 1 && title[i]; i++)
		message[i] = (uint16_t)(unsigned char)title[i];

	info.message = message;
	info.startText = startText;
	info.maxLength = (s32)(sizeof(buffer) / sizeof(buffer[0]) - 1);

	rc = oskLoadAsync(0, &param, &info);
	if (rc != 0)
		return rc;

	memset(&result, 0, sizeof(result));
	ps3_diag_checkpoint("osk:wait");
	while (oskGetInputText(&result) == 0) {
		usleep(16000);
	}
	ps3_diag_checkpoint("osk:done");

	if (result.str)
		utf8_from_utf16(buf, len, result.str, (size_t)(result.len > 0 ? result.len : 0));

	oskUnloadAsync(&result);
	return 0;
}

void swkbdClose(SwkbdConfig *kbd) {
	(void)kbd;
}

void romfsInit(void) {
}

void romfsExit(void) {
}

u64 svcGetSystemTick(void) {
	static time_t s_boot_sec = 0;
	struct timeval tv;
	u64 sec;

	gettimeofday(&tv, NULL);
	if (s_boot_sec == 0)
		s_boot_sec = tv.tv_sec;
	sec = (tv.tv_sec >= s_boot_sec) ? (u64)(tv.tv_sec - s_boot_sec) : 0ULL;
	return sec * 19200000ULL + ((u64)tv.tv_usec * 192ULL) / 10ULL;
}

Result psmInitialize(void) {
	return 0;
}

void psmExit(void) {
}

Result psmGetBatteryChargePercentage(u32 *pct) {
	if (pct)
		*pct = 100;
	return 0;
}

Result psmGetChargerType(PsmChargerType *type) {
	if (type)
		*type = PsmChargerType_Unconnected;
	return 0;
}

Result nifmInitialize(NifmServiceType type) {
	(void)type;
	return 0;
}

Result nifmGetCurrentIpAddress(u32 address[4]) {
	if (address) {
		address[0] = 0;
		address[1] = 0;
		address[2] = 0;
		address[3] = 0;
	}
	return 0;
}

void nifmExit(void) {
}

Result socketInitializeDefault(void) {
	return 0;
}

void socketExit(void) {
}

Result webWifiCreate(WebWifiConfig *config, void *scratch, const char *url, Uuid uuid, u32 option) {
	(void)config;
	(void)scratch;
	(void)url;
	(void)uuid;
	(void)option;
	return 0;
}

void webWifiShow(WebWifiConfig *config, void *scratch) {
	(void)config;
	(void)scratch;
}

void webWifiDestroy(WebWifiConfig *config) {
	(void)config;
}

#include <math.h>

float SDL_roundf(float x) {
	return roundf(x);
}

double SDL_round(double x) {
	return round(x);
}

