/*
 * ps3_paths.c - relative path support for the PS3 port.
 *
 * The lv2 file system only accepts absolute paths and PSL1GHT's librt passes
 * the path given to open()/mkdir()/opendir()/... straight to sysLv2Fs*().
 * The emulator code (shared with the Switch / Vita ports) uses relative paths
 * everywhere ("./data", "./roms", "./conf", DATA_PREFIX, SAVE_PREFIX ...), so
 * on PS3 all of them would fail.
 *
 * Instead of touching hundreds of call sites we hook newlib's __syscalls table
 * (filled by PSL1GHT's crt1.c) and transparently resolve every relative path
 * against the application directory:
 *
 *     "./data/font.ttf"  ->  /dev_hdd0/game/<TITLE_ID>/USRDIR/data/font.ttf
 *     "roms"             ->  /dev_hdd0/game/<TITLE_ID>/USRDIR/roms
 *     "."                ->  /dev_hdd0/game/<TITLE_ID>/USRDIR
 *
 * Absolute paths ("/dev_hdd0/...", "/dev_usb000/...") are passed unchanged.
 * This covers fopen(), SDL_RWFromFile(), TTF_OpenFont(), IMG_Load(), stat(),
 * mkdir(), opendir(), rename(), remove() ... since they all end up in newlib.
 */

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <utime.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/reent.h>
#include <sys/dirent.h>
#include <sys/syscalls.h>

#include "ps3_paths.h"

#ifndef PS3_TITLE_ID
#define PS3_TITLE_ID "UAEH00001"
#endif

#define PS3_PATH_MAX 1024

static char s_base_dir[PS3_PATH_MAX] = "/dev_hdd0/game/" PS3_TITLE_ID "/USRDIR";

/* original librt handlers */
static __typeof__(__syscalls.open_r)     orig_open_r;
static __typeof__(__syscalls.stat_r)     orig_stat_r;
static __typeof__(__syscalls.stat64_r)   orig_stat64_r;
static __typeof__(__syscalls.truncate_r) orig_truncate_r;
static __typeof__(__syscalls.chmod_r)    orig_chmod_r;
static __typeof__(__syscalls.rename_r)   orig_rename_r;
static __typeof__(__syscalls.mkdir_r)    orig_mkdir_r;
static __typeof__(__syscalls.opendir_r)  orig_opendir_r;
static __typeof__(__syscalls.rmdir_r)    orig_rmdir_r;
static __typeof__(__syscalls.link_r)     orig_link_r;
static __typeof__(__syscalls.unlink_r)   orig_unlink_r;
static __typeof__(__syscalls.utime_r)    orig_utime_r;

static int s_hooks_installed = 0;

/* Returns 'path' itself when it is already absolute, otherwise writes the
 * resolved absolute path into 'buf' and returns it. */
static const char *resolve(const char *path, char *buf, size_t sz)
{
	size_t blen;

	if (path == NULL || path[0] == '\0' || path[0] == '/')
		return path;

	/* skip any number of leading "./" */
	while (path[0] == '.' && path[1] == '/') {
		path += 2;
		while (path[0] == '/')
			path++;
	}
	if (path[0] == '.' && path[1] == '\0')
		path++;

	blen = strlen(s_base_dir);
	if (path[0] == '\0') {
		snprintf(buf, sz, "%s", s_base_dir);
	} else if (blen > 0 && s_base_dir[blen - 1] == '/') {
		snprintf(buf, sz, "%s%s", s_base_dir, path);
	} else {
		snprintf(buf, sz, "%s/%s", s_base_dir, path);
	}
	return buf;
}

static int hook_open_r(struct _reent *r, const char *file, int flags, int mode)
{
	char buf[PS3_PATH_MAX];
	return orig_open_r(r, resolve(file, buf, sizeof(buf)), flags, mode);
}

static int hook_stat_r(struct _reent *r, const char *path, struct stat *st)
{
	char buf[PS3_PATH_MAX];
	return orig_stat_r(r, resolve(path, buf, sizeof(buf)), st);
}

static int hook_stat64_r(struct _reent *r, const char *path, struct stat *st)
{
	char buf[PS3_PATH_MAX];
	return orig_stat64_r(r, resolve(path, buf, sizeof(buf)), st);
}

