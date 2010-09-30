/* This file deals with FAT, file allocation tables.
 *
 *  The entry points into this file are
 *   init_fat_bitmap	initialize the use bitmap
 *   done_fat_bitmap	dispose of resources
 *   fc_purge		reset FAT cache when file size changes
 *   bmap		map a file-relative offset into a block number
 *   peek_bmap		map offset into block number without doing I/O
 *   clusteralloc	allocate one cluster
 *   clusterfree	return one cluster to allocation pool
 *   clusterfreechain	free a chain of clusters
 *   extendfile		extend a file up to a new size
 *
 * Auteur: Antoine Leca, septembre 2010.
 * The basis for this file is the PCFS package targetting 386BSD 0.1,
 * published in comp.unix.bsd on October 1992 by Paul Popelka;
 * his work is to be rewarded; see also the notice below.
 * Updated:
 */
/*
 * Written by Paul Popelka (paulp@uts.amdahl.com)
 *
 * You can do anything you want with this software,
 *   just don't say you wrote it,
 *   and don't remove this notice.
 *
 * This software is provided "as is".
 *
 * The author supplies this software to be publicly
 * redistributed on the understanding that the author
 * is not responsible for the correct functioning of
 * this software in any circumstances and is not liable
 * for any damages caused by this software.
 *
 * October 1992
 */

#include "inc.h"

#include <stdlib.h>
#include <string.h>

#ifdef	COMPAT316
#include "compat.h"	/* MUST come before <minix/sysutil.h> */
#endif
#include <minix/sysutil.h>	/* panic */

/* Private global variables: */
  /* ptr to bitmap of in-use clusters */ 
PRIVATE unsigned char * inuse_bitmap;

/* Private functions:
 *   get_mask		?
 */
FORWARD _PROTOTYPE( void fc_setcache, (struct inode *,
	 int slot, cluster_t frcn, cluster_t abscn)			);
FORWARD _PROTOTYPE( void fc_lookup, (struct inode *,
	  cluster_t findcn, cluster_t * frcnp, cluster_t * abscnp)	);
FORWARD _PROTOTYPE( int fatentry, (cluster_t cluster, 
	cluster_t *oldcontents, cluster_t newcontents)		);

#define C_TO_B(cn)	( (((cn)-CLUST_FIRST) << bcc.cbShift) + bcc.clustBlk )

/* warning: the following will not work if cluster_t/zone_t is signed */
#define IS_VALID_CLUST(cn)		\
	( (cluster_t)((cn) - CLUST_FIRST) > (sb.maxClust - CLUST_FIRST))

/* warning: the following lines are not failsafe macros */
#define	get_le16(arr) ((u16_t)( (arr)[0] | ((arr)[1]<<8) ))
#define	get_le32(arr) ( get_le16(arr) | ((u32_t)get_le16((arr)+2)<<16) )

/* FIXME: writing... */

/*===========================================================================*
 *				init_fat_bitmap				     *
 *===========================================================================*/
