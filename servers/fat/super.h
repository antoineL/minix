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

#endif
