/* This file contains the procedures that deals with FATs.
 *
 *  The entry points into this file are
 *   init_fat_bitmap
 *   done_fat_bitmap
 *   fc_purge
 *   bmap		map a file-relative offset into a block number
 *   peek_bmap		map a file-relative offset into a block number
 *			without doing I/O
 *   clusteralloc
 *   clusterfree
 *   freeclusterchain
 *   extendfile
 *   + extendfile_many
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
	 int slot, cluster_t frcn, cluster_t abscn, block_t bn)		);
FORWARD _PROTOTYPE( void fc_lookup, (struct inode *,
	  cluster_t findcn, cluster_t * frcnp, cluster_t * abscnp)	);
FORWARD _PROTOTYPE( int fatentry, (cluster_t cluster, 
	cluster_t *oldcontents, cluster_t newcontents)		);

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
		bn = (byteoffset >> bcc.bnshift) + bcc.fatBlk;
		bo = byteoffset & bcc.brelmask;
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
		switch (sb.fatmask) {
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
			panic("unexpected value for sb.fatmask");
		}
		if ( (x & sb.fatmask) == 0) {
			sb.freeClust++;
			inuse_bitmap[cn >> 3] &= ~(1 << (cn & 0x07));
			if (sb.nextClust < 0)
				sb.nextClust = cn;
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
  cluster_t abscn,
  block_t bn)
{
/* Set a slot in the fat cache. */

  rip->i_fc[slot].fc_frcn = frcn;
  rip->i_fc[slot].fc_abscn = abscn;
  rip->i_fc[slot].fc_bn = bn;
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

DBGprintf(("FATfs: bmap in %lo, off %ld; init lookup for %ld+%ld at %ld\n",
	INODE_NR(rip), position, findcn, boff, cn));

  /* The "file" that makes up the FAT12/16 root directory is contiguous,
   * permanently allocated, of fixed size, and is not made up of clusters.
   */
  if (cn == CLUST_CONVROOT) {
	assert(IS_ROOT(rip));
	findbn = position >> bcc.bnshift;
	if (findbn > bcc.rootSiz) {
		/* DBGprintf? */
		return NO_BLOCK;
	}
	return bcc.rootBlk + findbn;
  }

  i = 0;
  findcn = position >> sb.cnshift;
  boff = (position & sb.crelmask) >> bcc.bnshift;

  /* Rummage around in the fat cache, maybe we can avoid tromping
   * thru every fat entry for the file.
   */
  fc_lookup(rip, findcn, &i, &cn);

  /* Handle all other files or directories the normal way. */

  for (; i < findcn; i++) {
/* WARNING: if we receive 0 (or 1) as next cluster... BOUM! */
	if (ATEOF(cn, sb.eofmask)) {
#if 1
		goto hiteof;
#else
		break;
#endif
	}
	/* Note this cannot overflow, even with FAT32:
	 * cn is less than 1<<28, and nibbles is 8
	 */
	byteoffset = cn * sb.nibbles / 2;
	bn = (byteoffset >> bcc.bnshift) + bcc.fatBlk;
	bo = byteoffset & bcc.brelmask;
	if (bn != bp0_bn) {
		if (bp0)
			put_block(bp0);
		bp0 = get_block(dev, bn, NORMAL);
		if (bp0 == NO_BLOCK) {
			DBGprintf(("FATfs: bmap unable to get "
				"FAT block %ld\n", (long)bn));
			return(NO_BLOCK);
		}
		bp0_bn = bn;
	}
	prevcn = cn;
#if 0
		switch (sb.fatmask) {
		case FAT12_MASK:
/*  If the first byte of the fat entry was the last byte
 *  in the block, then we must read the next block in the
 *  fat.  We hang on to the first block we read to insure
 *  that some other process doesn't update it while we
 *  are waiting for the 2nd block.
 *  Note that we free bp1 even though the next iteration of
 *  the loop might need it.
 */
/*
			x.byte[0] = bp0->b_data[bo];

			if (bo == bcc.bpblock-1) {
				if (error = bread(pmp->pm_devvp, bn+1,
				    bcc.bpblock, NOCRED, &bp1)) {
					put_block(bp0);
					return error;
				}
				x.byte[1] = bp1->b_data[0];
				put_block(bp1);
			} else {
				x.byte[1] = bp0->b_data[bo+1];
			}
			if (cn & 1)
				x.word >>= 4;
 */
		break;
	case FAT16_MASK:
		cn = get_le16(bp0->b_zfat16[bo]);
		break;
	case FAT32_MASK:
#if 0
		cn = *(uint32_t *)(bp0->b_data+bo);
#else
		cn = get_le32(bp0->b_zfat32[bo]);
#endif
		break;
	default:
		panic("unexpected value for sb.fatmask");
	}
	cn &= sb.fatmask;
#elif 1
	switch (sb.fatmask) {
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
		cn = get_le16(bp0->b_zfat16[bo]);
		break;
	case FAT32_MASK:
		cn = get_le32(bp0->b_zfat32[bo]);
		break;
	default:
		panic("unexpected value for sb.fatmask");
	}
	cn &= sb.fatmask;
#else
	cn = get_le16(bp0->b_zfat16[bo]);
	switch (sb.fatmask) {
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
		if (prevcn & 1)
			cn >>= 4;
		break;
	case FAT16_MASK:
		/* work is done */
		break;
	case FAT32_MASK:
#if 0
		cn = get_le32(bp0->b_zfat32[bo]);
#else
		cn |= get_le16(bp0->b_zfat16[bo+1])<<16;
#endif
		break;
	default:
		panic("unexpected value for sb.fatmask");
	}
	cn &= sb.fatmask;
#endif

/* WARNING: if we receive 0 (or 1) as next cluster... BOUM! */
  }