PUBLIC int init_fat_bitmap(void)
{
/* Read in fat blocks looking for free clusters.
 * For every free cluster found turn off its corresponding bit in the inuse_bitmap.
 */
  block_t cn;
  long byteoffset;
  block_t bn;			/* filesystem relative block number */
  int bo;
  struct buf *bp0 = 0;
  block_t bp0_bn = NO_BLOCK;
  cluster_t x;			/* read value */

/* FIXME: if FAT12, read the FAT into specials (contiguous) buffers
 * Do not forget to increase bufs_in_use...
 */

  /* Allocate memory for the bitmap of allocated clusters,
   * and then fill it in. */
  if (inuse_bitmap)
	free(inuse_bitmap);
  if (! read_only) {
	inuse_bitmap = malloc((sb.maxClust >> 3) + 1);
	if (inuse_bitmap == NULL) {
		return(ENOMEM);
	}
	/* Mark all clusters in use, we unmark the free ones in the
	 * fat scan loop further down.
	 */
	for (cn = 0; cn < (sb.maxClust >> 3) + 1; cn++)
		inuse_bitmap[cn] = 0xff;

	/* Figure how many free clusters are in the filesystem by ripping
	 * through the FAT, counting the number of entries whose content is
	 * zero. These represent free clusters.
	 */
	sb.freeClust = 0;
	sb.nextClust = -1;
	for (cn = CLUST_FIRST; cn <= sb.maxClust; cn++) {
		byteoffset = cn * sb.nibbles / 2;
		bn = (byteoffset >> bcc.bnShift) + bcc.fatBlk;
		bo = byteoffset & bcc.bMask;
		if (bn != bp0_bn) {
			if (bp0)
				put_block(bp0);
			bp0 = get_block(dev, bn, NORMAL);
			if (bp0 == NO_BLOCK) {
				DBGprintf(("FATfs: init_fat_bitmap unable "
					"to get FAT block %ld\n", (long)bn));
				return(EIO);
			}
			bp0_bn = bn;
		}
		switch (sb.fatMask) {
		case FAT12_MASK:
/* FIXME: give pointer to explanations  */
			x = get_le16(bp0->b_zfat16[bo]);
			if (cn & 1)
				x >>= 4;
			break;
		case FAT16_MASK:
			x = get_le16(bp0->b_zfat16[bo]);
			break;
		case FAT32_MASK:
			x = get_le32(bp0->b_zfat32[bo]);
			break;
		default:
			panic("unexpected value for sb.fatMask");
		}
		x &= sb.fatMask;
		if ( x == CLUST_FREE) {
			sb.freeClust++;
			inuse_bitmap[cn >> 3] &= ~(1 << (cn & 0x07));
			if (sb.nextClust < 0)
				sb.nextClust = cn;
		} else if ( (x > sb.maxClust && x < (sb.fatMask&CLUST_BAD))
		          || x < CLUST_FIRST ) {
			DBGprintf(("FATfs: init_fat_bitmap encounters "
				"invalid cluster number %ld\n", (long)x));
			/* FIXME: FS is not clean */
		}
	}
	put_block(bp0);

	DBGprintf(("FATfs: FAT bitmap is %u bytes\n", (sb.maxClust >> 3) + 1));
	DBGprintf(("FATfs: %ld free clusters, first is %ld\n",
		(long)sb.freeClust, (long)sb.nextClust));
  }
  return(OK);
}

/*===========================================================================*
 *				done_fat_bitmap				     *
 *===========================================================================*/
PUBLIC void done_fat_bitmap(void)
{
/* ... */
/* FIXME: if FAT12, deref the special blocks containing the FAT */

  if (inuse_bitmap)
	free(inuse_bitmap);
}

/*===========================================================================*
 *				fc_setcache				     *
 *===========================================================================*/
PRIVATE void fc_setcache(struct inode *rip, int slot,
  cluster_t frcn,
  cluster_t abscn)
{
/* Set a slot in the fat cache. */

  rip->i_fc[slot].fc_frcn = frcn;
  rip->i_fc[slot].fc_abscn = abscn;
}

/*===========================================================================*
 *				fc_lookup				     *
 *===========================================================================*/
PRIVATE void fc_lookup(struct inode *rip,
  cluster_t findcn,
  cluster_t * frcnp,
  cluster_t * abscnp)
{
/* Find the closest entry in the fat cache to the cluster we are looking for.
 */
  cluster_t frcn;
  struct fatcache * fcp, * closest = NULL;

  for (fcp = rip->i_fc; fcp < &rip->i_fc[FC_SIZE]; ++fcp) {
	frcn = fcp->fc_frcn;
	if (frcn != FCE_EMPTY  &&  frcn <= findcn) {
		if (closest == NULL  ||  frcn > closest->fc_frcn)
			closest = fcp;
	}
  }
  if (closest) {
	*frcnp  = closest->fc_frcn;
	*abscnp = closest->fc_abscn;
  }
}

/*===========================================================================*
 *				fc_purge				     *
 *===========================================================================*/
PUBLIC void fc_purge(struct inode *rip, cluster_t frcn)
{
/* Purge the FAT cache of all entries relating to file relative cluster frcn
 * and beyond.
 */
  struct fatcache * fcp;

  for (fcp = rip->i_fc; fcp < &rip->i_fc[FC_SIZE]; ++fcp) {
	if (fcp->fc_frcn != FCE_EMPTY  &&  fcp->fc_frcn >= frcn) {
		fcp->fc_frcn = FCE_EMPTY;
	}
  }
}

/*===========================================================================*
 *				bmap					     *
 *===========================================================================*/
