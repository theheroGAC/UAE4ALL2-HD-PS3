 /*
  * UAE - The Un*x Amiga Emulator
  *
  * Config file handling
  * This still needs some thought before it's complete...
  *
  * Copyright 1998 Brian King, Bernd Schmidt
  */

#include "sysconfig.h"
#include "sysdeps.h"

#include <ctype.h>

#include "config.h"
#include "options.h"
#include "thread.h"
#include "uae.h"
#include "autoconf.h"
#include "gui.h"
#include "rdb.h"


char * make_hard_dir_cfg_line (char *dst) {
	char buffer[256];
	int i;

	if (uae4all_hard_dir[0] != '\0') {
		for (i = strlen(uae4all_hard_dir); i > 0; i--)
			if ((uae4all_hard_dir[i] == '/')||(uae4all_hard_dir[i] == '\\'))
				break;
		if (i > 0) {
			strncpy(buffer, &uae4all_hard_dir[i+1], 256);
			strcat(buffer, ":");
			strncat(buffer, uae4all_hard_dir, 256 - strlen(buffer));
			strcpy(dst, buffer);
		} else
			return NULL;
	}

	return dst;
}


char * make_hard_file_cfg_line (char *dst) {
    char filepath[256];
    char buffer[256];

    if (!dst || dst[0] == '\0')
        return dst;

    const char *p = dst;
    int colons = 0;
    for (int i = 0; p[i] != '\0'; i++) {
        if (p[i] == ':') {
            colons++;
            if (colons == 4) {
                p = &p[i + 1];
                break;
            }
        }
    }
    strncpy(filepath, p, sizeof(filepath) - 1);
    filepath[sizeof(filepath) - 1] = '\0';

    int surfaces = 1;
    int sectors = 32;
    int reserved = 2;
    int blocksize = 512;

    FILE *myFile = fopen(filepath, "rb");
    if (myFile == NULL) {
        myFile = fopen(dst, "rb");
        if (myFile == NULL)
            return dst;
        strncpy(filepath, dst, sizeof(filepath) - 1);
        filepath[sizeof(filepath) - 1] = '\0';
    }

    fseek(myFile, 0, SEEK_END);
    unsigned long mySize = ftell(myFile);

    /* The root block is commonly located near the middle of an Amiga
     * partition; that does not change the reserved boot blocks at its start. */
    fclose(myFile);

    if (mySize >= 1073741824UL && mySize < 2147483648UL)
        surfaces = 2;
    else if (mySize >= 2147483648UL)
        surfaces = 4;

    snprintf(buffer, sizeof(buffer), "%d:%d:%d:%d:%s", sectors, surfaces, reserved, blocksize, filepath);
    strncpy(dst, buffer, 255);
    dst[255] = '\0';
    return dst;
}

 void parse_filesys_spec (int readonly, char *spec)
{
    if (!spec || !spec[0]) return;
    char volname[64] = "DH0";
    char path[256];
    strncpy(path, spec, sizeof(path) - 1);
    path[sizeof(path) - 1] = '\0';

    char *colon = strchr(path, ':');
    if (colon) {
        if ((colon == path + 3 && (strncasecmp(path, "ux0", 3) == 0 || strncasecmp(path, "uma", 3) == 0 || strncasecmp(path, "app", 3) == 0)) ||
            colon[1] == '/' || colon[1] == '\\') {
            strcpy(volname, "DH0");
        } else {
            size_t vlen = (size_t)(colon - path);
            if (vlen >= sizeof(volname)) vlen = sizeof(volname) - 1;
            strncpy(volname, path, vlen);
            volname[vlen] = '\0';
            memmove(path, colon + 1, strlen(colon + 1) + 1);
        }
    }

    char *mount_err = add_filesys_unit (currprefs.mountinfo, volname, path, readonly, 0, 0, 0, 0);
    if (mount_err) {
	write_log ("[HD] filesys mount FAILED: vol=%s path='%s': %s\n", volname, path, mount_err);
    } else {
	write_log ("[HD] filesys mount ok: vol=%s path='%s'\n", volname, path);
    }
}

 int parse_hardfile_spec (int readonly, char *spec)
{
    char *x0 = my_strdup (spec);
    char *x1, *x2, *x3, *x4;
    int units = 0;

    if (x0 == NULL)
	return 0;

    x1 = strchr (x0, ':');
    if (x1 == NULL)
	goto argh;
    *x1++ = '\0';
    x2 = strchr (x1 + 1, ':');
    if (x2 == NULL)
	goto argh;
    *x2++ = '\0';
    x3 = strchr (x2 + 1, ':');
    if (x3 == NULL)
	goto argh;
    *x3++ = '\0';
    x4 = strchr (x3 + 1, ':');
    if (x4 == NULL)
	goto argh;
    *x4++ = '\0';

    /* An RDB (partitioned) image carries its own geometry and one filesystem
     * per partition, so it is mounted as one unit per partition. The
     * geometry in the spec is only used for plain hardfiles. */
    {
	RdbPartition parts[RDB_MAX_PARTITIONS];
	int n = rdb_parse (x4, parts, RDB_MAX_PARTITIONS);

	if (n > 0) {
	    int i;
	    for (i = 0; i < n; i++) {
		RdbPartition *p = &parts[i];
		char *err = add_filesys_unit_ex (currprefs.mountinfo, 0, x4, readonly,
						 p->sectors, p->surfaces, p->reserved,
						 p->blocksize, p->start_block, p->blocks);
		if (err) {
		    write_log ("[HDF] RDB partition '%s' skipped: %s\n", p->name[0] ? p->name : "?", err);
		    fprintf (stderr, "%s\n", err);
		} else {
		    set_last_filesys_unit_dostype (currprefs.mountinfo, p->dostype);
		    units++;
		    write_log ("[HDF] RDB partition '%s' mounted: start block %u, %u blocks, dos 0x%08x, %s\n",
			       p->name[0] ? p->name : "?", p->start_block, p->blocks,
			       p->dostype, p->bootable ? "bootable" : "no boot block");
		}
	    }
	    free (x0);
	    return units;
	}
    }

    x4 = add_filesys_unit (currprefs.mountinfo, 0, x4, readonly, atoi (x0), atoi (x1), atoi (x2), atoi (x3));
    if (x4) {
	write_log ("[HDF] hardfile mount failed: %s\n", x4);
	fprintf (stderr, "%s\n", x4);
	free (x0);
	return 0;
    }

    free (x0);
    return 1;

 argh:
    free (x0);
    fprintf (stderr, "Bad hardfile parameter specified - type \"uae -h\" for help.\n");
    return 0;
}