#if 1
	if (!ATEOF(cn, sb.eofmask)) {
		if (bp0)
			put_block(bp0);
		fc_setcache(rip, FC_LASTMAP, i, cn, bn);
		return ((cn-CLUST_FIRST) << bcc.cbshift) + bcc.clustBlk + boff;
	}

hiteof:
	if (bp0)
		put_block(bp0);
	/* update last file cluster entry in the fat cache */
	fc_setcache(rip, FC_LASTFC, i-1, prevcn, 0);
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
	return ATEOF(cn, sb.eofmask) ? /*E2BIG*/ NO_BLOCK
	       : ((cn-CLUST_FIRST) << bcc.cbshift) + bcc.clustBlk;
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
	findbn = position >> bcc.bnshift;
	if (findbn > bcc.rootSiz) {
		/* DBGprintf? */
		return NO_BLOCK;
	}
	return bcc.rootBlk + findbn;
  }

  i = 0;
  findcn = position >> sb.cnshift;
  boff = (position & sb.crelmask) >> bcc.bnshift;

  /* Rummage around in the fat cache, maybe we can avoid tromping
   * thru every fat entry for the file.
   */
  fc_lookup(rip, findcn, &i, &cn);

  /* Handle all other files or directories the normal way. */

  for (; i < findcn; i++) {
/* WARNING: if we receive 0 (or 1) as next cluster... BOUM! */
	if (ATEOF(cn, sb.eofmask)) {
		if (bp0)
			put_block(bp0);
		/* do NOT update the FC_LASTFC entry of the cache */
		/* CHECKME: really? if we do, carry back prevcn... */
		return NO_BLOCK;
	}
	byteoffset = cn * sb.nibbles / 2;
	bn = (byteoffset >> bcc.bnshift) + bcc.fatBlk;
	bo = byteoffset & bcc.brelmask;
	if (bn != bp0_bn) {
		if (bp0)
			put_block(bp0);
		bp0 = get_block(dev, bn, PREFETCH);
		if (bp0 == NO_BLOCK || bp0->b_dev == NO_DEV)
			/* do NOT update the cache */
			return(NO_BLOCK);
		bp0_bn = bn;
	}
	switch (sb.fatmask) {
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
		panic("unexpected value for sb.fatmask");
	}
	cn &= sb.fatmask;

/* WARNING: if we receive 0 (or 1) as next cluster... BOUM! */
  }

  if (bp0)
	put_block(bp0);
  /* this result is correct, we can update the cache */
  fc_setcache(rip, FC_LASTMAP, i, cn, bn);
  return ((cn-CLUST_FIRST) << bcc.cbshift) + bcc.clustBlk + boff;
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

  byteoffset = cluster * sb.nibbles / 2;
  bn = (byteoffset >> bcc.bnshift) + bcc.fatBlk;
  bo = byteoffset & bcc.brelmask;
  bp0 = get_block(dev, bn, NORMAL);
  if (bp0 == NO_BLOCK) {
	DBGprintf(("FATfs: fatentry unable to get "
				"FAT for cluster %ld\n", (long)cluster));
	return(EIO);
  }
  switch (sb.fatmask) {
  case FAT12_MASK:
	/* If the first byte of the fat entry was the last byte in the block,
	 * then we must read the next block in the FAT. 
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
	panic("unexpected value for sb.fatmask");
  }
  if (bp0)
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
 *				freeclusterchain			     *
 *===========================================================================*/
PUBLIC int freeclusterchain(cluster_t startcluster)
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
PUBLIC int extendfile(struct inode *rip,
  struct buf **bpp,
  cluster_t *cnp)
{
/* Allocate a new cluster and chain it onto the end of the file.
 *  rip - inode of the file or directory to extend
 *  bpp - where to return the address of the buf header for the
 *        new file block
 *  ncp - where to put cluster number of the newly allocated file block
 *        If this pointer is 0, do not return the cluster number.
 *
 *  NOTE:
 *   This function is not responsible for turning on the DEUPD
 *   bit if the de_flag field of the denode and it does not
 *   change the de_FileSize field.  This is left for the caller
 *   to do.
 */
	int error = 0;
  cluster_t frcn;
  cluster_t cn;
  block_t bn;

#if 0 /* MFS interface(s) */
/*===========================================================================*
 *				clear_zone				     *
 *===========================================================================*/
PUBLIC void clear_zone(rip, pos, flag)
register struct inode *rip;	/* inode to clear */
off_t pos;			/* points to block to clear */
int flag;			/* 1 if called by new_block, 0 otherwise */
{
/* Zero a zone, possibly starting in the middle.  The parameter 'pos' gives
 * a byte in the first block to be zeroed.  Clearzone() is called from 
 * fs_readwrite(), truncate_inode(), and new_block().
 */

/*===========================================================================*
 *				new_block				     *
 *===========================================================================*/
PUBLIC struct buf *new_block(rip, position)
register struct inode *rip;	/* pointer to inode */
off_t position;			/* file pointer */
{
/* Acquire a new block and return a pointer to it.  Doing so may require
 * allocating a complete zone, and then returning the initial block.
 * On the other hand, the current zone may still have some unused blocks.
 */
#endif

  /* Do not try to extend the fixed root directory */
  if (rip->i_clust == CLUST_CONVROOT) {
	DBGprintf(("extendfile(): attempt to extend root directory\n"));
	return ENOSPC;
  }

/*
	fc_fileextends++;
 */
#if 0 /* using lastcluster() below instead */
/* If the "file last cluster" cache entry is empty,
 * and the file is not empty,
 * then fill the cache entry by calling bmap().
 */
	if (rip->i_fc[FC_LASTFC].fc_frcn == FCE_EMPTY  &&
	    rip->i_clust != CLUST_NONE) {
/*
		fc_lfcempty++;
		error = pcbmap(dep, 0xffff, 0, &cn);
 */
		/* we expect it to return E2BIG */
		if (error != E2BIG)
			return error;
		error = 0;
	}
#endif

/*
 *  Allocate another cluster and chain onto the end of the file.
 *  If the file is empty we make de_StartCluster point to the
 *  new block.  Note that de_StartCluster being 0 is sufficient
 *  to be sure the file is empty since we exclude attempts to
 *  extend the root directory above, and the root dir is the
 *  only file with a startcluster of 0 that has blocks allocated
 *  (sort of).
 */
  if (error = clusteralloc(&cn, CLUST_EOFE))
	return error;
  if (rip->i_clust == 0) {
	rip->i_clust = cn;
	frcn = 0;
  } else {
	error = fatentry(lastcluster(rip, &frcn), NULL, cn);
	if (error) {
		clusterfree(/*pmp,*/ cn);
		return error;
	}
	++frcn;
  }

/*
 *  Get the buf header for the (first) new block of the file.
 */
  assert(bpp);
  *bpp = get_block(dev, ((cn-CLUST_FIRST) << bcc.cbshift) + bcc.clustBlk, NO_READ);
  if (*bpp == NULL) {
	/* DBGprintf */
	/* undo alloc, undo fatentry */
	return(EIO);
  }
  zero_block(*bpp);
  /* FIXME: should we zero the rest of the cluster? */
/*
 	if (rip->i_Attributes & ATTR_DIRECTORY) {
 		*bpp = getblk(pmp->pm_devvp, cntobn(pmp, cn),
			pmp->pm_bpcluster);
	} else {
		*bpp = getblk(DETOV(dep), frcn,
			pmp->pm_bpcluster);
	}
	clrbuf(*bpp);
*/
/*
 *  Update the "last cluster of the file" entry in the denode's
 *  fat cache.
 */
  fc_setcache(rip, FC_LASTFC, frcn, cn, /****/0 );

/*
 *  Give them the filesystem relative cluster number
 *  if they want it.
 */
  if (cnp)
	*cnp = cn;
  return(OK);
}
