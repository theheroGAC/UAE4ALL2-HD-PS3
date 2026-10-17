#ifndef UAE_WHDLOAD_MANAGER_PS3_H
#define UAE_WHDLOAD_MANAGER_PS3_H

#ifdef __PS3__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PS3_WHDLOAD_ROOT "/dev_hdd0/uae4all/whdload"

int ps3_whdload_install_lha(const char *archive_path, char *installed_path, size_t installed_path_size);
int ps3_whdload_list(char names[][128], int max_names);
int ps3_whdload_prepare_launch(const char *game_name);
int ps3_whdload_can_launch(const char *game_name);
const char *ps3_whdload_root(void);
const char *ps3_whdload_get_last_error(void);
int ps3_whdload_is_aga(const char *game_name);
int ps3_whdload_needs_immediate_blitter(const char *game_name);
int ps3_whdload_needs_improved_blitter(const char *game_name);

#ifdef __cplusplus
}
#endif

#endif
#endif