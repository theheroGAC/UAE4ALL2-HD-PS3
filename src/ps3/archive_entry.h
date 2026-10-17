#ifndef PS3_ARCHIVE_ENTRY_H
#define PS3_ARCHIVE_ENTRY_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct archive;
struct archive_entry;

struct archive_entry *archive_entry_new(void);
void archive_entry_free(struct archive_entry *entry);
const char *archive_entry_pathname(struct archive_entry *entry);
const char *archive_entry_pathname_utf8(struct archive_entry *entry);
mode_t archive_entry_filetype(struct archive_entry *entry);
void archive_entry_set_pathname(struct archive_entry *entry, const char *path);
void archive_entry_set_filetype(struct archive_entry *entry, unsigned int type);
void archive_entry_set_perm(struct archive_entry *entry, unsigned int perm);

#ifdef __cplusplus
}
#endif

#endif
