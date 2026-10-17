#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "ps3_log.h"
#include "ps3_paths.h"

#ifndef PS3_TITLE_ID
#define PS3_TITLE_ID "UAEH00001"
#endif

#define PS3_LOG_PATH_MAX 1024

static FILE *s_log_file;

static volatile int s_log_busy;

static FILE *ps3_log_try_open(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0 && st.st_size > 4 * 1024 * 1024) {
        char old_path[PS3_LOG_PATH_MAX];
        snprintf(old_path, sizeof(old_path), "%s.old", path);
        remove(old_path);
        rename(path, old_path);
    }
    FILE *f = fopen(path, "a");
    if (f) {
        fprintf(f, "[PS3] ===== session start =====\n");
        fflush(f);
    }
    return f;
}

static FILE *ps3_log_open(void)
{
    char path[PS3_LOG_PATH_MAX];
    const char *base = ps3_base_dir();
    FILE *f;

    if (base && base[0]) {
        snprintf(path, sizeof(path), "%s/ps3log.txt", base);
        f = ps3_log_try_open(path);
        if (f)
            return f;
    }
    snprintf(path, sizeof(path), "/dev_hdd0/game/%s/USRDIR/ps3log.txt", PS3_TITLE_ID);
    f = ps3_log_try_open(path);
    if (f)
        return f;
    f = ps3_log_try_open("/dev_hdd0/tmp/ps3log.txt");
    if (f)
        return f;
    f = ps3_log_try_open("/dev_hdd0/ps3log.txt");
    if (f)
        return f;
    return ps3_log_try_open("./ps3log.txt");
}

void ps3_write_log(const char *fmt, ...)
{
    va_list ap;

    if (!fmt)
        return;
    if (__sync_lock_test_and_set(&s_log_busy, 1))
        return;
    if (!s_log_file) {
        s_log_file = ps3_log_open();
        if (!s_log_file) {
            __sync_lock_release(&s_log_busy);
            return;
        }
    }
    va_start(ap, fmt);
    vfprintf(s_log_file, fmt, ap);
    va_end(ap);
    fflush(s_log_file);
    __sync_lock_release(&s_log_busy);
}

void ps3_log_close(void)
{
    if (s_log_file) {
        fclose(s_log_file);
        s_log_file = NULL;
    }
}
