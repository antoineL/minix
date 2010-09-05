/* Various types used by the FAT file system.
 * Some parts have their own, separated headers
 * which hold the type descriptions:
 *   fat.h	description of the ondisk structures
 *   super.h	file system fundamental values
 *   inode.h	inode structure (also hold dir.entry)
 *   cache.h	block buffers and cache system
 *
 * Auteur: Antoine Leca, aout 2010.
 * Updated:
 */

#ifndef FAT_TYPE_H_
#define FAT_TYPE_H_

#include <sys/queue.h>
#include <minix/types.h>

#include "fat.h"		/* description of the ondisk structures */

/* Terminology:
 * A block is the basic unit of measure within a MINIX file
 * system server, and the unit managed by the cache.
 * Blocks are numbered using block_t (<minix/types.h>).
 * On disk, the file system area (on the minor device) can
 * be seen as made of blocks 0 to totalSiz-1.
 *
 * A sector is the fundamental unit of measure for a FAT
 * file system.
 * Its size can be known with (struct statvfs)s.f_frsize
 * Sectors are numbered using sector_t.
 * We do not care about CHS addressing, we always assume
 * LBA addressing (within the minor device) so sectors are
 * numbered from 0 to totalSecs-1.
 *
 * A cluster is the unit of allocation for files in a FAT
 * file system. The size of a cluster is a power-of-2
 * multiple of the sector size.
 * Its size can be known with (struct statvfs)s.f_bsize
 * Clusters are numbered using cluster_t == zone_t,
 * from 2 up to maxClust.
 *
 * The code should not assume that sectors are the same size
 * as blocks; in fact, it should be possible to independantly
 * modifiy the cache system: for example, to take advantage of
 * a central cache with a direct relationship with the VM
 * system; or to use a fixed-size cache system, without
 * loosing the possibility to mount odd-sized FAT file systems.
 * This also means that blocksize can be either bigger or
 * smaller than sectorsize.
 * However we assert that clusters are multiple of blocks.
 */

typedef	u32_t	sector_t;

#ifndef CLUSTER_T
#define CLUSTER_T
typedef	zone_t	cluster_t;	/* similar concept in Minix FS */
#endif

/* Buffer (block) cache.
 * To acquire a block, a routine calls get_block(), telling which block it
 *  wants. The block is then regarded as "in use" and has its 'b_count'
 * field incremented. All the blocks that are not in use are chained
 * together in an LRU list, implemented as a tail-queue, with '_FIRST'
 * pointing to the least recently used block, and '_TAIL' to the most
 * recently used block. Usage for LRU is measured by the time the
 * put_block() is done. If a block is modified, the modifying routine must
 * set b_dirt to DIRTY, so the block will eventually be rewritten to the disk.
 */

/* CHECKME!!! better use b_bytes==0 ?
 * make some macros
 * describe all the states a buffer can be (allocated/no, free/no, dirty etc.)
 */
/* A block is free if b_dev == NO_DEV. */

struct buf {
  /* Data portion of the buffer. Uninterpreted by cache. */
  union blkdata_u *dp;

  /* Header portion of the buffer. */
  TAILQ_ENTRY(buf) b_next;	/* used to link all free bufs in a chain */
  LIST_ENTRY( buf) b_hash;	/* used to link bufs on hash chains */
  size_t b_bytes; 	        /* Number of bytes allocated in bp */
  block_t b_blocknr;            /* block number of its (minor) device */
  dev_t b_dev;                  /* major | minor device where block resides */
  char b_dirt;                  /* CLEAN or DIRTY */
#define CLEAN              0	/* disk and memory copies identical */
#define DIRTY              1	/* disk and memory copies differ */
  char b_count;                 /* number of users of this buffer */
};

/* CHECKME... */
#define BUFHASH(b) ((b) % nr_bufs)


/* FIXME: hack to achieve compiling, but will make static analysers sick. */
#ifndef MAX_BLOCK_SIZE
#define MAX_BLOCK_SIZE	512	/* not the real size */
#endif

/* content of a buffer */
union direntry_u {
  struct fat_direntry d_direntry;
  struct fat_lfnentry d_lfnentry;
};

union blkdata_u {
  char b__data[MAX_BLOCK_SIZE];	     /* ordinary user data */
/* directory block */
#ifdef DIR_ENTRY_SIZE
  union direntry_u b__dir[MAX_BLOCK_SIZE / DIR_ENTRY_SIZE];
#else
  union direntry_u b__dir[16];
#endif
  unsigned char b__fat16[2][MAX_BLOCK_SIZE/2]; /* FAT16 chains */
  unsigned char b__fat32[4][MAX_BLOCK_SIZE/4]; /* FAT32 chains */
};

