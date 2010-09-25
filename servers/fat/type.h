/* Various types used by the FAT file system.
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
 * A block is the basic unit of measure within any MINIX
 * file system server, and the unit managed by the cache.
 * Its size can be known with (struct statfs)s.f_bsize
 * Blocks are numbered using block_t (<minix/types.h>).
 * On disk, the file system area (on the minor device) can
 * be seen as made of blocks 0 to sb.totalSiz-1.
 *
 * A sector is the fundamental unit of measure for a FAT
 * file system.
 * Its size can be known with (struct statvfs)s.f_frsize
 * Sectors are numbered using sector_t.
 * We do not care about CHS addressing, we always assume
 * LBA addressing (within the minor device) so sectors are
 * numbered from 0 to sb.totalSecs-1.
 *
 * A cluster is the unit of allocation for files in a FAT
 * file system. The size of a cluster is a power-of-2
 * multiple of the sector size.
 * Its size can be known with (struct statvfs)s.f_bsize
 * Clusters are numbered using cluster_t == zone_t,
 * from 2 up to sb.maxClust.
 * Notice that the clusters are in the data area, which does
 * not cover the whole file system area.
 *
 * The code does not assume that sectors are the same size
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
typedef	zone_t	cluster_t;	/* similar concept in Minix FS */

/* Directory entries.
 * Directory entries also play the role of the inode for files:
 * in addition to the name, they hold the size, the timestamps
 * of last modification, etc. Missing are the members for
 * permissions, uid/gid, or count of hard links, since there are
 * no such things on MS-DOS; the only data block addressed is the
 * first one (indexed by its cluster number); the other data
 * first will be found via the FAT itself, which is a linked list
 * of "next cluster numbers".
 *
 * Special files.
 * In the FAT file system, there are only regular files and directories.
 * There are no files representing character or block specials
 * (unless some extension are used; several schemes exist, but none is
 * universal enough to warrant its support here.)
 * There are no symlink (at file system level) either; again there are
 * several schemes to circumvent this lacking, but nothing is universal.
 *
 * Because of all these shortcomings, we cannot allow the FAT file system
 * to be used as root file system on MINIX.
 *
 * Links.
 * In the FAT file system, regular files have one and only one directory
 * entry; hence their link count is always 1, and cannot be incremented.
 * Directories (except the root) have always at least one entry in its
 * parent directory, plus the first entry named '.' in itself; in addition
 * it has one more link for each subdirectory it may have, where the
 * second entry named '..' will also have the same cluster number.
 * A consequence of this multiplication of entries for a given
 * directory, is that directory entries are never updated for timestamp
 * or when size change.
 *
 * We use the properties of this to build a semi-universal system to
 * access inodes starting with a directory entry coordinates;
 * such coordinates are stored inside a struct direntryref:
 *   dr_parent    is the starting cluster number of the directory
 *                filespace where the entry is located
 *   dr_entrypos  is the entry position within that directory
 * for regular files, there is only one entry so no ambiguity;
 * for directories, we choose the '.' entry, which is always at the
 * start of the directory, hence have always entrypos==0;
 * the entry of that directory within its own parent has the
 * (non-zero) starting cluster number, so the inode can be located;
 * the '..' entries in subdirectories also have the starting cluster
 * number stored, so the inode can be located again.
 * The only difficulty is because in the root directory, there are
 * no '.' entry; the consequence is that the coordinates (root_dir, 0)
 * can address a file which is not a directory; this case should be
 * handled specially.
 *
 * In addition to starting cluster number for the parent directory,
 * entries are also marked with the cluster number where the entry
 * actually stands, in dr_abscn; this allows quick access to the datas
 * when they need to be updated. Another optimisation is to store
 * the position of the first of the lfn_direntry which are preceding.
 */
struct direntryref {
  cluster_t	dr_parent;	/* cluster pointing the directory */
  unsigned	dr_entrypos;	/* position of the fat_direntry within */
  block_t	dr_absbn;	/* block holding the entry */
  unsigned	dr_lfnpos;	/* position of the lfn_direntry, if any */
};

/* File system fundamental values used by the FAT file system server.
 * The unique instance of this structure is the global variable 'sb'.
 */
