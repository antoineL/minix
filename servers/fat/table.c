/* This file contains the request dispatcher table.
 *
 * Auteur: Antoine Leca, aout 2010.
 * Updated:
 */

#define _TABLE
#include "inc.h"

#include <limits.h>	/* for NGROUPS_MAX, work around fishy headers */

/* Dispatch table for requests.
 * This is for the protocol as revised in December 2009 (SVN rev. 5780)
 */
PUBLIC _PROTOTYPE( int (*vfs_req_vec[]), (void) ) = {
	no_sys,		/*  0			*/
	no_sys,		/*  1 (was getnode)	*/
	do_putnode,	/*  2 putnode		*/
/*CHK*/	no_sys,		/*  3 slink		*/
	do_ftrunc,	/*  4 ftrunc		*/
	do_chown,	/*  5 chown		*/
	do_chmod,	/*  6 chmod		*/
	do_inhibread,	/*  7 inhibread		*/
	do_stat,	/*  8 stat		*/
	do_utime,	/*  9 utime		*/
	do_fstatfs,	/* 10 fstatfs		*/
	do_bread,	/* 11 bread		*/
	do_bwrite,	/* 12 bwrite		*/
	do_unlink,	/* 13 unlink		*/
	do_unlink,	/* 14 rmdir		*/
	do_unmount,	/* 15 unmount		*/
	do_sync,	/* 16 sync		*/
	do_new_driver,	/* 17 new_driver	*/
	do_flush,	/* 18 flush		*/
	do_read,	/* 19 read		*/
	do_write,	/* 20 write		*/
	do_create,	/* 21 mknod		*/
	do_create,	/* 22 mkdir		*/
	do_create,	/* 23 create		*/
/*CHK*/	no_sys,		/* 24 link		*/
 /**/	/*do_rename*/no_sys,	/* 25 rename		*/
	do_lookup,	/* 26 lookup		*/
	do_mountpoint,	/* 27 mountpoint	*/
	do_readsuper,	/* 28 readsuper		*/
	do_newnode,	/* 29 newnode		*/
/*CHK*/	no_sys,		/* 30 rdlink		*/
	do_getdents,	/* 31 getdents		*/
#ifdef REQ_STATVFS
	do_statvfs,	/* 32 statvfs		*/
#endif
};

/* This should not fail with "array size is negative": */
extern int chkTsz[sizeof(vfs_req_vec) == NREQS*sizeof(vfs_req_vec[0]) ? 1:-1];
