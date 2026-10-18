#ifndef UAE_GUI_PS3_H
#define UAE_GUI_PS3_H

#ifdef __PS3__

#include "ps3_compat.h"
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PS3_SCREEN_W 960
#define PS3_SCREEN_H 544
#define PS3_FOOTER_H 42.0f
#define PS3_LIST_START_Y 90.0f
#define PS3_LIST_ITEM_H 48.0f
#define PS3_LIST_ITEM_GAP 6.0f
#define PS3_LIST_BOTTOM_Y ((float)PS3_SCREEN_H - PS3_FOOTER_H - 4.0f)

#ifndef RGBA8
#define RGBA8(r,g,b,a) ((((a)&0xFF)<<24) | (((b)&0xFF)<<16) | (((g)&0xFF)<<8) | (((r)&0xFF)<<0))
#endif

#define PS3_COLOR_BG            RGBA8(15, 18, 26, 255)
#define PS3_COLOR_HEADER        RGBA8(22, 27, 39, 255)
#define PS3_COLOR_FOOTER        RGBA8(20, 24, 34, 255)
#define PS3_COLOR_CARD          RGBA8(28, 34, 48, 230)
#define PS3_COLOR_CARD_FOCUSED  RGBA8(38, 48, 70, 255)
#define PS3_COLOR_CARD_BORDER   RGBA8(50, 60, 85, 255)
#define PS3_COLOR_FOCUS_BORDER  RGBA8(229, 37, 33, 255)
#define PS3_COLOR_ACTIVE_PILL   RGBA8(229, 37, 33, 255)
#define PS3_COLOR_AMIGA_RED     RGBA8(229, 37, 33, 255)
#define PS3_COLOR_AMIGA_BLUE    RGBA8(0, 136, 204, 255)
#define PS3_COLOR_AMIGA_ORANGE  RGBA8(255, 140, 0, 255)
#define PS3_COLOR_AMIGA_GREEN   RGBA8(34, 197, 94, 255)
#define PS3_COLOR_TEXT_WHITE    RGBA8(255, 255, 255, 255)
#define PS3_COLOR_TEXT_MUTED    RGBA8(148, 163, 184, 255)
#define PS3_COLOR_TEXT_DIM      RGBA8(90, 105, 125, 255)
#define PS3_COLOR_ACCENT_GOLD   RGBA8(245, 158, 11, 255)
#define PS3_COLOR_SUCCESS       RGBA8(16, 185, 129, 255)
#define PS3_COLOR_DANGER        RGBA8(239, 68, 68, 255)
#define PS3_COLOR_OVERLAY_BG    RGBA8(8, 10, 16, 210)

typedef enum {
    PS3_TAB_LIBRARY = 0,
    PS3_TAB_FLOPPY,
    PS3_TAB_HARD_DISK,
    PS3_TAB_WHDLOAD,
    PS3_TAB_PRESETS,
    PS3_TAB_HARDWARE,
    PS3_TAB_DISPLAY,
    PS3_TAB_CONTROLS,
    PS3_TAB_SAVESTATES,
    PS3_TAB_SYSTEM,
    PS3_TAB_COUNT
} Ps3GuiTab;

#define PS3_BTN_CROSS        (1 << 0)
#define PS3_BTN_CIRCLE        (1 << 1)
#define PS3_BTN_SQUARE        (1 << 2)
#define PS3_BTN_TRIANGLE        (1 << 3)
#define PS3_BTN_L1        (1 << 4)
#define PS3_BTN_R1        (1 << 5)
#define PS3_BTN_L2       (1 << 6)
#define PS3_BTN_R2       (1 << 7)
#define PS3_BTN_PLUS     (1 << 8)
#define PS3_BTN_MINUS    (1 << 9)
#define PS3_BTN_UP       (1 << 10)
#define PS3_BTN_DOWN     (1 << 11)
#define PS3_BTN_LEFT     (1 << 12)
#define PS3_BTN_RIGHT    (1 << 13)

typedef struct {
    unsigned int buttons;
    int lx;
    int ly;
    int rx;
    int ry;
} Ps3PadData;

typedef struct {
    Ps3PadData pad;
    Ps3PadData prev_pad;
    unsigned int pressed;
    unsigned int released;
    unsigned int held;
    int touch_x;
    int touch_y;
    int touch_active;
    int touch_tap;
    int touch_hold_frames;
} Ps3InputState;

typedef struct {
    int battery_percent;
    bool is_charging;
    char time_str[16];
    char date_str[32];
} Ps3SystemInfo;

int  ps3_set_kickstart(int index, int load_rom);
int  ps3_confirm_eject_for_hard_disk_launch(void);
int  ps3_gui_init(void);
void ps3_gui_shutdown(void);
void ps3_gui_shutdown_final(void);
void ps3_gui_prepare_exit(void);
void ps3_gui_update_input(Ps3InputState *input);
void ps3_gui_flip(void);
void ps3_gui_update_system_info(Ps3SystemInfo *sysinfo);
void ps3_gui_save_as_started(void);

