/*
 * rdb.h - Amiga RigidDiskBlock (RDB) parsing.
 *
 * Real world HDF images (WinUAE, HstImager, a real Amiga, ...) are not
 * "a filesystem at block 0": block 0 holds a RigidDiskBlock which points to a
 * chain of PartitionBlocks, and each partition holds its own DOS/FFS
 * filesystem starting at the block named by the partition.
 *
 * The emulator exposes one mountable unit per filesystem, so an RDB image has
 * to be split into one unit per partition, each with its own start offset,
 * length, geometry and dostype. This module only parses the on-disk structures
 * (host independent, no emulator headers), so it can also be unit tested on the
 * development host.
 */
#ifndef UAE_RDB_H
#define UAE_RDB_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RDB_MAX_PARTITIONS 16
#define RDB_MAX_BLOCKSIZE 4096

typedef struct {
	char name[36];          /* Amiga volume name (BCPL drive name, cleaned) */
	unsigned int dostype;   /* de_DosType, e.g. 0x444f5301 = "DOS\1" (FFS) */
	unsigned int start_block; /* partition start, in blocks from the file start */
	unsigned int blocks;    /* partition length in blocks */
	int sectors;            /* sectors per track */
	int surfaces;
	int reserved;
	int blocksize;          /* bytes per block, normally 512 */
	int low_cyl;
	int high_cyl;
	unsigned int boot_priority;
	unsigned int flags;     /* pb_Flags */
	int bootable;           /* a DOS<=7 boot block was found at the partition start */
} RdbPartition;

/* Parse the RDB of an already open image (positioned anywhere; the function
 * seeks itself). Returns the number of usable partitions found, 0 when the
 * image does not contain an RDB, or -1 when an RDB is present but unusable. */
int rdb_parse_file(FILE *f, RdbPartition *out, int max_parts);

/* Same as rdb_parse_file() for a path. */
int rdb_parse(const char *path, RdbPartition *out, int max_parts);

/* True when the 512 bytes at the start of 'f' are a RigidDiskBlock. */
int rdb_is_rigid_disk_block(FILE *f);

#ifdef __cplusplus
}
#endif

#endif /* UAE_RDB_H */
