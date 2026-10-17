#ifdef __PS3__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <sys/stat.h>
#include <dirent.h>

#include <SDL.h>
#include <SDL_image.h>

#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "options.h"
#include "uae.h"
#include "menu.h"
#include "menu_config.h"

#include "uae_gui_ps3.h"
#include "ps3_diag.h"

extern SDL_Surface *prSDLScreen;

static inline Uint32 to_sdl_color(unsigned int col)
{
    Uint8 r = (col >> 0) & 0xFF;
    Uint8 g = (col >> 8) & 0xFF;
    Uint8 b = (col >> 16) & 0xFF;
    return SDL_MapRGB(prSDLScreen->format, r, g, b);
}

#define MAX_ENTRIES 8192
#define MAX_PATH_LEN 512

typedef struct {
    char name[256];
    bool is_dir;
    size_t size;
} FileEntry;

static FileEntry s_entries[MAX_ENTRIES];
static int s_num_entries = 0;
static char s_current_dir[MAX_PATH_LEN] = "./roms";
static bool s_hdf_mode = false;
static bool s_cd_mode = false;
static bool s_conf_mode = false;
static bool s_lha_mode = false;
static bool s_state_mode = false;
static bool s_dir_picker_mode = false;
static bool s_kick_mode = false;
static SDL_Surface *s_cover_surf = NULL;
static char s_cover_loaded_path[MAX_PATH_LEN] = "";

static bool is_supported_ext(const char *name)
{
    const char *ext = strrchr(name, '.');
    if (!ext) {
        if (s_kick_mode && strncasecmp(name, "kick", 4) == 0) return true;
        return false;
    }

    if (s_dir_picker_mode) {
        return true;
    }
    if (s_kick_mode) {
        return (strcasecmp(ext, ".rom") == 0 ||
                strcasecmp(ext, ".bin") == 0 ||
                strcasecmp(ext, ".a500") == 0 ||
                strcasecmp(ext, ".a1200") == 0 ||
                strcasecmp(ext, ".a4000") == 0 ||
                strcasecmp(ext, ".a600") == 0 ||
                strcasecmp(ext, ".cd32") == 0 ||
                strcasecmp(ext, ".ext") == 0 ||
                strcasecmp(ext, ".adf") == 0);
    }
    if (s_conf_mode) {
        return (strcasecmp(ext, ".conf") == 0);
    }
    if (s_lha_mode) {
        return (strcasecmp(ext, ".lha") == 0 || strcasecmp(ext, ".lzh") == 0);
    }
    if (s_state_mode) {
        return (strcasecmp(ext, ".asf") == 0);
    }
    if (s_cd_mode) {
        return (strcasecmp(ext, ".iso") == 0 ||
                strcasecmp(ext, ".cue") == 0 ||
                strcasecmp(ext, ".chd") == 0 ||
                strcasecmp(ext, ".bin") == 0 ||
                strcasecmp(ext, ".img") == 0 ||
                strcasecmp(ext, ".zip") == 0 ||
                strcasecmp(ext, ".7z") == 0);
    }
    if (s_hdf_mode) {
        return (strcasecmp(ext, ".hdf") == 0 ||
                strcasecmp(ext, ".hda") == 0 ||
                strcasecmp(ext, ".vhd") == 0 ||
                strcasecmp(ext, ".chd") == 0 ||
                strcasecmp(ext, ".zip") == 0 ||
                strcasecmp(ext, ".7z") == 0 ||
                strcasecmp(ext, ".gz") == 0 ||
                strcasecmp(ext, ".xz") == 0);
    }

    if (strcasecmp(ext, ".adf") == 0 ||
        strcasecmp(ext, ".adz") == 0 ||
        strcasecmp(ext, ".dms") == 0 ||
        strcasecmp(ext, ".m3u") == 0 ||
        strcasecmp(ext, ".chd") == 0 ||
        strcasecmp(ext, ".lha") == 0 ||
        strcasecmp(ext, ".lzh") == 0 ||
        strcasecmp(ext, ".ipf") == 0 ||
        strcasecmp(ext, ".hdf") == 0 ||
        strcasecmp(ext, ".zip") == 0 ||
        strcasecmp(ext, ".7z") == 0 ||
        strcasecmp(ext, ".gz") == 0 ||
        strcasecmp(ext, ".xz") == 0 ||
        strcasecmp(ext, ".bz2") == 0 ||
        strcasecmp(ext, ".iso") == 0 ||
        strcasecmp(ext, ".cue") == 0 ||
        strcasecmp(ext, ".fdi") == 0) {
        return true;
    }
    return false;
}

