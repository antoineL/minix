/* Function prototypes of the FAT file system
 *
 * Auteur: Antoine Leca, aout 2010.
 */

#ifndef FAT_PROTO_H_
#define FAT_PROTO_H_

#ifdef       COMPAT316
/* In revision 6339 (March 4, hence posterior to 3.1.6),
 * type cp_grant_id_t was moved from <minix/safecopies.h>
 * (which is not included always) to <minix/type.h> (which is).
 * So if compatibility matters, we include the former here.
 */
#include <minix/safecopies.h>
#endif

/* cache.c */
_PROTOTYPE( int do_flush, (void)					);
_PROTOTYPE( int do_sync, (void)						);
enum get_blk_arg_e {
	NORMAL,		/* forces get_block to read disk */
	ZERO_BLOCK,	/* asks get_block to fill block with zeroes */
	NO_READ,	/* prevents get_block from doing disk read */
	PREFETCH	/* tells get_block not to read or mark dev */
};
_PROTOTYPE( struct buf *get_block, (dev_t, block_t, enum get_blk_arg_e)	);
_PROTOTYPE( void init_cache, (int bufs, unsigned int blocksize)		);
/*
_PROTOTYPE( void put_block, (struct buf *bp, int block_type)		);
 */
_PROTOTYPE( void put_block, (struct buf *bp)				);
_PROTOTYPE( void rw_scattered, (dev_t dev,
			struct buf **bufq, int bufqsize, int rw_flag)	);
_PROTOTYPE( void zero_block, (struct buf *bp)				);

/* directory.c */
_PROTOTYPE( int do_getdents, (void)					);
_PROTOTYPE( int is_empty_dir, (struct inode *dir_ptr)			);
_PROTOTYPE( int lookup_dir, (struct inode *dir_ptr,
	char string[NAME_MAX], struct inode **res_inop)			);

/* direntry.c */
_PROTOTYPE( int add_direntry, (struct inode *dir_ptr,
	char string[NAME_MAX], struct inode **res_inop)			);
_PROTOTYPE( int del_direntry, (struct inode *dir_ptr,
				struct inode *ent_ptr)			);
_PROTOTYPE( struct inode *direntry_to_inode,
	(struct fat_direntry*, struct inode* dirp, struct direntryref*)	);
_PROTOTYPE( int update_direntry, (struct inode *)			);
_PROTOTYPE( int update_startclust, (struct inode *, cluster_t)		);

/* driver.c */
_PROTOTYPE( int dev_open, (endpoint_t, dev_t, int proc, int flags)	);
_PROTOTYPE( void dev_close, (endpoint_t driver_e, dev_t dev)		);
_PROTOTYPE( int do_new_driver, (void)					);
_PROTOTYPE( int label_to_driver, (cp_grant_id_t gid, size_t label_len)	);
_PROTOTYPE( int scattered_dev_io,(int op, iovec_t[], u64_t pos, int cnt));
_PROTOTYPE( int seqblock_dev_io, (int op, void *, u64_t pos, int cnt)	);

/* fat.c */
_PROTOTYPE( block_t bmap, (struct inode *rip, unsigned long position)	);
_PROTOTYPE( int clusteralloc, (cluster_t *res_clust, cluster_t fillwith));
_PROTOTYPE( int clusterfree, (cluster_t cluster)			);
_PROTOTYPE( cluster_t countfreeclusters, (void)				);
_PROTOTYPE( void done_fat_bitmap, (void)				);
_PROTOTYPE( int extendfile, (struct inode*, struct buf* *, cluster_t *)	);
_PROTOTYPE( int extendfileclear, (struct inode*rip, 
	unsigned long, struct buf * *bpp, cluster_t *ncp)		);
_PROTOTYPE( void fc_purge, (struct inode *rip, cluster_t frcn)		);
_PROTOTYPE( int freeclusterchain, (cluster_t startcluster)		);
_PROTOTYPE( int init_fat_bitmap, (void)					);

_PROTOTYPE( cluster_t lastcluster, (struct inode *, cluster_t *frcnp)	);
_PROTOTYPE( block_t peek_bmap, (struct inode *rip, unsigned long)	);

_PROTOTYPE( struct buf *new_block, (struct inode *rip, off_t position)	);


