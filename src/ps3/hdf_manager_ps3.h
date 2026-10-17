#ifndef HDF_MANAGER_H
#define HDF_MANAGER_H

#include <stddef.h>

#define HDF_DOSTYPE_OFS 0x444F5300
#define HDF_DOSTYPE_FFS 0x444F5301

typedef struct {
    char path[512];
    unsigned long size;
    unsigned long total_blocks;
    int blocksize;
    int sectors_per_track;
    int reserved;
    int surfaces;
    int cylinders;
    int is_readonly;
    int is_rdb;
    int valid;
    unsigned int dostype;
    /* RDB (partitioned) images: how many filesystems the image holds, how
     * many of them carry a DOS boot block, and where the first one starts. */
    int partition_count;
    int bootable_partitions;
    unsigned int start_block;
    char partition_names[192];
    char filesystem[32];
    char error[256];
} HdfInfo;

#ifdef __cplusplus
extern "C" {
#endif

int hdf_analyze(const char *path, HdfInfo *info);
int hdf_backup(const char *path, const char *dest_dir, char *err, size_t errsz);
int hdf_create_blank(const char *path, unsigned long megabytes, char *err, size_t errsz);

#ifdef __cplusplus
}
#endif

#endif
