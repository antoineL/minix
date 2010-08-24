/* File system fundamental values used by the FAT file system
 *
 * Auteur: Antoine Leca, aout 2010.
 */

#ifndef FAT_SUPER_H_
#define FAT_SUPER_H_

struct superblock {
  block_t resBlk, resSiz,	/* reserved zone: block nr (=0) and size */
	fatBlk, fatsSiz,	/* FATs: starting block nr and total size */
	rootBlk, rootSiz,	/* root: starting block nr and total size */
	clustBlk, clustSiz,	/* data: starting block nr and total size */
		totalSiz;	/* file system: total size in blocks */

  unsigned bpblock;		/* bytes per block (sector) */
  int bnshift;			/* shift off_t (file offset) right this
				 *  amount to get a block number */
  off_t brelmask;		/* and a file offset with this mask
				 *  to get block rel offset */
  unsigned blkpcluster;		/* blocks per cluster */
  unsigned bpcluster;		/* bytes per cluster */
  int cnshift;			/* shift off_t (file offset) right this
				 *  amount to get a cluster number */
  off_t crelmask;		/* and a file offset with this mask
				 *  to get cluster rel offset */

  int nFATs;			/* number of FATs */
  int blkpfat;			/* blocks per FAT */
  unsigned fatmask;		/* FATxx_MASK; gives the kind of FAT */

  int rootEntries;		/* number of entries in root dir */

  zone_t maxClust;		/* number of last allocatable cluster */
  int freeClustValid;		/* indicates that next 2 values are valid: */
  zone_t freeClust;		/* total number of free clusters */
  zone_t nextClust;		/* number of next free cluster */

  int depclust;			/* directory entries per cluster */
};

/* content of a buffer */
union fsdata_u {
    char b__data[SBLOCK_SIZE];		     /* ordinary user data */
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
