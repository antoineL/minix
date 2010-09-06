/* Function prototypes of the FAT file system
 *
 * Auteur: Antoine Leca, aout 2010.
 */

#ifndef FAT_PROTO_H_
#define FAT_PROTO_H_

#ifndef _IPC_H
#error need to #include <minix/ipc.h>		/* message */
#endif
#ifndef FAT_TYPE_H_
#error need to #include "type.h"
#endif

#if 0
struct buf;
struct inode;
struct direntry;
typedef	zone_t	cluster_t;	/* similar concept in Minix FS */
#endif

/* cache.c */
_PROTOTYPE( int do_flush, (void)					);
_PROTOTYPE( int do_sync, (void)						);
enum get_block_arg_e {
	NORMAL,		/* forces get_block to do disk read */
	NO_READ,	/* prevents get_block from doing disk read */
	PREFETCH	/* tells get_block not to read or mark dev */
};
_PROTOTYPE( struct buf *get_block, (dev_t, block_t blocknr, enum get_block_arg_e)	);
_PROTOTYPE( void init_cache, (int bufs)					);
/*
_PROTOTYPE( void put_block, (struct buf *bp, int block_type)		);
 */
_PROTOTYPE( void put_block, (struct buf *bp)				);
_PROTOTYPE( void rw_scattered, (dev_t dev,
			struct buf **bufq, int bufqsize, int rw_flag)	);
_PROTOTYPE( void zero_block, (struct buf *bp)				);

/* direntry.c */
_PROTOTYPE( int do_getdents, (void)					);
enum search_dir_arg_e {
	LOOK_UP,	/* tells search_dir to lookup string */
	ENTER,		/* tells search_dir to make dir entry */
	DELETE,		/* tells search_dir to delete entry */
	IS_EMPTY	/* tells search_dir to ret. OK or ENOTEMPTY */  
};
_PROTOTYPE( int search_dir, (struct inode *ldir_ptr, 
	char string [NAME_MAX], ino_t *numb,
	enum search_dir_arg_e flag, int check_permissions)		);	
_PROTOTYPE( int lookup_dir, (struct inode *dir_ptr,
	char string[NAME_MAX], struct inode **res_inop)			);

/* driver.c */
_PROTOTYPE( int dev_open, (endpoint_t driver_e, dev_t dev, int proc,
			   int flags)					);
_PROTOTYPE( void dev_close, (endpoint_t driver_e, dev_t dev)		);
_PROTOTYPE( int do_new_driver, (void)					);
_PROTOTYPE( int scattered_dev_io,(int op, iovec_t[], u64_t pos, int cnt));
_PROTOTYPE( int seqblock_dev_io, (int op, void *, u64_t pos, int cnt)	);

/* fat.c */
_PROTOTYPE( block_t bmap, (struct inode *rip, off_t position)		);
_PROTOTYPE( sector_t smap, (struct inode *rip, off_t position)		);
_PROTOTYPE( struct buf *new_block, (struct inode *rip, off_t position)	);


/* inode.c */
_PROTOTYPE( struct inode *init_inode, (void)				);
_PROTOTYPE( struct inode *fetch_inode, (ino_t ino_nr)			);
_PROTOTYPE( struct inode *find_inode, (cluster_t)			);
_PROTOTYPE( void get_inode, (struct inode *ino)				);
_PROTOTYPE( void put_inode, (struct inode *ino)				);
_PROTOTYPE( void link_inode, (struct inode *parent, struct inode *ino)	);
_PROTOTYPE( void unlink_inode, (struct inode *ino)			);
_PROTOTYPE( struct inode *get_free_inode, (void)			);
_PROTOTYPE( int have_free_inode, (void)					);
_PROTOTYPE( int have_used_inode, (void)					);

/* lookup.c */
_PROTOTYPE( int advance, (struct inode *dirp,
	struct inode **res_inode, char string[NAME_MAX], int chk_perm)	);
#define IGN_PERM	0
#define CHK_PERM	1
_PROTOTYPE( int do_lookup, (void)					);
_PROTOTYPE( int do_putnode, (void)					);

/* main.c */
_PROTOTYPE( int do_nothing, (void)					);
_PROTOTYPE( int readonly, (void)					);
_PROTOTYPE( int no_sys, (void)						);
_PROTOTYPE( void reply, (int, message *)				);

/* mount.c */
_PROTOTYPE( int do_readsuper, (void)					);
_PROTOTYPE( int do_unmount, (void)					);

/* readwrite.c */
_PROTOTYPE( int do_bread, (void)					);
_PROTOTYPE( int do_bwrite, (void)					);
_PROTOTYPE( int do_read, (void)						);
_PROTOTYPE( int do_write, (void)					);
_PROTOTYPE( void read_ahead, (void)					);

/* stat.c */
_PROTOTYPE( mode_t get_mode, (struct inode *ino, int mode)		);
_PROTOTYPE( int do_stat, (void)						);
_PROTOTYPE( int do_chmod, (void)					);
_PROTOTYPE( int do_utime, (void)					);

/* statfs.c */
_PROTOTYPE( int do_fstatfs, (void)					);
_PROTOTYPE( int do_statvfs, (void)					);
#endif