PUBLIC block_t bmap(struct inode *rip, unsigned long position)
{
/* Map a fileoffset into a physical block that is filesystem relative.
 * This function uses and updates the FAT cache.
 */
  block_t findbn;		/* file relative block to get */
  block_t boff;			/* offset of block within cluster */
  cluster_t i;			/* file-relative cluster iterator */
  cluster_t findcn;		/* file relative cluster to get	*/
  cluster_t cn;			/* filesystem relative cluster number */
  cluster_t prevcn;
  long byteoffset;
  block_t bn;			/* filesystem relative block number */
  int bo;
  struct buf *bp0 = 0;
  block_t bp0_bn = NO_BLOCK;

  cn = rip->i_clust;

  /* The "file" that makes up the FAT12/16 root directory is contiguous,
   * permanently allocated, of fixed size, and is not made up of clusters.
   */
  if (cn == CLUST_CONVROOT) {
	assert(IS_ROOT(rip));
	findbn = position >> bcc.bnShift;
	if (findbn > bcc.rootSiz) {
		/* DBGprintf? */
		return NO_BLOCK;
	}
	return bcc.rootBlk + findbn;
  }

  i = 0;
  findcn = position >> sb.cnShift;
  boff = (position & sb.cMask) >> bcc.bnShift;

DBGprintf(("FATfs: bmap in %lo, off %ld; init lookup for FR%ld+%ld, start at %ld",
	INODE_NR(rip), position, findcn, boff, cn));

  /* Rummage around in the fat cache, maybe we can avoid tromping
   * thru every fat entry for the file.
   */
  fc_lookup(rip, findcn, &i, &cn);

DBGprintf((", up to FR%d at %ld\n", i, cn));

  /* Handle all other files or directories the normal way. */

  for (; i < findcn; i++) {
	if (ATEOF(cn, sb.eofMask)) {
#if 1
		goto hiteof;
#else
		break;
#endif
	}
	if (cn < CLUST_FIRST || cn > sb.maxClust) {
		DBGprintf(("FATfs: bmap encounters "
			"invalid cluster number %ld\n", (long)cn));
		if (bp0)
			put_block(bp0);
		/* FIXME: FS is not clean */
		return(NO_BLOCK);
	}
	/* Note this cannot overflow, even with FAT32:
	 * cn is less than 1<<28, and nibbles is 8
	 */
	byteoffset = cn * sb.nibbles / 2;
	bn = (byteoffset >> bcc.bnShift) + bcc.fatBlk;
	bo = byteoffset & bcc.bMask;
	if (bn != bp0_bn) {
		if (bp0)
			put_block(bp0);
DBGprintf(("\n!Need blk %ld; ", bn));
		bp0 = get_block(dev, bn, NORMAL);
DBGprintf(("done bp0=%.8p bp0->dp=%.8p\n", bp0, bp0->dp));

		if (bp0 == NULL) {
			DBGprintf(("FATfs: bmap unable to get "
				"FAT block %ld\n", (long)bn));
			return(NO_BLOCK);
		}
		bp0_bn = bn;
	}
	prevcn = cn;
	switch (sb.fatMask) {
	case FAT12_MASK:
/*  If the first byte of the fat entry was the last byte
 *  in the block, then we must read the next block in the
 *  fat. 

WAS
 We hang on to the first block we read to insure
 *  that some other process doesn't update it while we
 *  are waiting for the 2nd block.
 *  Note that we free bp1 even though the next iteration of
 *  the loop might need it.
NOW IS !=
FIXME: explain
 */
		cn = get_le16(bp0->b_zfat16[bo]);
		if (prevcn & 1)
			cn >>= 4;
		break;
	case FAT16_MASK:
DBGprintf(("exam %.8p[bo=%x]=+%x => @%.8p [0]@%.8p=%.2X [1]@%.8p=%.2X ", bp0->dp, bo, bo*2, &bp0->b_zfat16[bo],
	&bp0->b_zfat16[bo][0], bp0->b_zfat16[bo][0], &bp0->b_zfat16[bo][1], bp0->b_zfat16[bo][1], get_le16(bp0->b_zfat16[bo])));

		cn = get_le16(bp0->b_zfat16[bo]);
		break;
	case FAT32_MASK:
		cn = get_le32(bp0->b_zfat32[bo]);
		break;
	default:
		panic("unexpected value for sb.fatMask");
	}
	cn &= sb.fatMask;
  }

#if 1
	if (!ATEOF(cn, sb.eofMask)) {
		if (bp0)
			put_block(bp0);
		if (cn < CLUST_FIRST || cn > sb.maxClust) {
			DBGprintf(("FATfs: bmap finds "
				"invalid cluster number %ld\n", (long)cn));
			/* FIXME: FS is not clean */
			return(NO_BLOCK);
		}
		fc_setcache(rip, FC_LASTMAP, i, cn);
DBGprintf((" => cn=%ld bn=%ld\n", cn, C_TO_B(cn) + boff));
		return C_TO_B(cn) + boff;
	}

hiteof:
	if (bp0)
		put_block(bp0);
	/* update last file cluster entry in the fat cache */
	fc_setcache(rip, FC_LASTFC, i-1, prevcn);
	return /*E2BIG*/ NO_BLOCK;
#else
hiteof:
	if (bp0)
		put_block(bp0);
	/* update last file cluster entry in the fat cache */
/*
FIXME: beware LASTMAP/LASTFC, i/i-1, cn/prevcn...
		fc_setcache(rip, FC_LASTMAP, i, cn);
	fc_setcache(rip, FC_LASTFC, i-1, prevcn);
 */
	return ATEOF(cn, sb.eofMask) ? /*E2BIG*/ NO_BLOCK
	       : C_TO_B(cn) + boff;
#endif
}

