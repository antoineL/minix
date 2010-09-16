/* This file contains the procedures that deals with FATs.
 *
 *  The entry points into this file are
 *   bmap		map a file-relative offset into a block number
 *
 *
 * Auteur: Antoine Leca, septembre 2010.
 * The basis for this file is the PCFS package targetting
 * 386BSD 0.1, published in comp.unix.bsd on October 1992
 * by Paul Popelka; his work is to be rewarded.
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

#if 0
#include "fat.h"
#include "super.h"
#include "inode.h"
#endif

/* warning: the following lines are not failsafe macros */
#define	get_le16(arr) ((u16_t)( (arr)[0] | ((arr)[1]<<8) ))
#define	get_le32(arr) ( get_le16(arr) | ((u32_t)get_le16((arr)+2)<<16) )

_PROTOTYPE( block_t bmap, (struct inode *rip, off_t position)		);

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

/*===========================================================================*
 *				bmap					     *
 *===========================================================================*/
PUBLIC block_t bmap(struct inode *rip, off_t position)
{
/*
 *  Map the logical cluster number of a file into
 *  a physical disk sector that is filesystem relative.
 *  dep - address of denode representing the file of interest
 *  findcn - file relative cluster whose filesystem relative
 *    cluster number and/or block number are/is to be found
 *  bnp - address of where to place the file system relative
 *    block number.  If this pointer is null then don't return
 *    this quantity.
 *  cnp - address of where to place the file system relative
 *    cluster number.  If this pointer is null then don't return
 *    this quantity.
 *  NOTE:
 *    Either bnp or cnp must be non-null.
 *    This function has one side effect.  If the requested
 *    file relative cluster is beyond the end of file, then
 *    the actual number of clusters in the file is returned
 *    in *cnp.  This is useful for determining how long a
 *    directory is.  If cnp is null, nothing is returned.
 */
#if 0
int
pcbmap(dep, findcn, bnp, cnp)
	struct denode *dep;
	unsigned long findcn;	/* file relative cluster to get		*/
	daddr_t *bnp=0;		/* returned filesys relative blk number	*/
	unsigned long *cnp=0;	/* returned cluster number		*/
#endif

	int error;
	cluster_t findcn;	/* file relative cluster to get	*/
	block_t findbn;		/* file relative block to get */
	block_t boff;		/* offset of block within cluster */
/*FIXME*/typedef unsigned long daddr_t;
	daddr_t *bnp;		/* returned filesys relative blk number	*/
	unsigned long *cnp;	/* returned cluster number		*/
/*FIXME*/typedef int u_long;
/*FIXME*/cluster_t /*u_long*/ i;	/* file-relative cluster iterator */
	cluster_t cn;
	cluster_t prevcn;
	u_long byteoffset;
	block_t bn;
	u_long bo;
	struct buf *bp0 = 0;
	u_long bp0_bn = -1;
	struct buf *bp1 = 0;
/*
	struct pcfsmount *pmp = dep->de_pmp;
	union fattwiddle x;
 */

	i = 0;
	cn = rip->i_clust;
	findbn = position >> sb.bnshift;
	findcn = position >> sb.cnshift;
	boff = (position & (sb.bpcluster-1)) >> sb.bnshift;

DBGprintf(("FATfs: bmap in %lo, off %ld; init lookup for %ld+%ld at %ld\n",
	INODE_NR(rip), position, findcn, boff, cn));

/*
 *  The "file" that makes up the root directory is contiguous,
 *  permanently allocated, of fixed size, and is not made up
 *  of clusters.  If the cluster number is beyond the end of
 *  the root directory, then return the number of clusters in
 *  the file.
 */
	if (cn == /*PCFSROOT*/ 1 ) {
		if (rip->i_Attributes & ATTR_DIRECTORY) {
			if (findbn > sb.rootBlk) {
				return /*E2BIG*/ NO_BLOCK;
			}
DBGprintf(("FATfs: bmap returns %ld\n", sb.rootBlk + findbn));
			return sb.rootBlk + findbn;

		} else {	/* just an empty file */
/* cannot happen? */
			return /*E2BIG*/ NO_BLOCK;
		}
	}

/*
 *  Handle all other files or directories the normal way.
 */
	if (sb.fatmask == FAT12_MASK) {	/* 12 bit fat	*/
		for (; i < findcn; i++) {
			if (ATEOF(cn, FAT12_MASK)) {
				goto hiteof;
			}
			byteoffset = cn + (cn >> 1);
			bn = (byteoffset >> sb.bnshift) + sb.fatBlk;
/*
			bo = byteoffset &  pmp->pm_brbomask;
			if (bn != bp0_bn) {
				if (bp0)
					brelse(bp0);
				if (error = bread(pmp->pm_devvp, bn,
				    pmp->pm_BytesPerSec, NOCRED, &bp0)) {
					brelse(bp0);
					return error;
				}
				bp0_bn = bn;
			}
			x.byte[0] = bp0->b_un.b_addr[bo];
 */
/*
 *  If the first byte of the fat entry was the last byte
 *  in the block, then we must read the next block in the
 *  fat.  We hang on to the first block we read to insure
 *  that some other process doesn't update it while we
 *  are waiting for the 2nd block.
 *  Note that we free bp1 even though the next iteration of
 *  the loop might need it.
 */
/*
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
/*
 *  Force the special cluster numbers in the range
 *  0x0ff0-0x0fff to be the same as for 16 bit cluster
 *  numbers to let the rest of pcfs think it is always
 *  dealing with 16 bit fats.
 */
/* FIXME */
			if ((cn & 0x0ff0) == 0x0ff0)
				cn |= 0xf000;
		}
	} else if (sb.fatmask == FAT16_MASK) {	/* 16 bit fat	*/
		for (; i < findcn; i++) {
			if (ATEOF(cn, FAT16_MASK)) {
				goto hiteof;
			}
			byteoffset = cn << 1;
			bn = (byteoffset >> sb.bnshift) + sb.fatBlk;
			bo = byteoffset & sb.brelmask;
			if (bn != bp0_bn) {
				if (bp0)
					put_block(bp0);
				bp0 = get_block(dev, bn, NORMAL);
				if (bp0 == NO_BLOCK) {
DBGprintf(("FATfs: bmap unable to get FAT block %ld\n", bn));
					return(NO_BLOCK);
				}
				bp0_bn = bn;
			}
			prevcn = cn;
			cn = *(uint16_t *)(bp0->b_data+bo);
		}
	} else if (sb.fatmask == FAT32_MASK) {	/* 32 bit fat	*/
		for (; i < findcn; i++) {
			if (ATEOF(cn, FAT32_MASK)) {
				goto hiteof;
			}
			byteoffset = cn << 2;
			bn = (byteoffset >> sb.bnshift) + sb.fatBlk;
			bo = byteoffset & sb.brelmask;
			if (bn != bp0_bn) {
				if (bp0)
					put_block(bp0);
				bp0 = get_block(dev, bn, NORMAL);
				if (bp0 == NO_BLOCK) {
DBGprintf(("FATfs: bmap unable to get FAT block %ld\n", bn));
					return(NO_BLOCK);
				}
				bp0_bn = bn;
			}
			prevcn = cn;
			cn = *(uint32_t *)(bp0->b_data+bo);
		}
	}

	if (!ATEOF(cn, sb.fatmask)) {
		if (bp0)
			put_block(bp0);
/*
		fc_setcache(rip, FC_LASTMAP, i, cn);
 */
#if 1
		return ((cn-CLUST_FIRST) * sb.blkpcluster) + sb.clustBlk;
#else
		return ((cn-CLUST_FIRST) << (sb.cnshift-sb.bnshift)) + sb.clustBlk;
#endif
	}

hiteof:
	if (cnp)
		*cnp = i;
	if (bp0)
		put_block(bp0);
	/* update last file cluster entry in the fat cache */
/*
	fc_setcache(rip, FC_LASTFC, i-1, prevcn);
 */
	return /*E2BIG*/ NO_BLOCK;
}
