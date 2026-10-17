#ifndef CFGFILE_H_
#define CFGFILE_H_

extern char * make_hard_dir_cfg_line (char *dst);
extern char * make_hard_file_cfg_line (char *dst);
extern void parse_filesys_spec (int readonly, char *spec);
/* Returns the number of units that were mounted. A plain hardfile adds one
 * unit; an RDB image adds one unit per partition. */
extern int parse_hardfile_spec (int readonly, char *spec);

#endif
