#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#include <SDL.h>

#include "ps3_compat.h"
#include "ps3_kbd.h"
#include "ps3_diag.h"
#include "sysdeps.h"

#ifdef __PS3__

#include "uae_gui_ps3.h"

extern SDL_Surface *prSDLScreen;

#ifndef PS3_KBD_USE_OSK
#define PS3_KBD_USE_OSK 0
#endif

#if PS3_KBD_USE_OSK

#include <sysutil/sysutil.h>
#include <sysutil/osk.h>
#include <sys/memory.h>

#define PS3_OSK_CONTAINER_SIZE (4 * 1024 * 1024)
#define PS3_OSK_TEXT_MAX 256

static volatile int s_osk_unloaded = 0;
static volatile int s_osk_canceled = 0;
static oskCallbackReturnParam s_osk_ret;
static u16 s_osk_ret_utf[PS3_OSK_TEXT_MAX];

static void ps3_osk_callback(u64 status, u64 param, void *usrdata)
{
	(void)param;
	(void)usrdata;
	if (status == SYSUTIL_OSK_DONE) {
		memset(&s_osk_ret, 0, sizeof(s_osk_ret));
		s_osk_ret.str = s_osk_ret_utf;
		oskUnloadAsync(&s_osk_ret);
	} else if (status == SYSUTIL_OSK_INPUT_CANCELED) {
		s_osk_canceled = 1;
		memset(&s_osk_ret, 0, sizeof(s_osk_ret));
		s_osk_ret.str = s_osk_ret_utf;
		oskUnloadAsync(&s_osk_ret);
	} else if (status == SYSUTIL_OSK_UNLOADED) {
		s_osk_unloaded = 1;
	}
}

static void ps3_utf16_from_ascii(const char *src, u16 *dst, int max)
{
	int i = 0;
	if (src) {
		for (; src[i] && i < max - 1; i++)
			dst[i] = (u16)(unsigned char)src[i];
	}
	dst[i] = 0;
}

static void ps3_ascii_from_utf16(const u16 *src, char *dst, int max)
{
	int i = 0;
	if (src) {
		for (; src[i] && i < max - 1; i++)
			dst[i] = (char)(src[i] < 128 ? src[i] : '?');
	}
	dst[i] = '\0';
}

void ps3_kbd_get(char *title, const char *initial_text, int maxLen, int multiline, char *buf)
{
	(void)multiline;

	if (!buf)
		return;
	buf[0] = '\0';
	if (maxLen < 1)
		return;

	int text_max = (maxLen + 1) < PS3_OSK_TEXT_MAX ? (maxLen + 1) : PS3_OSK_TEXT_MAX;

	u16 title_utf[64];
	u16 init_utf[PS3_OSK_TEXT_MAX];
	ps3_utf16_from_ascii(title, title_utf, (int)(sizeof(title_utf) / sizeof(u16)));
	ps3_utf16_from_ascii(initial_text, init_utf, text_max);
	s_osk_ret_utf[0] = 0;

	sys_mem_container_t container = 0;
	if (sysMemContainerCreate(&container, PS3_OSK_CONTAINER_SIZE) < 0) {
		write_log("[PS3] kbd: container create failed\n");
		return;
	}

	oskParam param;
	memset(&param, 0, sizeof(param));
	param.allowedPanels = OSK_PANEL_TYPE_DEFAULT | OSK_PANEL_TYPE_ALPHABET;
	param.firstViewPanel = OSK_PANEL_TYPE_ALPHABET;
	param.controlPoint.x = 0.0f;
	param.controlPoint.y = 0.0f;
	param.prohibitFlags = OSK_PROHIBIT_RETURN;

	oskInputFieldInfo info;
	memset(&info, 0, sizeof(info));
	info.message = title_utf;
	info.startText = init_utf;
	info.maxLength = text_max > 1 ? text_max - 1 : 63;

	s_osk_unloaded = 0;
	s_osk_canceled = 0;
	s_osk_ret_utf[0] = 0;
	memset(&s_osk_ret, 0, sizeof(s_osk_ret));
	s_osk_ret.str = s_osk_ret_utf;

	sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT3, ps3_osk_callback, NULL);

	oskSetInitialKeyLayout(OSK_INITIAL_FULLKEY_PANEL);
	oskSetKeyLayoutOption(OSK_FULLKEY_PANEL);

	if (oskLoadAsync(container, &param, &info) != 0) {
		write_log("[PS3] kbd: oskLoadAsync failed\n");
		sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT3);
		sysMemContainerDestroy(container);
		return;
	}

	write_log("[PS3] kbd: osk loaded, entering pump loop\n");

	ps3_diag_checkpoint("kbd:osk-open");

	int loops = 0;
	while (!s_osk_unloaded && !s_osk_canceled && loops < 60000) {
		sysUtilCheckCallback();
		ps3_gui_flip();
		SDL_Delay(16);
		loops++;
		if ((loops % 120) == 0)
			write_log("[PS3] kbd: pump loop=%d unloaded=%d canceled=%d\n", loops, s_osk_unloaded, s_osk_canceled);
	}

	write_log("[PS3] kbd: pump loop exit loop=%d unloaded=%d canceled=%d\n", loops, s_osk_unloaded, s_osk_canceled);

	if (s_osk_canceled && !s_osk_unloaded) {
		int extra = 0;
		while (!s_osk_unloaded && extra < 240) {
			sysUtilCheckCallback();
			ps3_gui_flip();
			SDL_Delay(16);
			extra++;
		}
		write_log("[PS3] kbd: unload drain extra=%d unloaded=%d\n", extra, s_osk_unloaded);
	}

	sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT3);
	sysMemContainerDestroy(container);

	ps3_diag_checkpoint("kbd:osk-close");

	if (!s_osk_canceled) {
		const u16 *src = s_osk_ret.str ? s_osk_ret.str : s_osk_ret_utf;
		if (src && src[0]) {
			ps3_ascii_from_utf16(src, buf, maxLen);
			write_log("[PS3] kbd: entered '%s' (res %d len %d)\n", buf, (int)s_osk_ret.res, (int)s_osk_ret.len);
		} else {
			write_log("[PS3] kbd: empty input (res %d)\n", (int)s_osk_ret.res);
		}
	}
}

