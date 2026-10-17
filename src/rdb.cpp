/*
 * rdb.cpp - Amiga RigidDiskBlock (RDB) parsing.
 *
 * See include/rdb.h. All the on-disk values are big endian regardless of the
 * host, so every field is read through rdb_be32().
 *
 * Layouts (devices/hardblocks.h):
 *
 *   RigidDiskBlock                     PartitionBlock
 *   0x00 rdb_ID          'RDSK'        0x00 pb_ID           'PART'
 *   0x04 rdb_SummedLongs              0x04 pb_SummedLongs
 *   0x08 rdb_HostID                   0x08 pb_HostID
 *   0x0c rdb_BlockBytes               0x0c pb_Next
 *   0x10 rdb_Flags                    0x10 pb_Flags
 *   0x14 rdb_BadBlockList             0x14 pb_Reserved1[2]
 *   0x18 rdb_PartitionList            0x1c pb_DevFlags
 *   0x1c rdb_FileSysHeaderList        0x20 pb_DriveName (BCPL, 32 bytes)
 *   0x20 rdb_DriveInit                0x40 pb_Reserved2[7]
 *   0x24 rdb_Reserved1[6]             0x5c pb_Environment[20]
 *   0x3c rdb_Cylinders                0xac pb_EReserved[12]
 *   0x40 rdb_Sectors
 *   0x44 rdb_Heads
 *   0x48 rdb_Interleave
 *   0x4c rdb_Park
 *   0x50 rdb_Reserved2[3]
 *   0x5c rdb_WritePreComp
 *   0x60 rdb_ReducedWrite
 *   0x64 rdb_WriteRetry
 *   0x68 rdb_ReadRetry
 *   0x6c rdb_LowRDB      (cylinder)
 *   0x70 rdb_HighRDB     (cylinder)
 *
 * The execution environment vector starts at pb_Environment:
 *   0 de_TableSize        8 de_LowCyl         15 de_DosType
 *   1 de_SizeBlock        9 de_HighCyl        16 de_Baud
 *   2 de_SecOrg          10 de_NumBuffers     17 de_Control
 *   3 de_Surfaces        11 de_BufMemType     18 de_BootBlocks
 *   4 de_SectorsPerTrack 12 de_MaxTransfer
 *   5 de_Reserved        13 de_Mask
 *   6 de_PreAlloc        14 de_BootPri
 *   7 de_Interleave
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "rdb.h"

#define RDB_ID   0x5244534bUL /* 'RDSK' */
#define PART_ID  0x50415254UL /* 'PART' */
#define RDB_NO_PARTITION 0xffffffffUL

static unsigned int rdb_be32(const unsigned char *p)
{
	return ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) |
	       ((unsigned int)p[2] << 8) | (unsigned int)p[3];
}

static int rdb_read_at(FILE *f, unsigned long offset, unsigned char *buf, size_t len)
{
	if (fseek(f, (long)offset, SEEK_SET) != 0)
		return 0;
	return fread(buf, 1, len, f) == len;
}

/* pb_DriveName and rdb_DriveName are BCPL strings: a length byte followed by
 * that many characters. Copy at most dstsz-1 characters and terminate. */
static void rdb_copy_bcpl(const unsigned char *src, size_t src_size, char *dst, size_t dstsz)
{
	size_t len;
	size_t i;

	if (!dst || dstsz == 0)
		return;
	dst[0] = '\0';
	if (!src || src_size == 0)
		return;

	len = src[0];
	if (len > src_size - 1)
		len = src_size - 1;
	if (len > dstsz - 1)
		len = dstsz - 1;

	for (i = 0; i < len; i++) {
		unsigned char c = src[1 + i];
		/* Keep the name printable so it can be shown in the GUI. */
		if (c < 0x20 || c == 0x7f)
			c = '_';
		dst[i] = (char)c;
	}
	dst[len] = '\0';

	/* Trim trailing spaces. */
	while (len > 0 && dst[len - 1] == ' ') {
		dst[len - 1] = '\0';
		len--;
	}
}

static int rdb_find_rdsk_offset(FILE *f, unsigned long *out_offset)
{
	unsigned char hdr[4];
	int i;

	if (!f)
		return 0;
	for (i = 0; i < 16; i++) {
		unsigned long off = (unsigned long)i * 512UL;
		if (!rdb_read_at(f, off, hdr, sizeof(hdr)))
			break;
		if (rdb_be32(hdr) == RDB_ID) {
			if (out_offset)
				*out_offset = off;
			return 1;
		}
	}
	return 0;
}