struct superblock {
  unsigned bpsector;		/* bytes per sector */
  int snshift;			/* shift off_t (file offset) right this
				 *  amount to get a sector rel number */
  off_t srelmask;		/* and a file offset with this mask
				 *  to get sector rel offset */
  unsigned bpcluster;		/* bytes per cluster */
  unsigned secpcluster;		/* sectors per cluster */
  int cnshift;			/* shift off_t (file offset) right this
				 *  amount to get a cluster rel number */
  off_t crelmask;		/* and a file offset with this mask
				 *  to get cluster rel offset */
  int csshift;			/* shift a sector number right this
				 *  amount to get a cluster number */

  sector_t resSec, resCnt,	/* reserved zone: sector nr (=0) and size */
	fatSec,   fatsCnt,	/* FATs: starting sector nr and total size */
	rootSec,  rootCnt,	/* root: starting sector nr and total size */
	clustSec, clustCnt,	/* data: starting sector nr and total size */
	          totalSecs;	/* file system: total size in sectors */

  int nFATs;			/* number of FATs */
  int secpfat;			/* sectors per FAT (each one) */
  int nibbles;			/* count of nibble (half-byte) per FAT entry*/
  unsigned fatmask;		/* FATxx_MASK; gives the kind of FAT */
  unsigned eofmask;		/* CLUSTMASK_EOFxx, accordingly */

  int rootEntries;		/* number of entries in root dir (!FAT32) */
  cluster_t rootCluster;	/* cluster for root direntries */

  cluster_t maxClust;		/* number of last allocatable cluster */
  int freeClustValid;		/* indicates that next 2 values are valid: */
  cluster_t freeClust;		/* total number of free clusters */
  cluster_t nextClust;		/* number of next free cluster */

  unsigned long maxFilesize;	/* maximum possible size for a file */
  int depsec;			/* directory entries per sector */
  int depclust;			/* directory entries per cluster */
};

/* Buffer (block) cache control values.
 * Fundamental values used by the buffer cache.
 * The unique instance of this structure is the global variable 'bcc'.
 */
struct bufcache_ctrl {
  unsigned bpblock;		/* bytes per block */
  int bnshift;			/* shift off_t (file offset) right this
				 *  amount to get a block rel number */
  off_t brelmask;		/* and a file offset with this mask
				 *  to get block rel offset */
  unsigned blkpcluster;		/* blocks per cluster */
  int cbshift;			/* shift a block number right this
				 *  amount to get a cluster number */

  block_t resBlk, resSiz,	/* reserved zone: block nr (=0) and count */
	fatBlk,   fatsSiz,	/* FATs: starting block nr and total count */
	rootBlk,  rootSiz,	/* root: starting block nr and total count */
	clustBlk, clustSiz,	/* data: starting block nr and total count */
	          totalBlks;	/* file system: total size in blocks */

  int blkpfat;			/* blocks per FAT */
  int depblk;			/* directory entries per block */

  int nbufs;			/* total number of buffers in cache */
};

/* Buffer (block) cache.
 * To acquire a block, a routine calls get_block(), telling which block
 * it wants. The block is then regarded as "in use" and has its 'b_ref'
 * field incremented. All the blocks that are not in use are chained
 * together in an LRU list, implemented as a tail-queue, with '_FIRST'
 * pointing to the least recently used block, and '_TAIL' to the most
 * recently used block. Usage for LRU is measured by the time the
 * put_block() is done. If a block is modified, the modifying routine must
 * set b_dirt to DIRTY, so the block will eventually be written to the disk.
 *
 * A free buffer is indicated with b_blocknr == NO_BLOCK (0); this is OK
 * with FAT, since the block or sector 0 is only read once at mounting,
 * and its content is never updated by the file system server.
 */
struct buf {
  /* Data portion of the buffer. Uninterpreted by cache. */
  union blkdata_u *dp;