static int hook_truncate_r(struct _reent *r, const char *path, off_t len)
{
	char buf[PS3_PATH_MAX];
	return orig_truncate_r(r, resolve(path, buf, sizeof(buf)), len);
}

static int hook_chmod_r(struct _reent *r, const char *path, mode_t mode)
{
	char buf[PS3_PATH_MAX];
	return orig_chmod_r(r, resolve(path, buf, sizeof(buf)), mode);
}

static int hook_rename_r(struct _reent *r, const char *oldp, const char *newp)
{
	char b1[PS3_PATH_MAX], b2[PS3_PATH_MAX];
	return orig_rename_r(r, resolve(oldp, b1, sizeof(b1)), resolve(newp, b2, sizeof(b2)));
}

static int hook_mkdir_r(struct _reent *r, const char *path, mode_t mode)
{
	char buf[PS3_PATH_MAX];
	return orig_mkdir_r(r, resolve(path, buf, sizeof(buf)), mode);
}

static DIR *hook_opendir_r(struct _reent *r, const char *path)
{
	char buf[PS3_PATH_MAX];
	return orig_opendir_r(r, resolve(path, buf, sizeof(buf)));
}

static int hook_rmdir_r(struct _reent *r, const char *path)
{
	char buf[PS3_PATH_MAX];
	return orig_rmdir_r(r, resolve(path, buf, sizeof(buf)));
}

static int hook_link_r(struct _reent *r, const char *oldp, const char *newp)
{
	char b1[PS3_PATH_MAX], b2[PS3_PATH_MAX];
	return orig_link_r(r, resolve(oldp, b1, sizeof(b1)), resolve(newp, b2, sizeof(b2)));
}

static int hook_unlink_r(struct _reent *r, const char *path)
{
	char buf[PS3_PATH_MAX];
	return orig_unlink_r(r, resolve(path, buf, sizeof(buf)));
}

static int hook_utime_r(struct _reent *r, const char *path, const struct utimbuf *times)
{
	char buf[PS3_PATH_MAX];
	return orig_utime_r(r, resolve(path, buf, sizeof(buf)), times);
}

#define PS3_HOOK(name) \
	do { \
		if (__syscalls.name) { \
			orig_##name = __syscalls.name; \
			__syscalls.name = (__typeof__(__syscalls.name))hook_##name; \
		} \
	} while (0)

static void install_hooks(void)
{
	if (s_hooks_installed)
		return;
	s_hooks_installed = 1;

	PS3_HOOK(open_r);
	PS3_HOOK(stat_r);
	PS3_HOOK(stat64_r);
	PS3_HOOK(truncate_r);
	PS3_HOOK(chmod_r);
	PS3_HOOK(rename_r);
	PS3_HOOK(mkdir_r);
	PS3_HOOK(opendir_r);
	PS3_HOOK(rmdir_r);
	PS3_HOOK(link_r);
	PS3_HOOK(unlink_r);
	PS3_HOOK(utime_r);
}

/* crt1.c fills __syscalls in constructor(104), globfile.c runs at 105:
 * install right after them so even static constructors see the hooks. */
static void ps3_paths_ctor(void) __attribute__((constructor(110)));
static void ps3_paths_ctor(void)
{
	install_hooks();
}

void ps3_paths_init(int argc, char **argv)
{
	install_hooks();

	/* argv[0] is the full path of the running EBOOT.BIN / SELF, e.g.
	 * /dev_hdd0/game/UAEH00001/USRDIR/EBOOT.BIN (also under RPCS3). */
	if (argc > 0 && argv && argv[0] && argv[0][0] == '/') {
		char tmp[PS3_PATH_MAX];
		char *slash;

		snprintf(tmp, sizeof(tmp), "%s", argv[0]);
		slash = strrchr(tmp, '/');
		if (slash && slash != tmp) {
			*slash = '\0';
			snprintf(s_base_dir, sizeof(s_base_dir), "%s", tmp);
		}
	}
}

const char *ps3_base_dir(void)
{
	return s_base_dir;
}

const char *ps3_resolve_path(const char *path, char *buf, size_t sz)
{
	return resolve(path, buf, sz);
}

#include "ps3_log.h"

static const char *safe_strcasestr(const char *haystack, const char *needle)
{
	if (!haystack || !needle) return NULL;
	size_t nlen = strlen(needle);
	if (nlen == 0) return haystack;
	while (*haystack) {
		if (strncasecmp(haystack, needle, nlen) == 0)
			return haystack;
		haystack++;
	}
	return NULL;
}