/* These defs make it possible to use to bp->b_data instead of bp->dp->b__data */
#define b_data		dp->b__data
#define b_dir		dp->b__dir
#define b_fat16		dp->b__fat16
#define b_fat32		dp->b__fat32

/* File system fundamental values used by the FAT file system server.
 * The unique instance of this structure is the global variable 'sb'.
 */
struct superblock {
  unsigned bpblock;		/* bytes per block */
  int bnshift;			/* shift off_t (file offset) right this
				 *  amount to get a block rel number */
  off_t brelmask;		/* and a file offset with this mask
				 *  to get block rel offset */
  unsigned bpsector;		/* bytes per sector */
  int snshift;			/* shift off_t (file offset) right this
				 *  amount to get a sector rel number */
  off_t srelmask;		/* and a file offset with this mask
				 *  to get sector rel offset */
  unsigned bpcluster;		/* bytes per cluster */
  unsigned blkpcluster;		/* blocks per cluster */
  unsigned secpcluster;		/* sectors per cluster */
  int cnshift;			/* shift off_t (file offset) right this
				 *  amount to get a cluster rel number */
  off_t crelmask;		/* and a file offset with this mask
				 *  to get cluster rel offset */

  sector_t resSec, resCnt,	/* reserved zone: sector nr (=0) and size */
	fatSec,   fatsCnt,	/* FATs: starting sector nr and total size */
	rootSec,  rootCnt,	/* root: starting sector nr and total size */
	clustSec, clustCnt,	/* data: starting sector nr and total size */
	          totalSecs;	/* file system: total size in sectors */

  block_t resBlk, resSiz,	/* reserved zone: block nr (=0) and size */
	fatBlk,   fatsSiz,	/* FATs: starting block nr and total size */
	rootBlk,  rootSiz,	/* root: starting block nr and total size */
	clustBlk, clustSiz,	/* data: starting block nr and total size */
	          totalSiz;	/* file system: total size in blocks */

  int nFATs;			/* number of FATs */
  int secpfat;			/* sectors per FAT (each one) */
  int blkpfat;			/* blocks per FAT */
  unsigned fatmask;		/* FATxx_MASK; gives the kind of FAT */
  unsigned eofmask;		/* CLUSTMASK_EOFxx, accordingly */

  int rootEntries;		/* number of entries in root dir (!FAT32) */
  cluster_t rootCluster;	/* cluster for root direntries */

  cluster_t maxClust;		/* number of last allocatable cluster */
  int freeClustValid;		/* indicates that next 2 values are valid: */
  cluster_t freeClust;		/* total number of free clusters */
  cluster_t nextClust;		/* number of next free cluster */

  off_t maxFilesize;		/* maximum possible size for a file */

  int depblk;			/* directory entries per block */
  int depsec;			/* directory entries per sector */
  int depclust;			/* directory entries per cluster */
};

/* Inode structure, as managed internally.
 *
 */

/* Number of inodes, and inode numbers.
 * There are no natural value in FAT file systems to represent inode numbers.
 * So we synthetise a number which is passed back to VFS.
 * In order to catch phasing errors, that number is built using bitmasks,
 * combining the index in an array for quick reference, and a generation
 * number which is hopefully not reused quickly.
 * Some useful macros are defined below.
 *
 * The following number must not exceed 16, the i_index field is only a short.
 * Also, it is good to be close to the number of inodes in VFS, currently 512.
 */
#define NUM_INODE_BITS		9

#if 0
/* We cannot use inode number 0, so to be able to use bitmasks to combine
 * inode and generation numbers, we have to use one fewer than the maximum of
 * inodes possible by using NUM_INODE_BITS bits.
 */
#define NUM_INODES	((1 << NUM_INODE_BITS) - 1)

#define ROOT_INODE_NR	1

#else
/* Size of the inodes[] array.
 * We should avoid inode number 0. Since the root inode is never freed,
 * we give it the [0] slot, and we make sure it gets a non-zero generation
 * number, so the resulting combined inode number is non-zero.
 */
#define NUM_INODES	(1 << NUM_INODE_BITS)

#define ROOT_GEN_NR	0x4007
#define ROOT_INODE_NR	(ROOT_GEN_NR << NUM_INODE_BITS)

#endif

struct direntryref {
  cluster_t	de_clust;	/* cluster pointing the directory */
  unsigned	de_entrypos;	/* position of the entry within */
};