/* inode.c */
_PROTOTYPE( struct inode *dirref_to_inode, (cluster_t, unsigned)	);
_PROTOTYPE( struct inode *fetch_inode, (ino_t ino_nr)			);
_PROTOTYPE( struct inode *find_inode, (cluster_t, unsigned)		);
_PROTOTYPE( void flush_inodes, (void)					);
_PROTOTYPE( struct inode *get_free_inode, (void)			);
_PROTOTYPE( void get_inode, (struct inode *ino)				);
_PROTOTYPE( int have_free_inode, (void)					);
_PROTOTYPE( int have_used_inode, (void)					);
_PROTOTYPE( struct inode *init_inodes, (int inodes)			);
_PROTOTYPE( void link_inode, (struct inode *parent, struct inode *ino)	);
_PROTOTYPE( void put_inode, (struct inode *ino)				);
_PROTOTYPE( void rehash_inode, (struct inode *ino)			);
_PROTOTYPE( void unlink_inode, (struct inode *ino)			);

/* lookup.c */
_PROTOTYPE( int advance, (struct inode *dirp,
	struct inode **res_inode, char string[NAME_MAX], int chk_perm)	);
#define IGN_PERM	0
#define CHK_PERM	1
_PROTOTYPE( int do_create, (void)					);
_PROTOTYPE( int do_lookup, (void)					);
_PROTOTYPE( int do_mountpoint, (void)					);
_PROTOTYPE( int do_newnode, (void)					);
_PROTOTYPE( int do_putnode, (void)					);
_PROTOTYPE( int do_unlink, (void)					);

_PROTOTYPE( int do_rename, (void)					);
_PROTOTYPE( int do_link, (void)						);
_PROTOTYPE( int do_symlink, (void)					);
_PROTOTYPE( int do_readlink, (void)					);

/* main.c */
_PROTOTYPE( time_t clock_time, (int * hundredthp)			);
_PROTOTYPE( int do_nothing, (void)					);
_PROTOTYPE( int main, (int argc, char *argv[])				);
_PROTOTYPE( int readonly, (void)					);
_PROTOTYPE( int no_sys, (void)						);

/* mount.c */
_PROTOTYPE( int do_readsuper, (void)					);
_PROTOTYPE( int do_unmount, (void)					);

/* readwrite.c */
_PROTOTYPE( int do_bread, (void)					);
_PROTOTYPE( int do_bwrite, (void)					);
_PROTOTYPE( int do_ftrunc, (void)					);
_PROTOTYPE( int do_inhibread, (void)					);
_PROTOTYPE( int do_read, (void)						);
_PROTOTYPE( int do_write, (void)					);
_PROTOTYPE( void read_ahead, (void)					);

/* stat.c */
_PROTOTYPE( mode_t get_mode, (struct inode *)				);
_PROTOTYPE( int do_stat, (void)						);
_PROTOTYPE( int do_chmod, (void)					);
_PROTOTYPE( int do_chown, (void)					);
_PROTOTYPE( int do_utime, (void)					);

/* statfs.c */
_PROTOTYPE( int do_fstatfs, (void)					);
_PROTOTYPE( int do_statvfs, (void)					);

/* utility.c */
_PROTOTYPE( int comp_name_lfn,
		(char string[NAME_MAX+1], int, struct fat_lfnentry[])	);
#ifndef CONVNAME_ENUM_
#define CONVNAME_ENUM_
enum convname_result_e {
  CONV_OK,
  CONV_HASLOWER,
  CONV_NAMETOOLONG,
  CONV_TRAILINGDOT,
  CONV_INVAL
};
#endif
_PROTOTYPE( int conv_83toname,
		(struct fat_direntry *, char string[NAME_MAX+1])	);
_PROTOTYPE( int conv_lfntoname,
		(int, struct fat_lfnentry[], char string[NAME_MAX+1])	);
_PROTOTYPE( int conv_nameto83,
		(char string[NAME_MAX+1], struct fat_direntry *)	);
_PROTOTYPE( int conv_nametolfn, (char string[NAME_MAX+1],
		int *, struct fat_lfnentry[], struct fat_direntry *)	);
_PROTOTYPE( time_t dos2unixtime, (uint8_t dosdate[2],uint8_t dostime[2]));
_PROTOTYPE( int lfn_chksum, (struct fat_direntry * fatdp)		);
_PROTOTYPE( void unix2dostime, (time_t,
			uint8_t deDate[2], uint8_t deTime[2])		);

#endif