static int copy_file_if_missing(const char *src, const char *dst)
{
	struct stat st;
	if (stat(dst, &st) == 0 && st.st_size > 0)
		return 0; /* already exists */

	FILE *fsrc = fopen(src, "rb");
	if (!fsrc)
		return -1;

	FILE *fdst = fopen(dst, "wb");
	if (!fdst) {
		fclose(fsrc);
		return -1;
	}

	static char s_copy_buf[16384];
	size_t n;
	while ((n = fread(s_copy_buf, 1, sizeof(s_copy_buf), fsrc)) > 0) {
		fwrite(s_copy_buf, 1, n, fdst);
	}
	fclose(fsrc);
	fclose(fdst);
	return 1;
}

void ps3_sync_persistent_kickstarts(void)
{
	ps3_write_log("[PS3] ps3_sync_persistent_kickstarts begin\n");
	/* Ensure all persistent user directories exist on /dev_hdd0 */
	mkdir("/dev_hdd0/uae4all", 0777);
	mkdir("/dev_hdd0/uae4all/kickstarts", 0777);
	mkdir("/dev_hdd0/uae4all/roms", 0777);
	mkdir("/dev_hdd0/uae4all/saves", 0777);
	mkdir("/dev_hdd0/uae4all/conf", 0777);
	mkdir("/dev_hdd0/uae4all/whdload", 0777);
	mkdir("/dev_hdd0/uae4all/whdload/Devs", 0777);
	mkdir("/dev_hdd0/uae4all/whdload/Devs/Kickstarts", 0777);
	ps3_write_log("[PS3] persistent directories ready\n");

	/* Directory bundled in PKG: <s_base_dir>/kickstarts */
	char bundled[PS3_PATH_MAX];
	snprintf(bundled, sizeof(bundled), "%s/kickstarts", s_base_dir);

	DIR *d = opendir(bundled);
	if (!d) {
		d = opendir("./kickstarts");
	}
	if (d) {
		ps3_write_log("[PS3] syncing kickstarts from %s\n", bundled);
		struct dirent *ent;
		int copied_count = 0;
		while ((ent = readdir(d)) != NULL) {
			if (ent->d_name[0] == '.')
				continue;

			char src_path[PS3_PATH_MAX];
			char dst_path[PS3_PATH_MAX];
			snprintf(src_path, sizeof(src_path), "%s/%s", bundled, ent->d_name);
			snprintf(dst_path, sizeof(dst_path), "/dev_hdd0/uae4all/kickstarts/%s", ent->d_name);

			int rc = copy_file_if_missing(src_path, dst_path);
			if (rc > 0) {
				copied_count++;
			}

			/* Also sync WHDLoad Kickstarts and RTB/PAT files to Devs/Kickstarts */
			const char *ext = strrchr(ent->d_name, '.');
			int is_whdload_aux = (ext && (strcasecmp(ext, ".rtb") == 0 || strcasecmp(ext, ".pat") == 0 || strcasecmp(ext, ".key") == 0 || strcasecmp(ext, ".keys") == 0));
			int is_whdload_rom = (strncasecmp(ent->d_name, "kick", 4) == 0 || strncasecmp(ent->d_name, "Kick", 4) == 0);
			if (is_whdload_aux || is_whdload_rom) {
				char whd_dst[PS3_PATH_MAX];
				snprintf(whd_dst, sizeof(whd_dst), "/dev_hdd0/uae4all/whdload/Devs/Kickstarts/%s", ent->d_name);
				copy_file_if_missing(src_path, whd_dst);
			}
		}
		closedir(d);
		ps3_write_log("[PS3] Populated %d new kickstart files into persistent /dev_hdd0/uae4all/kickstarts/\n", copied_count);
	} else {
		ps3_write_log("[PS3] Bundled kickstart directory not accessible: %s\n", bundled);
	}

	/* Provide common name aliases in /dev_hdd0/uae4all/kickstarts/ if base file exists */
	struct stat st13, st34005, st31, st40068;
	if (stat("/dev_hdd0/uae4all/kickstarts/kick13.rom", &st13) != 0) {
		if (stat("/dev_hdd0/uae4all/kickstarts/Kick34005.a500", &st34005) == 0) {
			copy_file_if_missing("/dev_hdd0/uae4all/kickstarts/Kick34005.a500", "/dev_hdd0/uae4all/kickstarts/kick13.rom");
		} else if (stat("/dev_hdd0/uae4all/kickstarts/kick34005.A500", &st34005) == 0) {
			copy_file_if_missing("/dev_hdd0/uae4all/kickstarts/kick34005.A500", "/dev_hdd0/uae4all/kickstarts/kick13.rom");
		}
	}
	if (stat("/dev_hdd0/uae4all/kickstarts/kick34005.A500", &st34005) != 0) {
		if (stat("/dev_hdd0/uae4all/kickstarts/kick13.rom", &st13) == 0) {
			copy_file_if_missing("/dev_hdd0/uae4all/kickstarts/kick13.rom", "/dev_hdd0/uae4all/kickstarts/kick34005.A500");
		} else if (stat("/dev_hdd0/uae4all/kickstarts/Kick34005.a500", &st34005) == 0) {
			copy_file_if_missing("/dev_hdd0/uae4all/kickstarts/Kick34005.a500", "/dev_hdd0/uae4all/kickstarts/kick34005.A500");
		}
	}
	if (stat("/dev_hdd0/uae4all/kickstarts/kick31.rom", &st31) != 0) {
		if (stat("/dev_hdd0/uae4all/kickstarts/kick40068.A1200", &st40068) == 0) {
			copy_file_if_missing("/dev_hdd0/uae4all/kickstarts/kick40068.A1200", "/dev_hdd0/uae4all/kickstarts/kick31.rom");
		}
	}
	if (stat("/dev_hdd0/uae4all/kickstarts/kick40068.A1200", &st40068) != 0 && stat("/dev_hdd0/uae4all/kickstarts/kick31.rom", &st31) == 0) {
		copy_file_if_missing("/dev_hdd0/uae4all/kickstarts/kick31.rom", "/dev_hdd0/uae4all/kickstarts/kick40068.A1200");
	}
	ps3_write_log("[PS3] ps3_sync_persistent_kickstarts finished\n");
}