void ps3_draw_rounded_rect(float x, float y, float w, float h, float r, unsigned int color);
void ps3_draw_rounded_rect_outline(float x, float y, float w, float h, float r, float thickness, unsigned int color);
void ps3_draw_card(float x, float y, float w, float h, bool focused, bool active);
void ps3_draw_card_custom(float x, float y, float w, float h, unsigned int bg_col, unsigned int border_col);
void ps3_draw_header(const char *title, Ps3GuiTab current_tab, const Ps3SystemInfo *sysinfo);
void ps3_draw_footer(const char *left_hint, const char *right_hint);
void ps3_draw_tab_bar(Ps3GuiTab current_tab, float y);

typedef enum {
    PS3_GLYPH_CROSS = 0,
    PS3_GLYPH_CIRCLE,
    PS3_GLYPH_SQUARE,
    PS3_GLYPH_TRIANGLE,
    PS3_GLYPH_L1,
    PS3_GLYPH_R1,
    PS3_GLYPH_L2,
    PS3_GLYPH_R2,
    PS3_GLYPH_PLUS,
    PS3_GLYPH_MINUS,
    PS3_GLYPH_DPAD,
    PS3_GLYPH_STICK
} Ps3ButtonGlyph;

void ps3_draw_text(float x, float y, unsigned int color, float scale, const char *text);
void ps3_draw_textf(float x, float y, unsigned int color, float scale, const char *fmt, ...);
void ps3_draw_text_centered(float cx, float y, unsigned int color, float scale, const char *text);
void ps3_draw_text_right(float rx, float y, unsigned int color, float scale, const char *text);
void ps3_draw_text_wrapped(float x, float y, float max_w, unsigned int color, float scale, const char *text);
void ps3_truncate_text(const char *in_text, float max_w, float scale, char *out_buf, size_t out_size);
int  ps3_get_text_width(float scale, const char *text);
int  ps3_get_text_height(float scale);

void ps3_draw_button_glyph(float x, float y, Ps3ButtonGlyph glyph);
void ps3_draw_hint_item(float x, float y, Ps3ButtonGlyph glyph, const char *label);
void ps3_draw_badge(float x, float y, const char *label, unsigned int bg_col, unsigned int text_col);
void ps3_draw_boing_ball_icon(float cx, float cy, float radius, float rot_angle);
void ps3_draw_led(float x, float y, const char *label, bool state, unsigned int led_col);
void ps3_draw_button_item(float x, float y, float w, float h, const char *title, const char *subtitle, const char *badge, bool focused, bool active);
void ps3_draw_button_item_custom(float x, float y, float w, float h, const char *title, const char *subtitle, const char *badge, unsigned int badge_col, bool focused, bool active);
void ps3_draw_selector_item(float x, float y, float w, float h, const char *title, const char *current_value, bool focused);
int  ps3_list_visible_rows(float start_y, float item_h, float gap);
void ps3_draw_list_page_indicator(int selected_item, int total_items, int visible_items);
void ps3_draw_switch_item(float x, float y, float w, float h, const char *title, bool enabled, bool focused);
void ps3_draw_slider_item(float x, float y, float w, float h, const char *title, int val, int min, int max, const char *suffix, bool focused);

void ps3_show_message_box(const char *title, const char *message, const char *btn_label);
void ps3_show_about_box(void);
bool ps3_show_confirm_box(const char *title, const char *message, const char *yes_label, const char *no_label);
int  ps3_show_disk_swap_dialog(const char *disk_title, int current_model);
int  ps3_show_library_options_dialog(const char *current_folder);
void ps3_gui_show_splash(void);
void ps3_gui_draw_splash_progress(float progress, const char *status_text, float ball_angle, float bounce);
void ps3_gui_draw_progress(const char *title, const char *subtitle, float fraction, const char *item_name);
void ps3_library_initial_load(void);

void ps3_view_library(Ps3InputState *input, int *selected_item);
void ps3_library_mark_dirty(void);
void ps3_view_floppy(Ps3InputState *input, int *selected_item);
void ps3_view_hard_disk(Ps3InputState *input, int *selected_item);
void ps3_view_whdload(Ps3InputState *input, int *selected_item);
void ps3_view_presets(Ps3InputState *input, int *selected_item);
void ps3_view_hardware(Ps3InputState *input, int *selected_item);
void ps3_view_display(Ps3InputState *input, int *selected_item);
void ps3_view_controls(Ps3InputState *input, int *selected_item);
void ps3_view_savestates(Ps3InputState *input, int *selected_item);
void ps3_view_system(Ps3InputState *input, int *selected_item);

int  ps3_gui_run_browser(char *out_path, const char *start_dir, int disk_drive_idx);

bool ps3_gui_is_ftp_active(void);
void ps3_gui_stop_ftp(void);
int  ps3_ftp_start(void);
void ps3_ftp_stop(void);
int  ps3_ftp_is_running(void);
void ps3_ftp_get_ip(char *buf, size_t sz);
int  ps3_ftp_get_port(void);

int  run_mainMenu_switch(void);
int  run_overlay_switch(void);

#ifdef __cplusplus
}
#endif

#endif
#endif