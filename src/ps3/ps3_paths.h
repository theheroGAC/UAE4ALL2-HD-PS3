#ifndef PS3_PATHS_H
#define PS3_PATHS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Sets the application base directory from argv[0] (directory of EBOOT.BIN).
 * Relative paths used anywhere in the program are resolved against it. */
void ps3_paths_init(int argc, char **argv);

/* Absolute application directory, e.g. /dev_hdd0/game/UAEH00001/USRDIR */
const char *ps3_base_dir(void);

/* Resolve a (possibly relative) path; returns 'path' if already absolute. */
const char *ps3_resolve_path(const char *path, char *buf, size_t sz);

/* Auto-copy bundled kickstarts from PKG to persistent /dev_hdd0/uae4all/kickstarts/ */
void ps3_sync_persistent_kickstarts(void);

/* Search for kickstart ROM file across persistent and staging dirs with case-insensitivity */
int ps3_find_kickstart_file(const char *name_or_alias, char *out_path, size_t out_sz);

#ifdef __cplusplus
}
#endif

#endif
