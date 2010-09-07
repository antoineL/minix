/* This file contains the request dispatcher table.
 *
 * Auteur: Antoine Leca, aout 2010.
 * Updated:
 */

#define _TABLE
#include "inc.h"

#include <limits.h>	/* for NGROUPS_MAX, work around fishy headers */

/*
	#include "inode.h"
 */

PUBLIC char dot1[2] = ".";	/* used for search_dir to bypass the access */
PUBLIC char dot2[3] = "..";	/* permissions for . and .. */

/* Dispatch table for requests. This is for the protocol as revised
 * in December 2009 (SVN rev. 5780)
 */
PUBLIC _PROTOTYPE( int (*vfs_req_vec[]), (void) ) = {
	no_sys,		/*  0			*/
	no_sys,		/*  1 (was getnode)	*/
	do_putnode,	/*  2 putnode		*/
/*WRK*/	readonly,	/*  3 slink		*/
 /**/	/*do_ftrunc*/readonly,	/*  4 ftrunc		*/
 /**/	readonly,	/*  5 chown		*/
	do_chmod,	/*  6 chmod		*/
	do_inhibread,	/*  7 inhibread		*/
	do_stat,	/*  8 stat		*/
	do_utime,	/*  9 utime		*/
	do_fstatfs,	/* 10 fstatfs		*/
	do_bread,	/* 11 bread		*/
	do_bwrite,	/* 12 bwrite		*/
 /**/	/*do_unlink*/readonly,	/* 13 unlink		*/
 /**/	/*do_rmdir*/readonly,	/* 14 rmdir		*/
	do_unmount,	/* 15 unmount		*/
	do_sync,	/* 16 sync		*/
	do_new_driver,	/* 17 new_driver	*/
	do_flush,	/* 18 flush		*/
	do_read,	/* 19 read		*/
	do_write,	/* 20 write		*/
 /**/	readonly,	/* 21 mknod		*/
 /**/	/*do_mkdir*/readonly,	/* 22 mkdir		*/
 /**/	/*do_create*/readonly,	/* 23 create		*/
 /**/	readonly,	/* 24 link		*/
 /**/	/*do_rename*/readonly,	/* 25 rename		*/
	do_lookup,	/* 26 lookup		*/
/*CHK*/	no_sys,		/* 27 mountpoint	*/
	do_readsuper,	/* 28 readsuper		*/
	no_sys,		/* 29 newnode (unsupported) */
/*CHK*/	no_sys,		/* 30 rdlink		*/
	do_getdents,	/* 31 getdents		*/
#ifdef REQ_STATVFS
	do_statvfs,	/* 32 statvfs		*/
#endif
};

/* This should not fail with "array size is negative": */
extern int chkTsz[sizeof(vfs_req_vec) == NREQS*sizeof(vfs_req_vec[0]) ? 1:-1];