/*===========================================================================*
 *				peek_bmap				     *
 *===========================================================================*/
PUBLIC block_t peek_bmap(struct inode *rip,
  unsigned long position)
{
/* Map a fileoffset into a physical block that is filesystem relative.
 * This function uses and updates the FAT cache.
 * Also this function does not try to do any I/O operation.
 */
  block_t findbn;		/* file relative block to get */
  block_t boff;			/* offset of block within cluster */
  cluster_t i;			/* file-relative cluster iterator */
  cluster_t findcn;		/* file relative cluster to get	*/
  cluster_t cn;			/* filesystem relative cluster number */
  long byteoffset;
  block_t bn;			/* filesystem relative block number */
  int bo;
  struct buf *bp0 = 0;
  block_t bp0_bn = NO_BLOCK;

  cn = rip->i_clust;

DBGprintf(("FATfs: peek_bmap in %lo, off %ld; init lookup for %ld+%ld at %ld\n",
	INODE_NR(rip), position, findcn, boff, cn));

  /* The "file" that makes up the FAT12/16 root directory is contiguous,
   * permanently allocated, of fixed size, and is not made up of clusters.
   */
  if (cn == CLUST_CONVROOT) {
	assert(IS_ROOT(rip));
	findbn = position >> bcc.bnShift;
	if (findbn > bcc.rootSiz) {
		/* DBGprintf? */
		return NO_BLOCK;
	}
	return bcc.rootBlk + findbn;
  }

  i = 0;
  findcn = position >> sb.cnShift;
  boff = (position & sb.cMask) >> bcc.bnShift;

  /* Rummage around in the fat cache, maybe we can avoid tromping
   * thru every fat entry for the file.
   */
  fc_lookup(rip, findcn, &i, &cn);

  /* Handle all other files or directories the normal way. */

  for (; i < findcn; i++) {
/* WARNING: if we receive 0 (or 1) as next cluster... BOUM! */
	if (ATEOF(cn, sb.eofMask)) {
		if (bp0)
			put_block(bp0);
		/* do NOT update the FC_LASTFC entry of the cache */
		/* CHECKME: really? if we do, carry back prevcn... */
		return NO_BLOCK;
	}
	if (cn < CLUST_FIRST || cn > sb.maxClust) {
		DBGprintf(("FATfs: peek_bmap encounters "
			"invalid cluster number %ld\n", (long)cn));
		if (bp0)
			put_block(bp0);
		/* FIXME: FS is not clean */
		return(NO_BLOCK);
	}
	byteoffset = cn * sb.nibbles / 2;
	bn = (byteoffset >> bcc.bnShift) + bcc.fatBlk;
	bo = byteoffset & bcc.bMask;
	if (bn != bp0_bn) {
		if (bp0)
			put_block(bp0);
		bp0 = get_block(dev, bn, PREFETCH);
		if (bp0 == NULL || bp0->b_dev != dev)
			/* do NOT update the cache */
			return(NO_BLOCK);
		bp0_bn = bn;
	}
	switch (sb.fatMask) {
	case FAT12_MASK:
		if (cn & 1)
			cn = get_le16(bp0->b_zfat16[bo]) >> 4;
		else
			cn = get_le16(bp0->b_zfat16[bo]);
		break;
	case FAT16_MASK:
		cn = get_le16(bp0->b_zfat16[bo]);
		break;
	case FAT32_MASK:
		cn = get_le32(bp0->b_zfat32[bo]);
		break;
	default:
		panic("unexpected value for sb.fatMask");
	}
	cn &= sb.fatMask;
  }

  if (bp0)
	put_block(bp0);
  if (cn < CLUST_FIRST || cn > sb.maxClust) {
	DBGprintf(("FATfs: peek_bmap finds "
			"invalid cluster number %ld\n", (long)cn));
	/* FIXME: FS is not clean */
	return(NO_BLOCK);
  }
  /* this result is correct, we can update the cache */
  fc_setcache(rip, FC_LASTMAP, i, cn);
  return C_TO_B(cn) + boff;
}

