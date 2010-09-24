/* This file contains the procedures that deals with FATs.
 *
 *  The entry points into this file are
 *   bmap		map a file-relative offset into a block number
 *
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

#include <string.h>

#ifdef	COMPAT316
#include "compat.h"	/* MUST come before <minix/sysutil.h> */
#endif
#include <minix/sysutil.h>	/* panic */

/* Private functions:
 *   get_mask		?
 */
/* useful only with symlinks... dropped ATM
FORWARD _PROTOTYPE( int ltraverse, (struct inode *rip, char *suffix)	);
 */

/* warning: the following lines are not failsafe macros */
#define	get_le16(arr) ((u16_t)( (arr)[0] | ((arr)[1]<<8) ))
#define	get_le32(arr) ( get_le16(arr) | ((u32_t)get_le16((arr)+2)<<16) )

/*===========================================================================*
 *				xxx					     *
 *===========================================================================*/


/*
 *  Find the closest entry in the fat cache to the
 *  cluster we are looking for.
 */
fc_lookup(
	struct inode *rip,
	/*unsigned long*/ cluster_t findcn,
	/*unsigned long*/ cluster_t * frcnp,
	/*unsigned long*/ cluster_t * abscnp)
{
	int i;
	unsigned long frcn;
	struct fatcache *closest = 0;

	for (i = 0; i < FC_SIZE; i++) {
		frcn = rip->i_fc[i].fc_frcn;
		if (frcn != FCE_EMPTY  &&  frcn <= findcn) {
			if (closest == 0  ||  frcn > closest->fc_frcn)
				closest = &rip->i_fc[i];
		}
	}
	if (closest) {
		*frcnp  = closest->fc_frcn;
		*abscnp = closest->fc_abscn;
	}
}

/*===========================================================================*
 *				bmap					     *
 *===========================================================================*/
PUBLIC block_t bmap(struct inode *rip, unsigned long position)
{
/* Map a fileoffset into a physical block that is filesystem relative. */
  cluster_t findcn;		/* file relative cluster to get	*/
  block_t findbn;		/* file relative block to get */
  block_t boff;			/* offset of block within cluster */
  cluster_t i;			/* file-relative cluster iterator */
  cluster_t cn;			/* filesystem relative cluster number */
  cluster_t prevcn;
  long byteoffset;
  block_t bn;			/* filesystem relative block number */
  long bo;
  struct buf *bp0 = 0;
  block_t bp0_bn = NO_BLOCK;
	struct buf *bp1 = 0;
/*
	union fattwiddle x;
 */

  i = 0;
  cn = rip->i_clust;
  findbn = position >> bcc.bnshift;
  findcn = position >> sb.cnshift;
  boff = (position & sb.crelmask) >> bcc.bnshift;

DBGprintf(("FATfs: bmap in %lo, off %ld; init lookup for %ld+%ld at %ld\n",
	INODE_NR(rip), position, findcn, boff, cn));

  /* The "file" that makes up the FAT12/16 root directory is contiguous,
   * permanently allocated, of fixed size, and is not made up of clusters.
   */
  if (cn == CLUST_CONVROOT ) {
	assert(rip->i_flags & I_ROOTDIR);
	if (findbn > bcc.rootBlk) {
		return /*E2BIG*/ NO_BLOCK;
	}
	return bcc.rootBlk + findbn;
  }

/* Rummage around in the fat cache, maybe we can avoid tromping
 * thru every fat entry for the file.
 */
/*  fc_lookup(dep, findcn, &i, &cn); */

#if 0
/*
 *  Find the closest entry in the fat cache to the
 *  cluster we are looking for.
 */
fc_lookup(
	struct inode *rip,
	/*unsigned long*/ cluster_t findcn,
	/*unsigned long*/ cluster_t * frcnp,
	/*unsigned long*/ block_t * bnp)
{
	int i;
	unsigned long cn;
	struct fatcache *closest = 0;

	for (i = 0; i < FC_SIZE; i++) {
		cn = rip->i_fc[i].fc_frcn;
		if (cn != FCE_EMPTY  &&  cn <= findcn) {
			if (closest == 0  ||  cn > closest->fc_frcn)
				closest = &rip->i_fc[i];
		}
	}
	if (closest) {
		*frcnp  = closest->fc_frcn;
		*bnp = closest->fc_bn;
	}
}
#endif

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
			x.byte[0] = bp0->b_un.b_addr[bo];

			if (bo == pmp->pm_BytesPerSec-1) {
				if (error = bread(pmp->pm_devvp, bn+1,
				    pmp->pm_BytesPerSec, NOCRED, &bp1)) {
					brelse(bp0);
					return error;
				}
				x.byte[1] = bp1->b_un.b_addr[0];
				brelse(bp1);
			} else {
				x.byte[1] = bp0->b_un.b_addr[bo+1];
			}
			if (cn & 1)
				x.word >>= 4;
			prevcn = cn;
			cn = x.word & FAT12_MASK;
 */
			break;
		case FAT16_MASK:
/* FIXME: use get_le16 */
			cn = *(uint16_t *)(bp0->b_data+bo);
			break;
		case FAT32_MASK:
/* FIXME: use get_le32 */
			cn = *(uint32_t *)(bp0->b_data+bo);
			break;
		default:
			panic("unexpected value for sb.fatmask");
		}
/* WARNING: if we receive 0 (or 1) as next cluster... BOUM! */
	}

#if 1
	if (!ATEOF(cn, sb.eofmask)) {
		if (bp0)
			put_block(bp0);
/*
		fc_setcache(rip, FC_LASTMAP, i, cn);
 */
		return ((cn-CLUST_FIRST) << bcc.cbshift) + bcc.clustBlk;
	}

hiteof:
	if (bp0)
		put_block(bp0);
	/* update last file cluster entry in the fat cache */
/*
	fc_setcache(rip, FC_LASTFC, i-1, prevcn);
 */
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
