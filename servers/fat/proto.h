/* Function prototypes of the FAT file system
 *
 * Auteur: Antoine Leca, aout 2010.
 */

#ifndef FAT_PROTO_H_
#define FAT_PROTO_H_

#ifndef _ANSI_H
#include <ansi.h>
#endif

/* inode.c */
_PROTOTYPE( struct inode *init_inode, (void)				);
_PROTOTYPE( struct inode *find_inode, (ino_t ino_nr)			);
_PROTOTYPE( void get_inode, (struct inode *ino)				);
_PROTOTYPE( void put_inode, (struct inode *ino)				);
_PROTOTYPE( void link_inode, (struct inode *parent, struct inode *ino)	);
_PROTOTYPE( void unlink_inode, (struct inode *ino)			);
_PROTOTYPE( struct inode *get_free_inode, (void)			);
_PROTOTYPE( int have_free_inode, (void)					);
_PROTOTYPE( int have_used_inode, (void)					);
_PROTOTYPE( int do_putnode, (void)					);

/* lookup.c */
_PROTOTYPE( int do_lookup, (void)					);

/* main.c */
_PROTOTYPE( int do_new_driver, (void)					);
_PROTOTYPE( int do_nothing, (void)					);
_PROTOTYPE( int readonly, (void)					);
_PROTOTYPE( int no_sys, (void)						);

/* mount.c */
_PROTOTYPE( int do_readsuper, (void)					);
_PROTOTYPE( int do_unmount, (void)					);

/* read.c */
int do_read(), do_blockrw();
int do_getdents();

/* stat.c */
_PROTOTYPE( mode_t get_mode, (struct inode *ino, int mode)		);
_PROTOTYPE( int do_stat, (void)						);
_PROTOTYPE( int do_chmod, (void)					);
_PROTOTYPE( int do_utime, (void)					);

/* statfs.c */
_PROTOTYPE( int do_fstatfs, (void)					);
_PROTOTYPE( int do_statvfs, (void)					);

#endif