/*===========================================================================*
 *				lastcluster				     *
 *===========================================================================*/
PUBLIC cluster_t lastcluster(struct inode *rip, cluster_t *frcnp)
{
/* ... */
  block_t b;

  if (rip->i_clust == CLUST_NONE || rip->i_clust == CLUST_CONVROOT)
	/* if the file is empty, there is no last cluster...
	 * likewise, if we were asked about the fixed root directory,
	 *  there is no real last cluster, but we fake one
	 */
	return rip->i_clust;

  if (rip->i_fc[FC_LASTFC].fc_frcn == FCE_EMPTY) {
	/* If the "file last cluster" cache entry is empty,
	 * then fill the cache entry by calling bmap().
	 */
/*
	fc_lfcempty++;
*/
	b = bmap(rip, FAT_FILESIZE_MAX);
	/* we expect it to return NO_BLOCK */
	if (b != NO_BLOCK)
		return CLUST_NONE;
  }
  assert(rip->i_fc[FC_LASTFC].fc_frcn != FCE_EMPTY);

  /* Give the file-relative cluster number if they want it. */
  if (frcnp)
	*frcnp = rip->i_fc[FC_LASTFC].fc_frcn;

  /* Return the absolute (file system relative) cluster number. */
  assert(rip->i_fc[FC_LASTFC].fc_abscn >= CLUST_FIRST);
  return rip->i_fc[FC_LASTFC].fc_abscn;
}

/*===========================================================================*
 *				fatentry				     *
 *===========================================================================*/
/* Set the cluster'th entry in the FAT.
 *  cluster - which cluster is of interest
 *  oldcontents - address of a word that is to receive
 *    the contents of the cluster'th entry
 *  newcontents - the new value to be written into the
 *    cluster'th element of the fat
 *
 *  This function can also be used to free a cluster
 *  by setting the fat entry for a cluster to 0.
 *
FIXME...
 *  All copies of the fat are updated
 *  NOTE:
 *    If fatentry() marks a cluster as free it does not
 *    update the inusemap in the pcfsmount structure.
 *    This is left to the caller.
 */