  /* Header portion of the buffer. */
  TAILQ_ENTRY(buf) b_next;	/* used to link all free bufs in a chain */
  LIST_ENTRY( buf) b_hash;	/* used to link bufs on hash chains */
  size_t b_bytes; 	        /* Number of bytes allocated in bp */
  block_t b_blocknr;            /* block number of its (minor) device */
/* CHECKME: is it still usefull? */
  dev_t b_dev;                  /* major | minor device where block resides */
  char b_dirt;                  /* CLEAN or DIRTY */
#define CLEAN		0	/* disk and memory copies identical */
#define DIRTY		1	/* disk and memory copies differ */
#define NOTREAD		2	/* just prefetched, not read in memory */
  char b_count;                 /* number of users of this buffer */

/* FIXME: we need an indication (or a hook) when the buffer is for
 * some FAT sector, and should be mirrored (written several times)
 * when flushed to disk...
 */
/* FIXME: we may need some special stuff to insure that FAT12
 * even and odd blocks are contiguous in logical address space
 * (since entry 342 lies part in sector 0 and part in sector 1)
 */

};
#define	IS_FREE_BLOCK(bp)	((bp)->b_blocknr == NO_BLOCK)
	/* beware: MFS cache considers b_dev == NO_DEV instead. */

/* actual content of directories */
union direntry_u {
  struct fat_direntry d_direntry;
  struct fat_lfnentry d_lfnentry;
};

#define DIR_ENTRY_SIZE     usizeof(union direntry_u)  /* # bytes/dir entry */
#define NR_DIR_ENTRIES(sz) ((sz)/DIR_ENTRY_SIZE)   /* # dir entries/struct */

/* content of a buffer */
union blkdata_u {
  char b__data[MAX_BLOCK_SIZE];	     /* ordinary user data */
  union direntry_u b__dir[NR_DIR_ENTRIES(MAX_BLOCK_SIZE)];
  unsigned char b__fat16[2][MAX_BLOCK_SIZE/2]; /* FAT16 chains */
  unsigned char b__fat32[4][MAX_BLOCK_SIZE/4]; /* FAT32 chains */
};
/* These defs make it possible to use to bp->b_data instead of bp->dp->b__data */
#define b_data		dp->b__data
#define b_dir		dp->b__dir
/* beware: the following fields are stored in little-endian packed form */
#define b_zfat16	dp->b__fat16
#define b_zfat32	dp->b__fat32

/*
CHECKME: refine this
 *  The fat entry cache as it stands helps make extending
 *  files a "quick" operation by avoiding having to scan
 *  the fat to discover the last cluster of the file.
 *  The cache also helps sequential reads by remembering
 *  the last cluster read from the file.  This also prevents
 *  us from having to rescan the fat to find the next cluster
 *  to read.  This cache is probably pretty worthless if a
 *  file is opened by multiple processes.
 */
/* FIXME when stuff stable: these constants should move to const.h */
#define	FC_SIZE		2	/* number of entries in the cache */
#define	FC_LASTMAP	0	/* entry the last call to bmap() resolved to */
#define	FC_LASTFC	1	/* entry for the last cluster in the file */

#define	FCE_EMPTY	(cluster_t)-1	/* not an actual cluster # */
/*
 * The fat cache structure.
 * fc_abscn is the filesystem relative cluster number that corresponds
 * to the (beginning of the) file relative cluster number (fc_frcn).
 */
struct fatcache {
  cluster_t fc_frcn;		/* file relative cluster number	*/
  cluster_t fc_abscn;		/* (filesystem relative) cluster number	*/
  block_t fc_bn;		/* (filesystem relative) block number */
};

/* Inode structure, as managed internally.
 * This is the in memory variant of a FAT directory entry.
 */
struct inode {
  LIST_ENTRY(inode) i_hash;	/* dirref hashtable chain entry */
  LIST_ENTRY(inode) i_hashclust; /* cluster hashtable chain entry */
  LIST_ENTRY(inode) i_hashref;	/* dirref hashtable chain entry */
  unsigned short i_index;	/* inode index for quick reference */
  unsigned short i_gen;		/* inode generation number */
  unsigned short i_ref;		/* VFS reference count */
  unsigned short i_flags;	/* any combination of I_* flags */
  TAILQ_ENTRY(inode) i_free;	/* free list chain entry */

/* FIXME */
	  ino_t i_numKILL;				/* inode number for quick reference */
  struct inode *i_parent;		/* parent inode pointer */
  LIST_HEAD(child_head, inode) i_child;	/* child inode anchor */
  LIST_ENTRY(inode) i_next;		/* sibling inode chain entry */