#else

#define PS3_KBD_TEXT_MAX 128
#define PS3_KBD_ROWS 5

static const char *s_kbd_rows[2][4] = {
	{ "1234567890-_", "qwertyuiop.", "asdfghjkl,'", "zxcvbnm:;!" },
	{ "1234567890-_", "QWERTYUIOP.", "ASDFGHJKL,'", "ZXCVBNM:;!" }
};
static const int s_kbd_row_cols[4] = { 12, 11, 10, 10 };
#define PS3_KBD_ACTION_COLS 5

static Uint32 kbd_sdl_color(unsigned int col)
{
	Uint8 r = (Uint8)((col >> 0) & 0xFF);
	Uint8 g = (Uint8)((col >> 8) & 0xFF);
	Uint8 b = (Uint8)((col >> 16) & 0xFF);
	return SDL_MapRGB(prSDLScreen->format, r, g, b);
}

static void kbd_action_rect(int col, float *x, float *w)
{
	static const float widths[PS3_KBD_ACTION_COLS] = { 120.0f, 200.0f, 88.0f, 88.0f, 88.0f };
	float total = 584.0f + 4 * 10.0f;
	float sx = 110.0f + (740.0f - total) * 0.5f;
	float cx = sx;
	int i;
	for (i = 0; i < col; i++)
		cx += widths[i] + 10.0f;
	*x = cx;
	*w = widths[col];
}

static void kbd_move_row(int row, int d, int *col)
{
	int n = (row < 4) ? s_kbd_row_cols[row] : PS3_KBD_ACTION_COLS;
	int c = (*col + d) % n;
	if (c < 0)
		c += n;
	*col = c;
}

static void kbd_apply_action(int sel_row, int sel_col, int shift, int *shift_out,
                             char *text, int text_max, int *commit, int *cancel)
{
	if (sel_row < 4) {
		const char *keys = s_kbd_rows[shift ? 1 : 0][sel_row];
		int n = s_kbd_row_cols[sel_row];
		if (sel_col < n) {
			int len = (int)strlen(text);
			if (len < text_max - 1) {
				text[len] = keys[sel_col];
				text[len + 1] = '\0';
			}
		}
		return;
	}
	switch (sel_col) {
	case 0:
		*shift_out = !shift;
		break;
	case 1: {
		int len = (int)strlen(text);
		if (len < text_max - 1) {
			text[len] = ' ';
			text[len + 1] = '\0';
		}
		break;
	}
	case 2: {
		int len = (int)strlen(text);
		if (len > 0)
			text[len - 1] = '\0';
		break;
	}
	case 3:
		*commit = 1;
		break;
	case 4:
		*cancel = 1;
		break;
	}
}

