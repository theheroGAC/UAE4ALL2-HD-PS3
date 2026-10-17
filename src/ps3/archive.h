#ifndef PS3_ARCHIVE_H
#define PS3_ARCHIVE_H

#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef __cplusplus
extern "C" {
#endif

struct archive;
struct archive_entry;

typedef int64_t la_int64_t;
typedef ssize_t la_ssize_t;

#define ARCHIVE_OK 0
#define ARCHIVE_EOF 1
#define ARCHIVE_RETRY (-10)
#define ARCHIVE_WARN (-20)
#define ARCHIVE_FAILED (-25)
#define ARCHIVE_FATAL (-30)

#define AE_IFREG 0100000
#define AE_IFDIR 0040000

typedef la_ssize_t archive_open_callback(struct archive *, void *);
typedef la_ssize_t archive_read_callback(struct archive *, void *, const void **);
typedef int archive_skip_callback(struct archive *, void *, la_int64_t);
typedef int archive_close_callback(struct archive *, void *);

struct archive *archive_read_new(void);
int archive_read_support_filter_all(struct archive *);
int archive_read_support_format_all(struct archive *);
int archive_read_support_format_zip(struct archive *);
int archive_read_support_format_7zip(struct archive *);
int archive_read_support_format_lha(struct archive *);
int archive_read_support_format_rar(struct archive *);
int archive_read_support_format_raw(struct archive *);
int archive_read_set_open_callback(struct archive *, archive_open_callback *);
int archive_read_set_read_callback(struct archive *, archive_read_callback *);
int archive_read_set_close_callback(struct archive *, archive_close_callback *);
int archive_read_set_callback_data(struct archive *, void *);
int archive_read_open1(struct archive *);
int archive_read_next_header(struct archive *, struct archive_entry **);
la_ssize_t archive_read_data(struct archive *, void *, size_t);
int archive_read_data_skip(struct archive *);
int archive_read_free(struct archive *);
int archive_read_close(struct archive *);
const char *archive_error_string(struct archive *);
const char *archive_path(struct archive *);

#ifdef __cplusplus
}
#endif

#endif
