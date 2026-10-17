#include "m3u_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

extern char uae4all_image_file0[256];
extern char changed_df[4][256];
extern int real_changed_df[4];
extern void disk_insert(int num, const char *name);
extern void ps3_osd_show(const char *text, int duration_ms);

struct m3u_playlist g_m3u = { {0}, {{0}}, {{0}}, 0, 0, 0 };

static void trim_whitespace(char *str)
{
    char *end;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
}

void m3u_clear(void)
{
    memset(&g_m3u, 0, sizeof(g_m3u));
}

int m3u_load(const char *filepath)
{
    m3u_clear();
    if (!filepath || filepath[0] == '\0') return 0;

    FILE *f = fopen(filepath, "r");
    if (!f) return 0;

    char dir[512];
    strncpy(dir, filepath, sizeof(dir) - 1);
    dir[sizeof(dir) - 1] = '\0';
    char *last_slash = strrchr(dir, '/');
    if (!last_slash) last_slash = strrchr(dir, '\\');
    if (last_slash) {
        *last_slash = '\0';
    } else {
        strcpy(dir, ".");
    }

    char line[512];
    while (fgets(line, sizeof(line), f) && g_m3u.disk_count < MAX_M3U_DISKS) {
        char *p = line;
        while (*p && (*p == ' ' || *p == '\t')) p++;
        int len = (int)strlen(p);
        while (len > 0 && (p[len - 1] == '\r' || p[len - 1] == '\n' || p[len - 1] == ' ' || p[len - 1] == '\t')) {
            p[len - 1] = '\0';
            len--;
        }
        if (len == 0 || p[0] == '#') continue;

        char full_path[512];
        if (p[0] == '/' || strchr(p, ':')) {
            strncpy(full_path, p, sizeof(full_path) - 1);
        } else {
            snprintf(full_path, sizeof(full_path), "%s/%s", dir, p);
        }
        full_path[sizeof(full_path) - 1] = '\0';

        strncpy(g_m3u.disk_paths[g_m3u.disk_count], full_path, sizeof(g_m3u.disk_paths[0]) - 1);

        const char *base = strrchr(full_path, '/');
        if (!base) base = strrchr(full_path, '\\');
        base = base ? (base + 1) : full_path;
        strncpy(g_m3u.disk_names[g_m3u.disk_count], base, sizeof(g_m3u.disk_names[0]) - 1);

        g_m3u.disk_count++;
    }
    fclose(f);

    if (g_m3u.disk_count > 0) {
        strncpy(g_m3u.playlist_path, filepath, sizeof(g_m3u.playlist_path) - 1);
        g_m3u.current_disk = 0;
        g_m3u.is_active = 1;
        m3u_swap_disk(0);
        return 1;
    }
    return 0;
}

int m3u_swap_disk(int disk_index)
{
    if (!g_m3u.is_active || g_m3u.disk_count <= 0) return 0;
    if (disk_index < 0) disk_index = 0;
    if (disk_index >= g_m3u.disk_count) disk_index = g_m3u.disk_count - 1;

    g_m3u.current_disk = disk_index;
    const char *path = g_m3u.disk_paths[disk_index];

    strncpy(uae4all_image_file0, path, 255);
    uae4all_image_file0[255] = '\0';
    strncpy(changed_df[0], path, 255);
    changed_df[0][255] = '\0';

    disk_insert(0, path);
    real_changed_df[0] = 1;

    char msg[128];
    snprintf(msg, sizeof(msg), "DISK %d/%d: %s", disk_index + 1, g_m3u.disk_count, g_m3u.disk_names[disk_index]);
    ps3_osd_show(msg, 2500);
    return 1;
}

int m3u_next_disk(void)
{
    if (!g_m3u.is_active || g_m3u.disk_count <= 1) return 0;
    int next = (g_m3u.current_disk + 1) % g_m3u.disk_count;
    return m3u_swap_disk(next);
}

int m3u_prev_disk(void)
{
    if (!g_m3u.is_active || g_m3u.disk_count <= 1) return 0;
    int prev = (g_m3u.current_disk - 1 + g_m3u.disk_count) % g_m3u.disk_count;
    return m3u_swap_disk(prev);
}