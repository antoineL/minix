/* This file contains the request dispatcher table.
 *
 * Auteur: Antoine Leca, aout 2010.
 * Updated:
 */

#define _POSIX_SOURCE 1
#define _MINIX 1

#include <sys/types.h>

#include <minix/config.h>
#include <minix/const.h>
#include <minix/type.h>
#include <minix/ipc.h>

#include <limits.h>	/* for NGROUPS_MAX, work around fishy headers */
#include <minix/vfsif.h>

#include "const.h"
#include "type.h"
#include "proto.h"
#define _TABLE
#include "glo.h"

/* Dispatch table for requests. This is for the protocol as revised
 * in December 2009 (SVN rev. 5780)
 */
PUBLIC _PROTOTYPE( int (*vfs_req_vec[]), (void) ) = {
	no_sys,		/*  0			*/
	no_sys,		/*  1 (was getnode)	*/
	do_putnode,	/*  2 putnode		*/
/*WRK*/	readonly,	/*  3 slink		*/
 /**/	readonly,	/*  4 ftrunc		*/
 /**/	readonly,	/*  5 chown		*/
 /**/	readonly,	/*  6 chmod		*/
 /**/	do_nothing,	/*  7 inhibread		*/
	do_stat,	/*  8 stat		*/
 /**/	readonly,	/*  9 utime		*/
	do_fstatfs,	/* 10 fstatfs		*/
	do_blockrw,	/* 11 bread		*/
	do_blockrw,	/* 12 bwrite		*/
 /**/	readonly,	/* 13 unlink		*/
 /**/	readonly,	/* 14 rmdir		*/
	do_unmount,	/* 15 unmount		*/
 /**/	do_nothing,	/* 16 sync		*/
	do_new_driver,	/* 17 new_driver	*/
 /**/	do_nothing,	/* 18 flush		*/
	do_read,	/* 19 read		*/
 /**/	readonly,	/* 20 write		*/
 /**/	readonly,	/* 21 mknod		*/
 /**/	readonly,	/* 22 mkdir		*/
 /**/	readonly,	/* 23 create		*/
 /**/	readonly,	/* 24 link		*/
 /**/	readonly,	/* 25 rename		*/
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
