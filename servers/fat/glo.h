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

/* Service internals */
EXTERN enum { NAKED, MOUNTED, UNMOUNTED } state;
EXTERN endpoint_t SELF_E;
EXTERN message m_in, m_out;

/* Communication with VFS */
EXTERN _PROTOTYPE (int (*vfs_req_vec[]), (void) ); /* VFS requests table */

/* Communication with driver */
EXTERN endpoint_t driver_e;	/* driver to use for the device */
EXTERN dev_t dev;		/* device the file system is mounted on */
EXTERN char fs_dev_label[16+1];	/* name of the device driver that is handled
				 * by this FS proc.
				 */

/* Buffer cache. */
EXTERN struct buf *buf;
#if 0
EXTERN struct buf **buf_hash;   /* the buffer hash table */
EXTERN struct buf *front;	/* points to least recently used free block */
EXTERN struct buf *rear;	/* points to most recently used free block */
#endif
TAILQ_HEAD(lruhead, buf) lru;
LIST_HEAD(bufhashhead, buf) *buf_hash;   /* the buffer hash table */

EXTERN int nr_bufs;
EXTERN int bufs_in_use;		/* # bufs currently in use (not on free list)*/
EXTERN int may_use_vmcache;

/* our block size. */
EXTERN unsigned int mfs_block_size;

/* User-settable options */
EXTERN int read_only;		/* is the file system mounted read-only? */

EXTERN uid_t use_uid;		/* use this uid */
EXTERN gid_t use_gid;		/* use this gid */
EXTERN mode_t use_file_mask;	/* show files with this mode */
EXTERN mode_t use_dir_mask;	/* show directories with this mode */
EXTERN int keep_atime;		/* do not update atime and access bits */

EXTERN int verbose;		/* emit comments on the console; debugging */

EXTERN enum { LFN_IGNORE, LFN_LOOK, LFN_USE } lfn_state;

EXTERN struct superblock superblock; /* file system fundamental values */

#endif