PRIVATE int fatentry(cluster_t cluster,
  cluster_t *oldcontents,
  cluster_t newcontents)
{
  long byteoffset;
  block_t bn;			/* filesystem relative block number */
  int bo;
  struct buf *bp0 = 0;
  unsigned x;			/* old value with surrounding for FAT12 */

/*printf("fatentry(func %d, pmp %08x, clust %d, oldcon %08x, newcon %d)\n",
	function, pmp, cluster, oldcontents, newcontents);*/

  /* Be sure the requested cluster is in the filesystem. */
  if (cluster < CLUST_FIRST || cluster > sb.maxClust)
	return EINVAL;
  /* Be sure we are not shooting ourselves in the foot. */
  if ( (newcontents > sb.maxClust && newcontents < (sb.fatMask&CLUST_BAD))
    ||  newcontents == CLUST_FIRST )
	return EINVAL;

  byteoffset = cluster * sb.nibbles / 2;
  bn = (byteoffset >> bcc.bnShift) + bcc.fatBlk;
  bo = byteoffset & bcc.bMask;
  bp0 = get_block(dev, bn, NORMAL);
  if ( bp0 == NULL || !IS_VALID_BLOCK(bp0) ) {
	DBGprintf(("FATfs: fatentry unable to get "
				"FAT for cluster %ld\n", (long)cluster));
	return(EIO);
  }
  switch (sb.fatMask) {
  case FAT12_MASK:
	/* If the first byte of the fat entry was the last byte in the block,
	 * then we must also access the next block in the FAT.
WAS
 We hang on to the first block we read to insure
 *  that some other process doesn't update it while we
 *  are waiting for the 2nd block.
 *  Note that we free bp1 even though the next iteration of
 *  the loop might need it.
NOW IS !=
FIXME: explain
	 */
	/* Updating entries in 12 bit fats is a pain in the butt.
	 *
	 * The following picture shows where nibbles go when moving from
	 * a 12 bit cluster number into the appropriate bytes in the FAT.
	 *
	 *      byte m        byte m+1      byte m+2
	 *    +----+----+   +----+----+   +----+----+
	 *    |  0    1 |   |  2    3 |   |  4    5 |   FAT bytes
	 *    +----+----+   +----+----+   +----+----+
	 *
	 *       +----+----+----+ +----+----+----+
	 *       |  3    0    1 | |  4    5    2 |
	 *       +----+----+----+ +----+----+----+
	 *         cluster n        cluster n+1
	 *
	 *    Where n is even.
	 *    m = n + (n >> 2) = n * 3 / 2
	 */
	x = get_le16(bp0->b_zfat16[bo]);
	if (cluster & 1) { /* oddcluster */
		/* x is m+1:m+2 or |2 3|4 5| or 0x4523, according to scheme above */
		if (oldcontents)
			*oldcontents = x >> 4;
		bp0->b_zfat16[bo][0] = (x & 0xF) | ( (newcontents << 4) & 0xF0);
		bp0->b_zfat16[bo][1] = (newcontents >> 4) & 0xFF;
	} else {
		/* x is m:m+1 or |0 1|2 3| or 0x2301 according to scheme above */
		if (oldcontents)
			*oldcontents = x & FAT12_MASK;
		bp0->b_zfat16[bo][0] =  newcontents & 0xFF;
/* FIXME: "& 0xF" is unnecessary */
		bp0->b_zfat16[bo][1] = ((newcontents >> 8) & 0xF) | (x & 0xF0);
	}
	break;
  case FAT16_MASK:
	if (oldcontents)
		*oldcontents = get_le16(bp0->b_zfat16[bo]);
	bp0->b_zfat16[bo][0] =  newcontents & 0xFF;
	bp0->b_zfat16[bo][1] = (newcontents << 8) & 0xFF;
	break;
  case FAT32_MASK:
	if (oldcontents)
		*oldcontents = get_le32(bp0->b_zfat32[bo]);
	bp0->b_zfat32[bo][0] =  newcontents & 0xFF;
	bp0->b_zfat32[bo][1] = (newcontents <<  8) & 0xFF;
	bp0->b_zfat32[bo][2] = (newcontents << 16) & 0xFF;
/* FIXME: "& 0xF" is unnecessary */
	bp0->b_zfat32[bo][3] =((newcontents << 24) & 0xF)
		| (bp0->b_zfat32[bo][3] & 0xF0); /* keep higher four bits */
	break;
  default:
	panic("unexpected value for sb.fatMask");
  }
  bp0->b_dirt = DIRTY;
  put_block(bp0);
  return(OK);
}

/*===========================================================================*
 *				clusteralloc				     *
 *===========================================================================*/
PUBLIC int clusteralloc(cluster_t *retcluster, cluster_t fillwith)
{
/* Allocate a free cluster.
 * retcluster - put the allocated cluster's number here.
 * fillwith - put this value into the fat entry for the
 *     allocated cluster.
 */
  int r;
  cluster_t cn;
  cluster_t end_cn;

/* FIXME: use nextClust;
 * use a separate function pour for() loop;
 * 1er tir: from nextClust; si chou blanc, 2e tir from 0
 */

  /* This for loop really needs to start from 0. */
  for (cn = 0; cn <= sb.maxClust; cn += 8) {
	if (inuse_bitmap[cn >> 3] != 0xff) {
		end_cn = cn | 0x07;
		for (; cn <= end_cn; cn++) {
			if ((inuse_bitmap[cn >> 3] & (1 << (cn & 0x07))) == 0)
				goto found_one;
		}
		printf("clusteralloc(): this shouldn't happen\n");
	}
  }
  return ENOSPC;

found_one:
  r = fatentry(cn, NULL, fillwith);
  if (r == OK) {
	inuse_bitmap[cn >> 3] |= 1 << (cn & 0x07);
	sb.freeClust--;
/*
	pmp->pm_fmod++;
*/
	*retcluster = cn;
  }
  DBGprintf(("clusteralloc(): allocated cluster %d\n", cn));
  return r;
}

