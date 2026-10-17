#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <archive.h>
#include <archive_entry.h>

static char s_dummy_archive[256];
static char s_dummy_entry[128];

struct archive *archive_read_new(void) {
	memset(s_dummy_archive, 0, sizeof(s_dummy_archive));
	return (struct archive *)s_dummy_archive;
}

int archive_read_support_filter_all(struct archive *a) { (void)a; return ARCHIVE_OK; }
int archive_read_support_format_all(struct archive *a) { (void)a; return ARCHIVE_OK; }
int archive_read_support_format_zip(struct archive *a) { (void)a; return ARCHIVE_OK; }
int archive_read_support_format_7zip(struct archive *a) { (void)a; return ARCHIVE_OK; }
int archive_read_support_format_lha(struct archive *a) { (void)a; return ARCHIVE_OK; }
int archive_read_support_format_rar(struct archive *a) { (void)a; return ARCHIVE_OK; }
int archive_read_support_format_raw(struct archive *a) { (void)a; return ARCHIVE_OK; }

int archive_read_set_open_callback(struct archive *a, archive_open_callback *f) { (void)a; (void)f; return ARCHIVE_OK; }
int archive_read_set_read_callback(struct archive *a, archive_read_callback *f) { (void)a; (void)f; return ARCHIVE_OK; }
int archive_read_set_close_callback(struct archive *a, archive_close_callback *f) { (void)a; (void)f; return ARCHIVE_OK; }
int archive_read_set_callback_data(struct archive *a, void *d) { (void)a; (void)d; return ARCHIVE_OK; }

int archive_read_open1(struct archive *a) { (void)a; return ARCHIVE_FAILED; }
int archive_read_next_header(struct archive *a, struct archive_entry **e) { (void)a; (void)e; return ARCHIVE_FAILED; }
la_ssize_t archive_read_data(struct archive *a, void *b, size_t s) { (void)a; (void)b; (void)s; return -1; }
int archive_read_data_skip(struct archive *a) { (void)a; return -1; }
int archive_read_free(struct archive *a) { (void)a; return ARCHIVE_OK; }
int archive_read_close(struct archive *a) { (void)a; return ARCHIVE_OK; }
const char *archive_error_string(struct archive *a) { (void)a; return "archive support not built"; }
const char *archive_path(struct archive *a) { (void)a; return ""; }

struct archive_entry *archive_entry_new(void) {
	memset(s_dummy_entry, 0, sizeof(s_dummy_entry));
	return (struct archive_entry *)s_dummy_entry;
}
void archive_entry_free(struct archive_entry *e) { (void)e; }
const char *archive_entry_pathname(struct archive_entry *e) { (void)e; return NULL; }
const char *archive_entry_pathname_utf8(struct archive_entry *e) { (void)e; return NULL; }
mode_t archive_entry_filetype(struct archive_entry *e) { (void)e; return AE_IFREG; }
void archive_entry_set_pathname(struct archive_entry *e, const char *p) { (void)e; (void)p; }
void archive_entry_set_filetype(struct archive_entry *e, unsigned int t) { (void)e; (void)t; }
void archive_entry_set_perm(struct archive_entry *e, unsigned int p) { (void)e; (void)p; }