static int compare_entries(const void *a, const void *b)
{
    const FileEntry *ea = (const FileEntry *)a;
    const FileEntry *eb = (const FileEntry *)b;

    if (strcmp(ea->name, "..") == 0) return -1;
    if (strcmp(eb->name, "..") == 0) return 1;

    if (ea->is_dir && !eb->is_dir) return -1;
    if (!ea->is_dir && eb->is_dir) return 1;

    return strcasecmp(ea->name, eb->name);
}

static void scan_directory(const char *path)
{
    ps3_diag_checkpoint("scan:dir");
    s_num_entries = 0;

    if (strcmp(path, "./") != 0 && strcmp(path, ".") != 0 && strcmp(path, "/") != 0 && strlen(path) > 1) {
        strncpy(s_entries[0].name, "..", sizeof(s_entries[0].name));
        s_entries[0].is_dir = true;
        s_entries[0].size = 0;
        s_num_entries = 1;
    }

    DIR *dfd = opendir(path);
    if (dfd) {
        struct dirent *dir;
        while ((dir = readdir(dfd)) != NULL && s_num_entries < MAX_ENTRIES) {
            if (dir->d_name[0] == '.' && dir->d_name[1] != '.') {
                continue;
            }
            char full[1024];
            snprintf(full, sizeof(full), "%s/%s", path, dir->d_name);
            struct stat st;
            bool is_dir = false;
            size_t fsz = 0;
            if (stat(full, &st) == 0) {
                is_dir = S_ISDIR(st.st_mode);
                fsz = (size_t)st.st_size;
            }
            const char *ext = strrchr(dir->d_name, '.');
            bool supported = is_supported_ext(dir->d_name);
            if (is_dir || supported) {
                strncpy(s_entries[s_num_entries].name, dir->d_name, sizeof(s_entries[s_num_entries].name) - 1);
                s_entries[s_num_entries].name[sizeof(s_entries[s_num_entries].name) - 1] = '\0';
                s_entries[s_num_entries].is_dir = is_dir;
                s_entries[s_num_entries].size = fsz;
                s_num_entries++;
            }
        }
        closedir(dfd);
    }

    if (s_num_entries > 0) {
        qsort(s_entries, s_num_entries, sizeof(FileEntry), compare_entries);
    }
}

static void try_load_cover(const char *dir_path, const char *filename)
{
    ps3_diag_checkpoint("scan:cover");
    if (s_cover_surf) {
        SDL_FreeSurface(s_cover_surf);
        s_cover_surf = NULL;
    }
    s_cover_loaded_path[0] = '\0';

    if (!filename || strlen(filename) == 0)
        return;

    char base_no_ext[256];
    strncpy(base_no_ext, filename, sizeof(base_no_ext) - 1);
    base_no_ext[sizeof(base_no_ext) - 1] = '\0';
    char *dot = strrchr(base_no_ext, '.');
    if (dot) *dot = '\0';

    char test_paths[5][MAX_PATH_LEN];
    snprintf(test_paths[0], MAX_PATH_LEN, "%s/%s.png", dir_path, base_no_ext);
    snprintf(test_paths[1], MAX_PATH_LEN, "%s/%s.jpg", dir_path, base_no_ext);
    snprintf(test_paths[2], MAX_PATH_LEN, "./data/covers/%s.png", base_no_ext);
    snprintf(test_paths[3], MAX_PATH_LEN, "./data/covers/%s.jpg", base_no_ext);
    snprintf(test_paths[4], MAX_PATH_LEN, "./data/thumbs/%s.png", base_no_ext);

    for (int i = 0; i < 5; i++) {
        struct stat st;
        if (stat(test_paths[i], &st) == 0) {
            s_cover_surf = IMG_Load(test_paths[i]);
            if (s_cover_surf) {
                strncpy(s_cover_loaded_path, test_paths[i], sizeof(s_cover_loaded_path) - 1);
                break;
            }
        }
    }
}

