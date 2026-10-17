#include <string.h>
#include <stdbool.h>

#include "ps3_compat.h"
#include "ps3_kbd.h"

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
