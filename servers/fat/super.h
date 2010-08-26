/* File system fundamental values used by the FAT file system server
 *
 * Auteur: Antoine Leca, aout 2010.
 * Updated:
 */

#ifndef FAT_SUPER_H_
#define FAT_SUPER_H_

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

#include <minix/types.h>

typedef	u32_t	sector_t;
typedef	zone_t	cluster_t;	/* similar concept in Minix FS */

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

/* FIXME: hack to achieve compiling, but will make static analysers sick. */
#ifndef MAX_BLOCK_SIZE
#define MAX_BLOCK_SIZE	1	/* not the real size */
#endif

/* content of a buffer */
union blkdata_u {
  char b__data[MAX_BLOCK_SIZE];	     /* ordinary user data */
#if 0
/* directory block */
    struct direct b__dir[NR_DIR_ENTRIES(_MAX_BLOCK_SIZE)];    
/* V1 indirect block */
    zone1_t b__v1_ind[V1_INDIRECTS];	     
/* V2 indirect block */
    zone_t  b__v2_ind[V2_INDIRECTS(_MAX_BLOCK_SIZE)];	     
/* V1 inode block */
    d1_inode b__v1_ino[V1_INODES_PER_BLOCK]; 
/* V2 inode block */
    d2_inode b__v2_ino[V2_INODES_PER_BLOCK(_MAX_BLOCK_SIZE)]; 
/* bit map block */
    bitchunk_t b__bitmap[FS_BITMAP_CHUNKS(_MAX_BLOCK_SIZE)];  
#endif
};

/* These defs make it possible to use to bp->b_data instead of bp->b.b__data */
#define b_data   bp->b__data
#define b_dir    bp->b__dir
#define b_v1_ind bp->b__v1_ind
#define b_v2_ind bp->b__v2_ind
#define b_v1_ino bp->b__v1_ino
#define b_v2_ino bp->b__v2_ino
#define b_bitmap bp->b__bitmap


#endif