static char get_entry_letter(const char *name)
{
    if (!name || name[0] == '\0') return ' ';
    int idx = 0;
    while (name[idx] != '\0' && (name[idx] == '[' || name[idx] == '(' || name[idx] == '_' || name[idx] == '.' || name[idx] == ' ' || name[idx] == '-')) {
        idx++;
    }
    if (name[idx] == '\0') return (char)toupper((unsigned char)name[0]);
    return (char)toupper((unsigned char)name[idx]);
}

static int find_next_letter_index(int current_idx, int direction)
{
    if (s_num_entries <= 1) return 0;
    if (current_idx < 0) current_idx = 0;
    if (current_idx >= s_num_entries) current_idx = s_num_entries - 1;
    char cur_char = get_entry_letter(s_entries[current_idx].name);

    if (direction > 0) {
        for (int i = current_idx + 1; i < s_num_entries; i++) {
            char c = get_entry_letter(s_entries[i].name);
            if (c != cur_char) {
                return i;
            }
        }
        return 0;
    } else {
        for (int i = current_idx - 1; i >= 0; i--) {
            char c = get_entry_letter(s_entries[i].name);
            if (c != cur_char) {
                while (i > 0 && get_entry_letter(s_entries[i - 1].name) == c) {
                    i--;
                }
                return i;
            }
        }
        return s_num_entries - 1;
    }
}