/*
 *  The fat entry cache as it stands helps make extending
 *  files a "quick" operation by avoiding having to scan
 *  the fat to discover the last cluster of the file.
 *  The cache also helps sequential reads by remembering
 *  the last cluster read from the file.  This also prevents
 *  us from having to rescan the fat to find the next cluster
 *  to read.  This cache is probably pretty worthless if a
 *  file is opened by multiple processes.
 */
#define	FC_SIZE		3	/* number of entries in the cache */
#define	FC_LASTMAP	0	/* entry the last call to bmap() resolved to */
#define	FC_LASTFC	1	/* entry for the last cluster in the file */

#define	FCE_EMPTY	0xffff	/* doesn't represent an actual cluster # */
/*
 * The fat cache structure.
 * fc_bn is the filesystem relative block number that corresponds
 * to the (beginning of the) file relative cluster number (fc_frcn).
 */
struct fatcache {
  cluster_t fc_frcn;		/* file relative cluster number	*/
  block_t fc_bn;		/* (filesystem relative) block number */
};

/* This is the in memory variant of a FAT directory entry.
 */
struct inode {
  struct inode *i_parent;		/* parent inode pointer */
  LIST_HEAD(child_head, inode) i_child;	/* child inode anchor */
  LIST_ENTRY(inode) i_next;		/* sibling inode chain entry */
  LIST_ENTRY(inode) i_hash;		/* hashtable chain entry */
  unsigned short i_index;			/* inode number for quick reference */
  unsigned short i_gen;			/* inode generation number */
  unsigned short i_ref;			/* VFS reference count */
  unsigned short i_flags;		/* any combination of I_* flags */
  union {
	TAILQ_ENTRY(inode) u_free;	/* free list chain entry */
	struct direntryref u_dirref;	/* coordinates of this entry */
  } i_u;

  struct fat_direntry i_direntry;	/* actual data of entry */
/*
 * Shorthand macros used to reference fields in the direntry
 *  contained in the denode structure.
 */
#define	i_Name		i_direntry.deName
#define	i_Extension	i_direntry.deExtension
#define	i_Attributes	i_direntry.deAttributes
#define	i_LCase		i_direntry.deLCase
/* beware: the following fields are stored in little-endian packed form */
#define	i_StartCluster	i_direntry.deStartCluster
#define	i_HighClust	i_direntry.deHighClust
#define	i_FileSize	i_direntry.deFileSize

  off_t i_size;				/* current file size in bytes */
  cluster_t i_clust;			/* number of first cluster of data */
  struct fatcache i_fc[FC_SIZE];	/* fat cache */

  mode_t i_mode;		/* file type, protection, etc. */
  char i_mountpoint;		/* true if mounted on */

};

#define i_free		i_u.u_free
#define i_dirref	i_u.u_dirref

#define I_DIR		0x01		/* this inode represents a directory */
#define I_HANDLE	0x02		/* this inode has an open handle */

/*
 *  Values for the de_flag field of the denode.
 */
#define	DELOCKED	0x0001		/* directory entry is locked	*/
#define	DEWANT		0x0002		/* someone wants this de	*/
#define	DERENAME	0x0004		/* de is being renamed		*/
#define	DEUPD		0x0008		/* file has been modified	*/
#define	DESHLOCK	0x0010		/* file has shared lock		*/
#define	DEEXLOCK	0x0020		/* file has exclusive lock	*/
#define	DELWAIT		0x0040		/* someone waiting on file lock	*/
#define	DEMOD		0x0080		/* denode wants to be written back
					 *  to disk			*/

/* Some handy macros to manage the synthetised inode numbers.
 * warning: the following line is not a proper macro
 */
#define INODE_NR(i)	(((i)->i_gen << NUM_INODE_BITS) | (i)->i_index)
#if 0
#define INODE_INDEX(n)	(((n) & ((1 << NUM_INODE_BITS) - 1)) - 1)
#else
#define INODE_INDEX(n)	( (n) & ((1 << NUM_INODE_BITS) - 1) )
#endif
#define INODE_GEN(n)	(((n) >> NUM_INODE_BITS) & 0xffff)

#if 0
#define IS_ROOT(i)	((i)->i_num == ROOT_INODE_NR)
#else
#define IS_ROOT(i)	((i)->i_index == (ROOT_INODE_NR & 0xffff))
#endif

#define IS_DIR(i)	((i)->i_flags & I_DIR)
#define HAS_CHILDREN(i)	(!LIST_EMPTY(& (i)->i_child))

#define MODE_TO_DIRFLAG(m)	(S_ISDIR(m) ? I_DIR : 0)

#endif