/*===========================================================================*
 *				clusterfree				     *
 *===========================================================================*/
PUBLIC int clusterfree(cluster_t cluster)
{
/* ... */
  int r;

  r = fatentry(cluster, NULL, CLUST_FREE);
  if (r == OK) {
  /* If the cluster was successfully marked free, then update the count of
   * free clusters, and turn off the "allocated" bit in the
   * "in use" cluster bit map.
   */
	sb.freeClust++;
	inuse_bitmap[cluster >> 3] &= ~(1 << (cluster & 0x07));
/* FIXME: update sb.nextClust */
  }
  return r;
}

/*===========================================================================*
 *				clusterfreechain			     *
 *===========================================================================*/
/*
PUBLIC int freeclusterchain(cluster_t startcluster)
 */
PUBLIC int clusterfreechain(cluster_t startcluster)
{
/* Free a chain of clusters.
 * startcluster - number of the 1st cluster in the chain
 *    of clusters to be freed.
 */
  cluster_t nextcluster;
  int r = 0;

  while (startcluster >= CLUST_FIRST  &&  startcluster <= sb.maxClust) {
	r = fatentry(startcluster, &nextcluster, CLUST_FREE );
	if (r) {
		printf("freeclusterchain(): free failed, cluster %d\n",
			startcluster);
		break;
	}
  /* If the cluster was successfully marked free, then update the count of
   * free clusters, and turn off the "allocated" bit in the
   * "in use" cluster bit map.
   */
	sb.freeClust++;
	inuse_bitmap[startcluster >> 3] &= ~(1 << (startcluster & 0x07));
/* FIXME: update sb.nextClust */
	startcluster = nextcluster;
/*
	pmp->pm_fmod++;
 */
  }
  return r;
}

/*===========================================================================*
 *				extendfile				     *
 *===========================================================================*/
PUBLIC int extendfile(
  struct inode *rip,		/* inode of the file or directory to extend */
  struct buf **bpp,		/* where to return the buf for 1st new block*/
  cluster_t *newcnp)		/* where to put newly allocated cluster numb*/
{
/* Allocate a new cluster and chain it at the end of the file.
 * May returns the buf for the first block of the newly allocated cluster,
 * and may also return the cluster number.
 *
 * Note: This function is not responsible for updating the i_filesize value
 * of the inode. This is left for the caller to do.
 */
  int r;
  cluster_t frcn;
  cluster_t cn;
  block_t bnx;

  /* Do not try to extend the fixed root directory */
  if (rip->i_clust == CLUST_CONVROOT) {
	DBGprintf(("extendfile(): attempt to extend root directory\n"));
	return(ENOSPC);
  }

  /* Allocate another cluster. */
  if ( (r = clusteralloc(&cn, CLUST_EOFE)) != OK)
	return r;

  if (bpp) {
  /* Get the buf header for the (first) new block of the file
   * if they want it.
   */
	*bpp = get_block(dev, C_TO_B(cn), NO_READ);
	if (*bpp == NULL) {
		/* DBGprintf */
		clusterfree(cn);
		return(EIO);
	}
	zero_block(*bpp);
  }

  /* Chain onto the end of the file.
   * If the file is (was) empty we make i_clust point to the new cluster.
   */
  if (rip->i_clust == 0) {
	rip->i_clust = cn;
	update_startclust(rip, cn);
	frcn = 0;
  } else {
	r = fatentry(lastcluster(rip, &frcn), NULL, cn);
	if (r != OK) {
		if (bpp) put_block(*bpp);
		clusterfree(cn);
		return r;
	}
	++frcn;
  }

  /* Update the "last cluster of the file" entry in the fat cache. */
  fc_setcache(rip, FC_LASTFC, frcn, cn);

  /* Give them the filesystem relative cluster number if they want it. */
  if (newcnp)
	*newcnp = cn;
  return(OK);
}

/*===========================================================================*
 *				extendfileclear				     *
 *===========================================================================*/
PUBLIC int extendfileclear(
  struct inode *rip,		/* inode of the file or directory to extend */
  unsigned long position,	/* ... */
  struct buf **bpp,		/* where to return the buf for 1st new block*/
  cluster_t *cnp)		/* where to put newly allocated cluster numb*/
{
}
