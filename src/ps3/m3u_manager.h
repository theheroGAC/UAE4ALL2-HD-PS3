#ifndef M3U_MANAGER_H
#define M3U_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_M3U_DISKS 16

struct m3u_playlist {
    char playlist_path[512];
    char disk_paths[MAX_M3U_DISKS][512];
    char disk_names[MAX_M3U_DISKS][128];
    int disk_count;
    int current_disk;
    int is_active;
};

extern struct m3u_playlist g_m3u;

int m3u_load(const char *filepath);
void m3u_clear(void);
int m3u_swap_disk(int disk_index);
int m3u_next_disk(void);
int m3u_prev_disk(void);

#ifdef __cplusplus
}
#endif

#endif