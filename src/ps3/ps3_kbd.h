#ifndef PS3_KEYBOARD_H
#define PS3_KEYBOARD_H

#ifdef __cplusplus
extern "C" {
#endif

void ps3_kbd_get(char *title, const char *initial_text, int maxLen, int multiline, char *buf);

#ifdef __cplusplus
}
#endif

#endif