int rdb_is_rigid_disk_block(FILE *f)
{
	return rdb_find_rdsk_offset(f, NULL);
}

static int rdb_blocksize_ok(unsigned int blocksize)
{
	if (blocksize < 512 || blocksize > RDB_MAX_BLOCKSIZE)
		return 0;
	if ((blocksize & (blocksize - 1)) != 0)
		return 0;
	return 1;
}

int rdb_parse_file(FILE *f, RdbPartition *out, int max_parts)
{
	unsigned char *block;
	unsigned char boot[4];
	unsigned long rdsk_offset = 0;
	unsigned int blocksize;
	unsigned int cylinders, sectors, heads;
	unsigned int part_ptr;
	int count = 0;
	int std_rdsk = 1;

	if (!f || !out || max_parts <= 0)
		return 0;

	if (!rdb_find_rdsk_offset(f, &rdsk_offset))
		return 0;

	block = (unsigned char *)malloc(RDB_MAX_BLOCKSIZE);
	if (!block)
		return -1;
	memset(out, 0, sizeof(RdbPartition) * (size_t)max_parts);

	if (!rdb_read_at(f, rdsk_offset, block, 512)) {
		free(block);
		return -1;
	}

	/* In AmigaOS <devices/hardblocks.h>, rdb_ChkSum is at 0x08, rdb_HostID is
	 * at 0x0c, rdb_BlockBytes is at 0x10, and rdb_PartitionList is at 0x1c.
	 * Fall back to the legacy ChkSum-less offsets only if 0x10 is not a valid
	 * power-of-two block size and 0x0c is. */
	if (rdb_blocksize_ok(rdb_be32(block + 0x10))) {
		std_rdsk = 1;
		blocksize = rdb_be32(block + 0x10);
		part_ptr = rdb_be32(block + 0x1c);
		cylinders = rdb_be32(block + 0x40);
		sectors = rdb_be32(block + 0x44);
		heads = rdb_be32(block + 0x48);
	} else if (rdb_blocksize_ok(rdb_be32(block + 0x0c))) {
		std_rdsk = 0;
		blocksize = rdb_be32(block + 0x0c);
		part_ptr = rdb_be32(block + 0x18);
		cylinders = rdb_be32(block + 0x3c);
		sectors = rdb_be32(block + 0x40);
		heads = rdb_be32(block + 0x44);
	} else {
		std_rdsk = 1;
		blocksize = 512;
		part_ptr = rdb_be32(block + 0x1c);
		cylinders = rdb_be32(block + 0x40);
		sectors = rdb_be32(block + 0x44);
		heads = rdb_be32(block + 0x48);
	}
	(void)cylinders;

	while (part_ptr != RDB_NO_PARTITION && part_ptr != 0 && count < max_parts) {
		RdbPartition *p = &out[count];
		const unsigned char *env;
		const unsigned char *drv_name;
		unsigned int next_part;
		unsigned int env_size;
		unsigned int sizeblock_lw, p_sectors, p_surfaces, p_reserved;
		unsigned int low_cyl, high_cyl;
		unsigned int dostype;
		unsigned int flags;
		unsigned int start_block, blocks;
		unsigned int p_blocksize;
		int std_part;

		if (part_ptr >= 0x80000000UL)
			break;
		if (!rdb_read_at(f, (unsigned long)part_ptr * blocksize, block, blocksize))
			break;
		if (rdb_be32(block) != PART_ID)
			break;

		/* In AmigaOS <devices/hardblocks.h>, PartitionBlock has pb_ChkSum at
		 * 0x08, pb_HostID at 0x0c, pb_Next at 0x10, pb_Flags at 0x14,
		 * pb_DriveName at 0x24, pb_Reserved2[15] at 0x44, and pb_Environment
		 * at 0x80. Auto-detect against the legacy 0x5c layout. */
		if (blocksize >= 0x80 + 16 * 4 &&
		    rdb_be32(block + 0x80) >= 9 && rdb_be32(block + 0x80) <= 64 &&
		    (std_rdsk || rdb_be32(block + 0x5c) < 9 || rdb_be32(block + 0x5c) > 64)) {
			std_part = 1;
		} else if (blocksize >= 0x5c + 16 * 4 &&
		           rdb_be32(block + 0x5c) >= 9 && rdb_be32(block + 0x5c) <= 64) {
			std_part = 0;
		} else {
			break;
		}

		next_part = rdb_be32(block + (std_part ? 0x10 : 0x0c));
		flags = rdb_be32(block + (std_part ? 0x14 : 0x10));
		drv_name = block + (std_part ? 0x24 : 0x20);
		env = block + (std_part ? 0x80 : 0x5c);

		env_size = rdb_be32(env + 0);
		if (env_size < 9)
			break;

		sizeblock_lw = rdb_be32(env + 1 * 4);
		p_surfaces = rdb_be32(env + 3 * 4);
		p_sectors = rdb_be32(env + 4 * 4);
		p_reserved = rdb_be32(env + 5 * 4);
		low_cyl = rdb_be32(env + 8 * 4);
		high_cyl = rdb_be32(env + 9 * 4);
		p->boot_priority = (env_size >= 14) ? (int)rdb_be32(env + 14 * 4) : 0;
		dostype = (env_size >= 15) ? rdb_be32(env + 15 * 4) : 0x444f5300UL;
		p->flags = flags;

		if (sizeblock_lw >= 128 && rdb_blocksize_ok(sizeblock_lw * 4))
			p_blocksize = sizeblock_lw * 4;
		else
			p_blocksize = blocksize;

		if (p_sectors == 0 || p_sectors > 0xffff)
			p_sectors = (sectors > 0 && sectors <= 0xffff) ? sectors : 32;
		if (p_surfaces == 0 || p_surfaces > 0xffff)
			p_surfaces = (heads > 0 && heads <= 0xffff) ? heads : 1;
		if (high_cyl < low_cyl) {
			/* Malformed partition: skip it but keep walking the list. */
			part_ptr = next_part;
			continue;
		}

		/* In AmigaOS DosEnvec, de_LowCyl and de_HighCyl are absolute cylinder
		 * numbers from cylinder 0 of the drive (they already include the RDB
		 * cylinders reserved at the beginning of the disk). */
		start_block = low_cyl * p_surfaces * p_sectors;
		blocks = (high_cyl - low_cyl + 1) * p_surfaces * p_sectors;

		/* Partitions without a DOS filesystem (swap, unformatted) are not
		 * mountable; skip them but keep walking. */
		if (dostype == 0 || blocks == 0) {
			part_ptr = next_part;
			continue;
		}

		rdb_copy_bcpl(drv_name, 32, p->name, sizeof(p->name));
		p->dostype = dostype;
		p->start_block = start_block;
		p->blocks = blocks;
		p->sectors = (int)p_sectors;
		p->surfaces = (int)p_surfaces;
		p->reserved = (int)p_reserved;
		p->blocksize = (int)p_blocksize;
		p->low_cyl = (int)low_cyl;
		p->high_cyl = (int)high_cyl;

		/* A partition boots when its own first block carries a DOS boot
		 * block (the same test the hardfile mount uses). */
		p->bootable = 0;
		if (dostype >= 0x444f5300UL && dostype <= 0x444f5307UL) {
			if (rdb_read_at(f, (unsigned long)start_block * p_blocksize, boot, sizeof(boot))) {
				if (boot[0] == 'D' && boot[1] == 'O' && boot[2] == 'S' && boot[3] <= 7)
					p->bootable = 1;
			}
			/* Some RDB tools express de_SectorsPerTrack in physical rdb_BlockBytes
			 * sectors even when de_SizeBlock is larger than 512 bytes. */
			if (!p->bootable && p_blocksize != blocksize &&
			    rdb_read_at(f, (unsigned long)start_block * blocksize, boot, sizeof(boot)) &&
			    boot[0] == 'D' && boot[1] == 'O' && boot[2] == 'S' && boot[3] <= 7) {
				unsigned int scale = p_blocksize / blocksize;
				if (scale > 1 && (p_sectors % scale) == 0) {
					p_sectors /= scale;
					start_block /= scale;
					blocks /= scale;
					p->sectors = (int)p_sectors;
					p->start_block = start_block;
					p->blocks = blocks;
				} else {
					p_blocksize = blocksize;
					p->blocksize = (int)p_blocksize;
				}
				p->bootable = 1;
			}
		}

		count++;
		part_ptr = next_part;
	}

	free(block);
	return count;
}

int rdb_parse(const char *path, RdbPartition *out, int max_parts)
{
	FILE *f;
	int res;

	if (!path || !out || max_parts <= 0)
		return 0;

	f = fopen(path, "rb");
	if (!f)
		return 0;

	res = rdb_parse_file(f, out, max_parts);
	fclose(f);
	return res;
}
