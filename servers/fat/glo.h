/* Global variables used by the FAT file system
 *
 * Auteur: Antoine Leca, aout 2010.
 */

#ifndef FAT_GLO_H_
#define FAT_GLO_H_

/* EXTERN should be extern except for the table file */
#ifdef _TABLE
#undef EXTERN
#define EXTERN
#endif

#ifndef _TYPES_H
#error need to #include <sys/types.h>		/* dev_t */
#endif

#ifndef _TYPE_H
#error need to #include <minix/type.h>		/* endpoint_t */
#endif

#ifndef _IPC_H
#error need to #include <minix/ipc.h>		/* message */
#endif

EXTERN enum { NAKED, MOUNTED, UNMOUNTED } state;

EXTERN _PROTOTYPE (int (*vfs_req_vec[]), (void) ); /* VFS requests table */

EXTERN message m_in, m_out;

EXTERN dev_t dev;		/* device the file system is mounted on */
EXTERN char fs_dev_label[16+1];	/* name of the device driver that is handled
				 * by this FS proc.
				 */
EXTERN endpoint_t driver_ep;	/* driver to use for the device */
EXTERN int read_only;		/* is the file system mounted read-only? */

EXTERN uid_t use_uid;		/* use this uid */
EXTERN gid_t use_gid;		/* use this gid */
EXTERN mode_t use_umask;	/* show modes with this usermask */

EXTERN enum { LFN_IGNORE, LFN_LOOK, LFN_USE } lfn_state;

EXTERN block_t resBlk, resSiz,	/* reserved zone: block nr (=0) and size */
	fatBlk, fatsSiz,	/* FATs: starting block nr and total size */
	rootBlk, rootSiz,	/* root: starting block nr and total size */
	clustBlk, clustSiz,	/* data: starting block nr and total size */
		totalSiz;	/* file system: total size in blocks */

EXTERN unsigned bpblock;	/* bytes per block (sector) */
EXTERN int bnshift;		/* shift off_t (file offset) right this
				 *  amount to get a block number */
EXTERN off_t brelmask;		/* and a file offset with this mask
				 *  to get block rel offset */
EXTERN unsigned blkpcluster;	/* blocks per cluster */
EXTERN unsigned bpcluster;	/* bytes per cluster */
EXTERN int cnshift;		/* shift off_t (file offset) right this
				 *  amount to get a cluster number */
EXTERN off_t crelmask;		/* and a file offset with this mask
				 *  to get cluster rel offset */

EXTERN int nFATs;		/* number of FATs */
EXTERN int blkpfat;		/* blocks per FAT */
EXTERN unsigned fatmask;	/* FATxx_MASK; gives the kind of FAT */

EXTERN int rootEntries;		/* number of entries in root dir */

EXTERN zone_t maxClust;		/* number of last allocatable cluster */
EXTERN int freeClustValid;	/* indicates that next 2 values are valid: */
EXTERN zone_t freeClust;	/* total number of free clusters */
EXTERN zone_t nextClust;	/* number of next free cluster */

EXTERN int depclust;		/* directory entries per cluster */

#endif