static int kbd_internal_get(char *title, const char *initial_text, int maxLen, char *buf)
{
	static char text[PS3_KBD_TEXT_MAX];
	int text_max = maxLen < PS3_KBD_TEXT_MAX ? maxLen : PS3_KBD_TEXT_MAX;
	int shift = 0;
	int sel_row = 0, sel_col = 0;
	int frame_count = 0, hold_frames = 0, rep_timer = 0;
	int commit = 0, cancel = 0;

	if (text_max < 2)
		return 0;

	text[0] = '\0';
	if (initial_text && initial_text[0]) {
		int i;
		for (i = 0; initial_text[i] && i < text_max - 1; i++)
			text[i] = initial_text[i];
		text[i] = '\0';
	}

	ps3_diag_checkpoint("kbd:gui-open");

	Ps3InputState input;
	memset(&input, 0, sizeof(input));

	while (!commit && !cancel) {
		ps3_gui_update_input(&input);
		frame_count++;

		if (frame_count > 8) {
			if (input.pressed & PS3_BTN_LEFT) kbd_move_row(sel_row, -1, &sel_col);
			if (input.pressed & PS3_BTN_RIGHT) kbd_move_row(sel_row, +1, &sel_col);
			if (input.pressed & PS3_BTN_UP) { sel_row = (sel_row + PS3_KBD_ROWS - 1) % PS3_KBD_ROWS; sel_col = 0; }
			if (input.pressed & PS3_BTN_DOWN) { sel_row = (sel_row + 1) % PS3_KBD_ROWS; sel_col = 0; }

			unsigned int dirs = input.held & (PS3_BTN_LEFT | PS3_BTN_RIGHT | PS3_BTN_UP | PS3_BTN_DOWN);
			if (dirs) {
				hold_frames++;
				if (hold_frames > 28) {
					rep_timer++;
					if (rep_timer >= 3) {
						rep_timer = 0;
						if (dirs & PS3_BTN_LEFT) kbd_move_row(sel_row, -1, &sel_col);
						if (dirs & PS3_BTN_RIGHT) kbd_move_row(sel_row, +1, &sel_col);
						if (dirs & PS3_BTN_UP) { sel_row = (sel_row + PS3_KBD_ROWS - 1) % PS3_KBD_ROWS; sel_col = 0; }
						if (dirs & PS3_BTN_DOWN) { sel_row = (sel_row + 1) % PS3_KBD_ROWS; sel_col = 0; }
					}
				}
			} else {
				hold_frames = 0;
				rep_timer = 0;
			}

			commit = 0;
			cancel = 0;
			if (input.pressed & PS3_BTN_CROSS)
				kbd_apply_action(sel_row, sel_col, shift, &shift, text, text_max, &commit, &cancel);
			if (input.pressed & (PS3_BTN_SQUARE | PS3_BTN_L2 | PS3_BTN_R2)) {
				int len = (int)strlen(text);
				if (len > 0)
					text[len - 1] = '\0';
			}
			if (input.pressed & PS3_BTN_TRIANGLE) {
				if (text[0] != '\0')
					commit = 1;
			}
			if (input.pressed & PS3_BTN_CIRCLE)
				cancel = 1;
		} else {
			commit = 0;
			cancel = 0;
		}

		if ((commit || cancel) && (input.held & (PS3_BTN_CROSS | PS3_BTN_CIRCLE)))
			SDL_Delay(120);

		SDL_FillRect(prSDLScreen, NULL, kbd_sdl_color(PS3_COLOR_OVERLAY_BG));
		ps3_draw_card_custom(110.0f, 26.0f, 740.0f, 476.0f, PS3_COLOR_HEADER, PS3_COLOR_FOCUS_BORDER);
		ps3_draw_text_centered(480.0f, 38.0f, PS3_COLOR_AMIGA_RED, 1.05f, title && title[0] ? title : "Input");

		char shown[PS3_KBD_TEXT_MAX + 2];
		int tlen = (int)strlen(text);
		int start = 0;
		if (tlen > 34)
			start = tlen - 34;
		if (tlen < (int)sizeof(shown) - 2) {
			memcpy(shown, text + start, (size_t)(tlen - start));
			shown[tlen - start] = '_';
			shown[tlen - start + 1] = '\0';
		}
		ps3_draw_rounded_rect(126.0f, 62.0f, 708.0f, 40.0f, 6.0f, PS3_COLOR_CARD);
		ps3_draw_text(140.0f, 70.0f, PS3_COLOR_TEXT_WHITE, 0.95f, shown);

		{
			float kw = 52.0f, kh = 42.0f, gap = 5.0f;
			int r;
			float row_y0 = 118.0f;
			for (r = 0; r < 4; r++) {
				const char *keys = s_kbd_rows[shift ? 1 : 0][r];
				int n = s_kbd_row_cols[r];
				float line_w = n * kw + (n - 1) * gap;
				float sx = 110.0f + (740.0f - line_w) * 0.5f;
				int c;
				for (c = 0; c < n; c++) {
					float kx = sx + c * (kw + gap);
					float ky = row_y0 + r * (kh + gap);
					unsigned int bg = (r == sel_row && c == sel_col) ? PS3_COLOR_FOCUS_BORDER : PS3_COLOR_CARD;
					ps3_draw_rounded_rect(kx, ky, kw, kh, 5.0f, bg);
					char s[2];
					s[0] = keys[c];
					s[1] = '\0';
					ps3_draw_text_centered(kx + kw * 0.5f, ky + 10.0f, PS3_COLOR_TEXT_WHITE, 0.92f, s);
				}
			}
		}

		{
			static const char *labels[PS3_KBD_ACTION_COLS] = { "SHIFT", "SPACE", "DEL", "OK", "END" };
			int c;
			float ky = 118.0f + 4 * 47.0f;
			for (c = 0; c < PS3_KBD_ACTION_COLS; c++) {
				float kx, kw;
				kbd_action_rect(c, &kx, &kw);
				unsigned int bg = (sel_row == 4 && c == sel_col) ? PS3_COLOR_FOCUS_BORDER : PS3_COLOR_CARD;
				if (c == 0 && shift && !(sel_row == 4 && c == sel_col))
					bg = PS3_COLOR_SUCCESS;
				ps3_draw_rounded_rect(kx, ky, kw, 42.0f, 5.0f, bg);
				ps3_draw_text_centered(kx + kw * 0.5f, ky + 10.0f, PS3_COLOR_TEXT_WHITE, 0.92f, labels[c]);
			}
		}

		ps3_draw_text_centered(480.0f, 432.0f, PS3_COLOR_TEXT_MUTED, 0.82f,
			"D-Pad move   X insert   SQUARE delete   TRIANGLE confirm   CANCEL: CIRCLE");

		ps3_gui_flip();
		SDL_Delay(20);
	}

	while (1) {
		ps3_gui_update_input(&input);
		if (!(input.held & (PS3_BTN_CROSS | PS3_BTN_CIRCLE | PS3_BTN_SQUARE | PS3_BTN_TRIANGLE)))
			break;
		SDL_Delay(10);
	}

	ps3_diag_checkpoint("kbd:gui-close");

	if (commit && text[0] != '\0') {
		int i;
		for (i = 0; text[i] && i < maxLen - 1; i++)
			buf[i] = text[i];
		buf[i] = '\0';
		write_log("[PS3] kbd: entered '%s'\n", buf);
		return 1;
	}

	write_log("[PS3] kbd: canceled\n");
	buf[0] = '\0';
	return 0;
}

void ps3_kbd_get(char *title, const char *initial_text, int maxLen, int multiline, char *buf)
{
	(void)multiline;

	if (!buf)
		return;
	buf[0] = '\0';
	if (maxLen < 1)
		return;

	ps3_gui_init();
	write_log("[PS3] kbd: internal keyboard open\n");
	kbd_internal_get(title, initial_text, maxLen, buf);
}

#endif

#else

void ps3_kbd_get(char *title, const char *initial_text, int maxLen, int multiline, char *buf) {
	SwkbdConfig kbd;

	(void)initial_text;
	(void)multiline;

	if (!buf)
		return;

	buf[0] = '\0';

	if (swkbdCreate(&kbd, 0) < 0)
		return;

	swkbdConfigMakePresetDefault(&kbd);

	if (title && title[0]) {
		swkbdConfigSetGuideText(&kbd, title);
		swkbdConfigSetHeaderText(&kbd, title);
	}

	swkbdConfigSetInitialText(&kbd, initial_text);
	swkbdConfigSetStringLenMax(&kbd, maxLen > 1 ? (size_t)(maxLen - 1) : 64);

	swkbdShow(&kbd, buf, (size_t)maxLen);
	swkbdClose(&kbd);
}

#endif