int ps3_gui_run_browser(char *out_path, const char *start_dir, int disk_drive_idx)
{
    s_hdf_mode = (disk_drive_idx >= 4 && disk_drive_idx < 8);
    s_cd_mode = (disk_drive_idx == 8);
    s_conf_mode = (disk_drive_idx == 9);
    s_lha_mode = (disk_drive_idx == 11);
    s_state_mode = (disk_drive_idx == 10);
    s_dir_picker_mode = (disk_drive_idx == 12);
    s_kick_mode = (disk_drive_idx == 13);
    if (start_dir && strlen(start_dir) > 0) {
        strncpy(s_current_dir, start_dir, sizeof(s_current_dir) - 1);
    }

    scan_directory(s_current_dir);

    int selected_idx = 0;
    int scroll_offset = 0;
    int items_per_page = 9;
    int last_cover_idx = -1;

    int hold_up_counter = 0;
    int hold_down_counter = 0;

    Ps3InputState input;
    memset(&input, 0, sizeof(input));
    Ps3SystemInfo sysinfo;
    memset(&sysinfo, 0, sizeof(sysinfo));

    while (1) {
        ps3_gui_update_input(&input);
        ps3_gui_update_system_info(&sysinfo);

        if (input.pressed & PS3_BTN_UP && s_num_entries > 0) {
            selected_idx--;
            if (selected_idx < 0) selected_idx = s_num_entries - 1;
        }
        if (input.pressed & PS3_BTN_DOWN && s_num_entries > 0) {
            selected_idx++;
            if (selected_idx >= s_num_entries) selected_idx = 0;
        }

        if (input.held & PS3_BTN_UP && s_num_entries > 0) {
            hold_up_counter++;
            if (hold_up_counter > 12 && (hold_up_counter % 3 == 0)) {
                selected_idx--;
                if (selected_idx < 0) selected_idx = s_num_entries - 1;
            }
        } else {
            hold_up_counter = 0;
        }

        if (input.held & PS3_BTN_DOWN && s_num_entries > 0) {
            hold_down_counter++;
            if (hold_down_counter > 12 && (hold_down_counter % 3 == 0)) {
                selected_idx++;
                if (selected_idx >= s_num_entries) selected_idx = 0;
            }
        } else {
            hold_down_counter = 0;
        }
        if (input.pressed & PS3_BTN_L2) {
            selected_idx -= items_per_page;
            if (selected_idx < 0) selected_idx = 0;
        }
        if (input.pressed & PS3_BTN_R2 && s_num_entries > 0) {
            selected_idx += items_per_page;
            if (selected_idx >= s_num_entries) selected_idx = s_num_entries - 1;
        }

        if (input.pressed & (PS3_BTN_L1 | PS3_BTN_LEFT)) {
            selected_idx = find_next_letter_index(selected_idx, -1);
        }
        if (input.pressed & (PS3_BTN_R1 | PS3_BTN_RIGHT)) {
            selected_idx = find_next_letter_index(selected_idx, 1);
        }
        if (!s_dir_picker_mode && (input.pressed & PS3_BTN_TRIANGLE)) {
            selected_idx = find_next_letter_index(selected_idx, 1);
        }

        if (selected_idx < scroll_offset) {
            scroll_offset = selected_idx;
        }
        if (selected_idx >= scroll_offset + items_per_page) {
            scroll_offset = selected_idx - items_per_page + 1;
        }

        if (input.touch_tap) {
            float list_x = 20.0f;
            float list_y = 92.0f;
            float item_h = 42.0f;
            if (input.touch_x >= list_x && input.touch_x <= list_x + 580.0f) {
                int touched_item = (int)((input.touch_y - list_y) / (item_h + 4.0f));
                if (touched_item >= 0 && touched_item < items_per_page) {
                    int real_idx = scroll_offset + touched_item;
                    if (real_idx >= 0 && real_idx < s_num_entries) {
                        if (selected_idx == real_idx) {
                            input.pressed |= PS3_BTN_CROSS;
                        } else {
                            selected_idx = real_idx;
                        }
                    }
                }
            }
        }

        if (selected_idx != last_cover_idx && selected_idx >= 0 && selected_idx < s_num_entries) {
            last_cover_idx = selected_idx;
            if (!s_entries[selected_idx].is_dir) {
                try_load_cover(s_current_dir, s_entries[selected_idx].name);
            } else {
                if (s_cover_surf) {
                    SDL_FreeSurface(s_cover_surf);
                    s_cover_surf = NULL;
                }
            }
        }

        if (input.pressed & PS3_BTN_CROSS) {
            if (selected_idx >= 0 && selected_idx < s_num_entries) {
                FileEntry *ent = &s_entries[selected_idx];
                if (ent->is_dir) {
                    if (strcmp(ent->name, "..") == 0) {
                        char *last_slash = strrchr(s_current_dir, '/');
                        if (last_slash && last_slash != s_current_dir) {
                            *last_slash = '\0';
                        } else if (last_slash == s_current_dir) {
                            strcpy(s_current_dir, "/");
                        } else if (strcmp(s_current_dir, ".") == 0) {
                            strcpy(s_current_dir, "/");
                        } else {
                            strcpy(s_current_dir, ".");
                        }
                    } else {
                        char new_dir[MAX_PATH_LEN];
                        snprintf(new_dir, sizeof(new_dir), "%s/%s", s_current_dir, ent->name);
                        strncpy(s_current_dir, new_dir, sizeof(s_current_dir) - 1);
                    }
                    scan_directory(s_current_dir);
                    selected_idx = 0;
                    scroll_offset = 0;
                    last_cover_idx = -1;
                } else {
                    if (s_dir_picker_mode) {
                        strncpy(out_path, s_current_dir, MAX_PATH_LEN - 1);
                        out_path[MAX_PATH_LEN - 1] = '\0';
                        if (s_cover_surf) {
                            SDL_FreeSurface(s_cover_surf);
                            s_cover_surf = NULL;
                        }
                        while (1) {
                            ps3_gui_update_input(&input);
                            if (!(input.pad.buttons & (PS3_BTN_CROSS | PS3_BTN_CIRCLE | PS3_BTN_SQUARE | PS3_BTN_TRIANGLE)))
                                break;
                            SDL_Delay(10);
                        }
                        return 1;
                    }
                    snprintf(out_path, MAX_PATH_LEN, "%s/%s", s_current_dir, ent->name);
                    if (s_cover_surf) {
                        SDL_FreeSurface(s_cover_surf);
                        s_cover_surf = NULL;
                    }
                    while (1) {
                        ps3_gui_update_input(&input);
                        if (!(input.pad.buttons & (PS3_BTN_CROSS | PS3_BTN_CIRCLE | PS3_BTN_SQUARE | PS3_BTN_TRIANGLE)))
                            break;
                        SDL_Delay(10);
                    }
                    return 1;
                }
            }
        }

        if (s_dir_picker_mode && (input.pressed & PS3_BTN_TRIANGLE)) {
            strncpy(out_path, s_current_dir, MAX_PATH_LEN - 1);
            out_path[MAX_PATH_LEN - 1] = '\0';
            if (s_cover_surf) {
                SDL_FreeSurface(s_cover_surf);
                s_cover_surf = NULL;
            }
            while (1) {
                ps3_gui_update_input(&input);
                if (!(input.pad.buttons & (PS3_BTN_CROSS | PS3_BTN_CIRCLE | PS3_BTN_SQUARE | PS3_BTN_TRIANGLE)))
                    break;
                SDL_Delay(10);
            }
            return 1;
        }

        if (input.pressed & PS3_BTN_CIRCLE) {
            if (s_cover_surf) {
                SDL_FreeSurface(s_cover_surf);
                s_cover_surf = NULL;
            }
            while (1) {
                ps3_gui_update_input(&input);
                if (!(input.pad.buttons & (PS3_BTN_CROSS | PS3_BTN_CIRCLE | PS3_BTN_SQUARE | PS3_BTN_TRIANGLE)))
                    break;
                SDL_Delay(10);
            }
            return 0;
        }

        if (input.pressed & PS3_BTN_SQUARE) {
            out_path[0] = '\0';
            if (s_cover_surf) {
                SDL_FreeSurface(s_cover_surf);
                s_cover_surf = NULL;
            }
            while (1) {
                ps3_gui_update_input(&input);
                if (!(input.pad.buttons & (PS3_BTN_CROSS | PS3_BTN_CIRCLE | PS3_BTN_SQUARE | PS3_BTN_TRIANGLE)))
                    break;
                SDL_Delay(10);
            }
            return 2;
        }

        SDL_FillRect(prSDLScreen, NULL, to_sdl_color(PS3_COLOR_BG));

        char browser_title[128];
        if (s_dir_picker_mode)
            snprintf(browser_title, sizeof(browser_title), "SELECT ROM DIRECTORY");
        else if (disk_drive_idx == 11)
            snprintf(browser_title, sizeof(browser_title), "SELECT WHDLOAD LHA");
        else if (disk_drive_idx == 9)
            snprintf(browser_title, sizeof(browser_title), "SELECT CONFIGURATION");
        else if (disk_drive_idx == 10)
            snprintf(browser_title, sizeof(browser_title), "SELECT SAVE STATE");
        else if (s_hdf_mode)
            snprintf(browser_title, sizeof(browser_title), "SELECT HDF FOR SLOT %d", disk_drive_idx - 3);
        else if (s_cd_mode)
            snprintf(browser_title, sizeof(browser_title), "SELECT CD32 IMAGE");
        else
            snprintf(browser_title, sizeof(browser_title), "INSERT DISK INTO DF%d", disk_drive_idx);
        ps3_draw_header(browser_title, s_dir_picker_mode ? PS3_TAB_LIBRARY : (s_hdf_mode ? PS3_TAB_HARD_DISK : PS3_TAB_FLOPPY), &sysinfo);

        ps3_draw_card_custom(20.0f, 52.0f, PS3_SCREEN_W - 40.0f, 34.0f, RGBA8(22, 28, 40, 255), PS3_COLOR_CARD_BORDER);
        ps3_draw_text(34.0f, 60.0f, PS3_COLOR_AMIGA_BLUE, 0.95f, "DIR:");

        char path_buf[128];
        ps3_truncate_text(s_current_dir, PS3_SCREEN_W - 220.0f, 0.95f, path_buf, sizeof(path_buf));
        ps3_draw_text(80.0f, 60.0f, PS3_COLOR_TEXT_WHITE, 0.95f, path_buf);

        char count_str[64];
        snprintf(count_str, sizeof(count_str), "%d files", s_num_entries);
        ps3_draw_text_right(PS3_SCREEN_W - 36.0f, 60.0f, PS3_COLOR_TEXT_MUTED, 0.85f, count_str);

        float list_x = 20.0f;
        float list_y = 90.0f;
        float list_w = 580.0f;
        float item_h = 42.0f;

        for (int i = 0; i < items_per_page; i++) {
            int entry_idx = scroll_offset + i;
            float item_y = list_y + (float)i * (item_h + 4.0f);

            if (entry_idx < s_num_entries) {
                FileEntry *entry = &s_entries[entry_idx];
                bool is_sel = (entry_idx == selected_idx);

                ps3_draw_card(list_x, item_y, list_w, item_h, is_sel, false);

                char name_buf[128];
                float max_name_w = list_w - 260.0f;
                ps3_truncate_text(entry->name, max_name_w, 0.95f, name_buf, sizeof(name_buf));

                if (entry->is_dir) {
                    ps3_draw_badge(list_x + 12.0f, item_y + 10.0f, "DIR", RGBA8(40, 55, 80, 255), PS3_COLOR_AMIGA_BLUE);
                    ps3_draw_text(list_x + 62.0f, item_y + 11.0f, is_sel ? PS3_COLOR_TEXT_WHITE : RGBA8(220, 235, 255, 255), 0.95f, name_buf);
                } else {
                    const char *ext = strrchr(entry->name, '.');
                    const char *type_tag = ext ? (ext + 1) : "ADF";
                    Uint32 b_bg = RGBA8(40, 40, 50, 255);
                    Uint32 b_fg = PS3_COLOR_TEXT_WHITE;
                    if (ext) {
                        if (!strcasecmp(ext, ".adf") || !strcasecmp(ext, ".adz") || !strcasecmp(ext, ".ipf") || !strcasecmp(ext, ".dms")) {
                            b_bg = RGBA8(12, 45, 75, 255); b_fg = RGBA8(14, 165, 233, 255);
                        } else if (!strcasecmp(ext, ".m3u")) {
                            b_bg = RGBA8(65, 35, 15, 255); b_fg = RGBA8(249, 115, 22, 255);
                        } else if (!strcasecmp(ext, ".lha") || !strcasecmp(ext, ".lzh")) {
                            b_bg = RGBA8(60, 50, 15, 255); b_fg = RGBA8(234, 179, 8, 255);
                        } else if (!strcasecmp(ext, ".zip") || !strcasecmp(ext, ".7z")) {
                            b_bg = RGBA8(65, 20, 45, 255); b_fg = RGBA8(236, 72, 153, 255);
                        } else if (!strcasecmp(ext, ".chd") || !strcasecmp(ext, ".iso") || !strcasecmp(ext, ".cue") || !strcasecmp(ext, ".bin") || !strcasecmp(ext, ".img")) {
                            b_bg = RGBA8(60, 20, 20, 255); b_fg = RGBA8(229, 37, 33, 255);
                        } else if (!strcasecmp(ext, ".hdf") || !strcasecmp(ext, ".hda") || !strcasecmp(ext, ".vhd")) {
                            b_bg = RGBA8(15, 55, 35, 255); b_fg = RGBA8(16, 185, 129, 255);
                        }
                    }
                    ps3_draw_badge(list_x + 12.0f, item_y + 10.0f, type_tag, b_bg, b_fg);
                    ps3_draw_text(list_x + 62.0f, item_y + 11.0f, is_sel ? PS3_COLOR_TEXT_WHITE : RGBA8(220, 230, 245, 255), 0.95f, name_buf);

                    char sz_buf[32];
                    if (entry->size >= 1024 * 1024) {
                        snprintf(sz_buf, sizeof(sz_buf), "%.1f MB", (float)entry->size / (1024.0f * 1024.0f));
                    } else {
                        snprintf(sz_buf, sizeof(sz_buf), "%u KB", (unsigned int)(entry->size / 1024));
                    }
                    ps3_draw_text_right(list_x + list_w - 14.0f, item_y + 11.0f, PS3_COLOR_TEXT_MUTED, 0.85f, sz_buf);
                }
            }
        }

        float preview_x = 616.0f;
        float preview_y = 90.0f;
        float preview_w = 324.0f;
        float preview_h = 404.0f;

        ps3_draw_card_custom(preview_x, preview_y, preview_w, preview_h, PS3_COLOR_CARD, PS3_COLOR_CARD_BORDER);

        if (selected_idx >= 0 && selected_idx < s_num_entries) {
            FileEntry *sel_entry = &s_entries[selected_idx];

            ps3_draw_badge(preview_x + 16.0f, preview_y + 12.0f,
                s_dir_picker_mode ? (sel_entry->is_dir ? "DIRECTORY" : "FILE IN FOLDER") : (sel_entry->is_dir ? "DIRECTORY INFO" : (s_cd_mode ? "CD IMAGE INFO" : "AMIGA DISK INFO")),
                s_dir_picker_mode ? PS3_COLOR_AMIGA_BLUE : PS3_COLOR_AMIGA_RED, PS3_COLOR_TEXT_WHITE);

            if (s_cover_surf) {
                float img_max_w = preview_w - 32.0f;
                float img_max_h = 240.0f;
                float surf_w = (float)s_cover_surf->w;
                float surf_h = (float)s_cover_surf->h;

                float scale_x = img_max_w / surf_w;
                float scale_y = img_max_h / surf_h;
                float final_scale = (scale_x < scale_y) ? scale_x : scale_y;
                if (final_scale > 1.0f) final_scale = 1.0f;

                int draw_w = (int)(surf_w * final_scale);
                int draw_h = (int)(surf_h * final_scale);
                int draw_x = (int)(preview_x + (preview_w - draw_w) * 0.5f);
                int draw_y = (int)(preview_y + 44.0f + (img_max_h - draw_h) * 0.5f);

                SDL_Rect dst_r = { (Sint16)draw_x, (Sint16)draw_y, (Uint16)draw_w, (Uint16)draw_h };
                SDL_SoftStretch(s_cover_surf, NULL, prSDLScreen, &dst_r);
            } else {
                float ph_w = preview_w - 32.0f;
                float ph_h = 220.0f;
                float ph_x = preview_x + 16.0f;
                float ph_y = preview_y + 44.0f;

                ps3_draw_rounded_rect(ph_x, ph_y, ph_w, ph_h, 6.0f, RGBA8(18, 22, 32, 255));
                ps3_draw_boing_ball_icon(ph_x + (ph_w * 0.5f), ph_y + 80.0f, 32.0f, 0.0f);
                ps3_draw_text_centered(ph_x + (ph_w * 0.5f), ph_y + 135.0f, PS3_COLOR_TEXT_MUTED, 0.95f, sel_entry->is_dir ? "Folder" : "Amiga Disk Image");
                ps3_draw_text_centered(ph_x + (ph_w * 0.5f), ph_y + 160.0f, PS3_COLOR_TEXT_DIM, 0.85f, "Cover: <name>.png");
            }


            ps3_draw_text(preview_x + 16.0f, preview_y + 278.0f, PS3_COLOR_TEXT_DIM, 0.80f, sel_entry->is_dir ? "DIRECTORY:" : "FILE NAME:");


            char name_buf[128];
            ps3_truncate_text(sel_entry->name, preview_w - 32.0f, 0.90f, name_buf, sizeof(name_buf));
            ps3_draw_text(preview_x + 16.0f, preview_y + 298.0f, PS3_COLOR_TEXT_WHITE, 0.90f, name_buf);


            if (!sel_entry->is_dir) {
                char size_txt[64];
                char size_buf[64];
                const char *ext = strrchr(sel_entry->name, '.');
                const char *type_desc = "Disk Image";
                if (ext) {
                    if (!strcasecmp(ext, ".adf")) type_desc = "Standard ADF";
                    else if (!strcasecmp(ext, ".m3u")) type_desc = "M3U Multi-Disk Playlist";
                    else if (!strcasecmp(ext, ".chd")) type_desc = "CHD Compressed Image";
                    else if (!strcasecmp(ext, ".ipf")) type_desc = "CAPS / IPF Image";
                    else if (!strcasecmp(ext, ".adz")) type_desc = "Compressed ADF";
                    else if (!strcasecmp(ext, ".dms")) type_desc = "DMS Disk";
                    else if (!strcasecmp(ext, ".iso") || !strcasecmp(ext, ".cue") || !strcasecmp(ext, ".bin") || !strcasecmp(ext, ".img")) type_desc = "CD Image";
                    else if (!strcasecmp(ext, ".lha") || !strcasecmp(ext, ".lzh")) type_desc = "LHA Archive";
                    else if (!strcasecmp(ext, ".zip")) type_desc = "ZIP Archive";
                    else if (!strcasecmp(ext, ".hdf")) type_desc = "Hard Disk Image";
                    else if (!strcasecmp(ext, ".conf")) type_desc = "UAE4All Configuration";
                    else if (!strcasecmp(ext, ".asf")) type_desc = "Amiga Save State";
                }
                snprintf(size_txt, sizeof(size_txt), "Size: %u KB (%s)", (unsigned int)(sel_entry->size / 1024), type_desc);
                ps3_truncate_text(size_txt, preview_w - 32.0f, 0.85f, size_buf, sizeof(size_buf));
                ps3_draw_text(preview_x + 16.0f, preview_y + 326.0f, PS3_COLOR_TEXT_MUTED, 0.85f, size_buf);
            } else {
                ps3_draw_text(preview_x + 16.0f, preview_y + 326.0f, PS3_COLOR_TEXT_MUTED, 0.85f, "Folder / Subdirectory");
            }
        }

        SDL_Rect ftr_r = { 0, PS3_SCREEN_H - 42, PS3_SCREEN_W, 42 };
        SDL_FillRect(prSDLScreen, &ftr_r, to_sdl_color(PS3_COLOR_FOOTER));
        ps3_draw_rounded_rect_outline(0, PS3_SCREEN_H - 42, PS3_SCREEN_W, 42, 0.0f, 1.0f, PS3_COLOR_CARD_BORDER);

        float cur_x = 20.0f;
        float btn_y = PS3_SCREEN_H - 32.0f;

        if (s_dir_picker_mode) {
            ps3_draw_hint_item(cur_x, btn_y, PS3_GLYPH_CROSS, "Open Folder");
            cur_x += 140.0f;
            ps3_draw_hint_item(cur_x, btn_y, PS3_GLYPH_TRIANGLE, "Select Folder");
            cur_x += 160.0f;
            ps3_draw_hint_item(cur_x, btn_y, PS3_GLYPH_CIRCLE, "Cancel");
        } else {
            ps3_draw_hint_item(cur_x, btn_y, PS3_GLYPH_CROSS, "Insert");
            cur_x += 115.0f;

            ps3_draw_hint_item(cur_x, btn_y, PS3_GLYPH_CIRCLE, "Back");
            cur_x += 100.0f;

            ps3_draw_hint_item(cur_x, btn_y, PS3_GLYPH_SQUARE, "Eject");
            cur_x += 110.0f;

            ps3_draw_hint_item(cur_x, btn_y, PS3_GLYPH_L1, "");
            ps3_draw_hint_item(cur_x + 35.0f, btn_y, PS3_GLYPH_R1, "Letter A-Z");
        }

        ps3_draw_hint_item(PS3_SCREEN_W - 200.0f, btn_y, PS3_GLYPH_L2, "");
        ps3_draw_hint_item(PS3_SCREEN_W - 160.0f, btn_y, PS3_GLYPH_R2, "Page Up/Down");

        SDL_Flip(prSDLScreen);
        SDL_Delay(16);
    }
}

#endif