int ps3_find_kickstart_file(const char *name_or_alias, char *out_path, size_t out_sz)
{
	if (!name_or_alias || name_or_alias[0] == '\0')
		return 0;

	struct stat st;
	if (stat(name_or_alias, &st) == 0 && st.st_size > 0) {
		snprintf(out_path, out_sz, "%s", name_or_alias);
		return 1;
	}

	const char *base = strrchr(name_or_alias, '/');
	base = base ? base + 1 : name_or_alias;
	if (base[0] == '\0')
		return 0;

	char bundled_dir[PS3_PATH_MAX];
	snprintf(bundled_dir, sizeof(bundled_dir), "%s/kickstarts", s_base_dir);

	const char *dirs[] = {
		"/dev_hdd0/uae4all/kickstarts",
		"/dev_hdd0/uae4all/kickstarts/kickstarts",
		bundled_dir,
		"/dev_usb000/uae4all/kickstarts",
		"/dev_usb000/kickstarts",
		"/dev_usb000/kickstarts/kickstarts",
		"/dev_usb000/roms/kickstarts",
		"/dev_usb001/uae4all/kickstarts",
		"/dev_usb001/kickstarts",
		"/dev_usb001/kickstarts/kickstarts",
		"/dev_usb001/roms/kickstarts",
		NULL
	};

	struct stat usb_st;
	int usb0_ok = (stat("/dev_usb000", &usb_st) == 0);
	int usb1_ok = (stat("/dev_usb001", &usb_st) == 0);

	/* 1. Direct check */
	for (int d = 0; dirs[d]; d++) {
		if (strncmp(dirs[d], "/dev_usb000", 11) == 0 && !usb0_ok) continue;
		if (strncmp(dirs[d], "/dev_usb001", 11) == 0 && !usb1_ok) continue;
		char cand[PS3_PATH_MAX];
		snprintf(cand, sizeof(cand), "%s/%s", dirs[d], base);
		if (stat(cand, &st) == 0 && S_ISREG(st.st_mode) && st.st_size >= 200000 && st.st_size <= 1100000) {
			snprintf(out_path, out_sz, "%s", cand);
			return 1;
		}
	}

	/* 2. Case-insensitive check */
	for (int d = 0; dirs[d]; d++) {
		if (strncmp(dirs[d], "/dev_usb000", 11) == 0 && !usb0_ok) continue;
		if (strncmp(dirs[d], "/dev_usb001", 11) == 0 && !usb1_ok) continue;
		DIR *dirp = opendir(dirs[d]);
		if (!dirp) continue;
		struct dirent *ent;
		while ((ent = readdir(dirp)) != NULL) {
			if (ent->d_name[0] == '.') continue;
			if (strcasecmp(ent->d_name, base) == 0) {
				char cand[PS3_PATH_MAX];
				snprintf(cand, sizeof(cand), "%s/%s", dirs[d], ent->d_name);
				if (stat(cand, &st) == 0 && S_ISREG(st.st_mode) && st.st_size >= 200000 && st.st_size <= 1100000) {
					snprintf(out_path, out_sz, "%s", cand);
					closedir(dirp);
					return 1;
				}
			}
		}
		closedir(dirp);
	}

	/* 3. Alias / tag substring search */
	const char *search_tag = NULL;
	if (safe_strcasestr(base, "1.3") || safe_strcasestr(base, "13") || safe_strcasestr(base, "34005"))
		search_tag = "13";
	else if (safe_strcasestr(base, "3.1") || safe_strcasestr(base, "31") || safe_strcasestr(base, "40068"))
		search_tag = "31";
	else if (safe_strcasestr(base, "2.0") || safe_strcasestr(base, "20") || safe_strcasestr(base, "37175"))
		search_tag = "20";
	else if (safe_strcasestr(base, "CD32") || safe_strcasestr(base, "cd32") || safe_strcasestr(base, "40060"))
		search_tag = "cd32";

	if (search_tag) {
		for (int d = 0; dirs[d]; d++) {
			if (strncmp(dirs[d], "/dev_usb000", 11) == 0 && !usb0_ok) continue;
			if (strncmp(dirs[d], "/dev_usb001", 11) == 0 && !usb1_ok) continue;
			DIR *dirp = opendir(dirs[d]);
			if (!dirp) continue;
			struct dirent *ent;
			while ((ent = readdir(dirp)) != NULL) {
				if (ent->d_name[0] == '.') continue;
				const char *ext = strrchr(ent->d_name, '.');
				if (ext && (strcasecmp(ext, ".rtb") == 0 || strcasecmp(ext, ".pat") == 0 || strcasecmp(ext, ".adf") == 0 ||
				            strcasecmp(ext, ".lha") == 0 || strcasecmp(ext, ".lzh") == 0 || strcasecmp(ext, ".zip") == 0 ||
				            strcasecmp(ext, ".iso") == 0 || strcasecmp(ext, ".cue") == 0 || strcasecmp(ext, ".bin") == 0 ||
				            strcasecmp(ext, ".chd") == 0 || strcasecmp(ext, ".hdf") == 0))
					continue;

				int match = 0;
				if (strcmp(search_tag, "13") == 0) {
					if (safe_strcasestr(ent->d_name, "13") || safe_strcasestr(ent->d_name, "1.3") || safe_strcasestr(ent->d_name, "34005"))
						match = 1;
				} else if (strcmp(search_tag, "31") == 0) {
					if (safe_strcasestr(ent->d_name, "31") || safe_strcasestr(ent->d_name, "3.1") || safe_strcasestr(ent->d_name, "40068"))
						match = 1;
				} else if (strcmp(search_tag, "20") == 0) {
					if (safe_strcasestr(ent->d_name, "20") || safe_strcasestr(ent->d_name, "2.0") || safe_strcasestr(ent->d_name, "37175"))
						match = 1;
				} else if (strcmp(search_tag, "cd32") == 0) {
					if (safe_strcasestr(ent->d_name, "40060") || safe_strcasestr(ent->d_name, "cd32"))
						match = 1;
				}

				if (match) {
					char cand[PS3_PATH_MAX];
					snprintf(cand, sizeof(cand), "%s/%s", dirs[d], ent->d_name);
					if (stat(cand, &st) == 0 && S_ISREG(st.st_mode) && st.st_size >= 200000 && st.st_size <= 1100000) {
						snprintf(out_path, out_sz, "%s", cand);
						closedir(dirp);
						return 1;
					}
				}
			}
			closedir(dirp);
		}
	}

	return 0;
}