  struct fat_direntry i_direntry; /* actual data of entry */
/* Shorthand macros used to reference fields in the direntry */
#define	i_Name		i_direntry.deName
#define	i_Extension	i_direntry.deExtension
#define	i_Attributes	i_direntry.deAttributes
#define	i_LCase		i_direntry.deLCase
/* beware: the following fields are stored in little-endian packed form */
#define	iz_1stClusterLo	i_direntry.deStartCluster
#define	iz_1stClusterHi	i_direntry.deStartClusterHi
#define	iz_FileSize	i_direntry.deFileSize
#define	iz_MDate	i_direntry.deMDate
#define	iz_MTime	i_direntry.deMTime
#define	iz_ADate	i_direntry.deADate

  char * i_long_filename;	/* long file name (\0 terminated) */
/* FIXME: useful? using malloc/free? */
  int i_lfn_alloclen;		/* size allocated to above field */

  mode_t i_mode;		/* file type, protection, as seen by VFS */
  unsigned long i_size;		/* current file size in bytes */
  cluster_t i_clust;		/* number of first cluster of data */

  struct direntryref i_dirref;	/* coordinates of this entry */
#define	i_parent_clust	i_dirref.dr_parent /* cluster pointing the directory*/
#define	i_entrypos	i_dirref.dr_entrypos /* position of the entry within*/
#define	i_actual_clust	i_dirref.dr_abscn /* cluster pointing the entry */

  /* cached values for timestamps: */
  time_t i_btime;		/* when was file created (birth) */
  time_t i_mtime;		/* when was file data last changed */
  time_t i_atime;		/* when was file data last accessed */
  time_t i_ctime;		/* when was inode itself changed */

  struct fatcache i_fc[FC_SIZE];	/* fat cache */
};

/* Inode flags: i_flags is the |-ing of all relevant flags */
#define I_DIR		0x0001		/* this inode is a directory */
#define I_ROOTDIR	0x0002		/* this inode is the root directory */
#define I_DIRSIZED	0x0004		/* size is not estimated */
#define I_DIRNOTSIZED	0x0008		/* size is pure guess */

#define I_MOUNTPOINT	0x0010		/* this inode is a mount point */
#define I_ORPHAN	0x0020		/* the path leading to this inode
					 * was unlinked, but is still used
					 */

#define I_SEEK		0x0100		/* last operation incured a seek */
#define I_MTIME		0x0200		/* touched, should update MTime */
#define I_ACCESSED	0x0400		/* file was accessed */
#define I_DIRTY		0x0800		/* on-disk copy differs */

#define I_HASHED	0x1000		/* linked-in in dirref hastable */
#define I_HASHED_CLUST	0x2000		/* linked-in in cluster hastable */
#define I_HASHED_DIRREF	0x1000		/* linked-in in dirref hastable */

/* Placeholders for cached timestamps: */
#define	TIME_UNDETERM	(time_t)0	/* the value is undeterminate */
#define	TIME_NOT_CACHED	(time_t)1	/* compute from on-disk value */
#define	TIME_UPDATED	(time_t)2	/* inode was updated and is dirty;
					 * value to be retrieved from clock
					 */
#define	TIME_UNKNOWN	(time_t)3	/* nothing known about the value */
	/* Upper values are real timestamps which can be used directly */

/*
 *  Values for the de_flag field of the denode.
 */
#define NO_SEEK            0	/* i_seek = NO_SEEK if last op was not SEEK */
#define ISEEK              1	/* i_seek = ISEEK if last op was SEEK */

#define I_HANDLE	0x02		/* this inode has an open handle */
#define	DELOCKED	0x0001		/* directory entry is locked	*/
#define	DEWANT		0x0002		/* someone wants this de	*/
#define	DERENAME	0x0004		/* de is being renamed		*/
	#define	DEUPD		0x0008		/* file has been modified	*/
#define	DESHLOCK	0x0010		/* file has shared lock		*/
#define	DEEXLOCK	0x0020		/* file has exclusive lock	*/
#define	DELWAIT		0x0040		/* someone waiting on file lock	*/
#define	DEMOD		0x0080		/* denode wants to be written back
					 *  to disk			*/

#define	CLUST_CONVROOT	1	/* fixed root is given a conventional number*/

#define IS_DIR(i)	((i)->i_flags & I_DIR)
#define IS_ROOT(i)	((i)->i_flags & I_ROOTDIR)

#define HAS_CHILDREN(i)	(!LIST_EMPTY(& (i)->i_child))

#define MODE_TO_DIRFLAG(m)	(S_ISDIR(m) ? I_DIR : 0)

#endif
