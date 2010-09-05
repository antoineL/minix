/* This file contains the procedures that deals with FATs.
 *
 *  The entry points into this file are
 *   bmap		map a file-relative offset into a block number
 *   smap		map a file-relative offset into a sector number
 *
 *
 * Auteur: Antoine Leca, septembre 2010.
 * The basis for this file is the PCFS package targetting
 * 386BSD 0.1, published in comp.unix.bsd on October 1992
 * by Paul Popelka; his work is to be rewarded.  * Updated:
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

/*
 *  Fat cache stats.
 */
int fc_fileextends       = 0;	/* # of file extends			*/
int fc_lfcempty          = 0;	/* # of time last file cluster cache entry
				 * was empty */
int fc_bmapcalls         = 0;	/* # of times pcbmap was called		*/
#define	LMMAX	20
int fc_lmdistance[LMMAX];	/* counters for how far off the last cluster
				 * mapped entry was. */
int fc_largedistance     = 0;	/* off by more than LMMAX		*/

/*
 *  Set a slot in the fat cache.
 */
#define	fc_setcache(dep, slot, frcn, fsrcn) \
	(dep)->de_fc[slot].fc_frcn = frcn; \
	(dep)->de_fc[slot].fc_fsrcn = fsrcn;

_PROTOTYPE( block_t bmap, (		);
_PROTOTYPE( sector_t smap, (struct inode *rip, off_t position)		);

/*===========================================================================*
 *				bmap					     *
 *===========================================================================*/
PUBLIC block_t bmap(struct inode *rip, off_t position)
{
  return smap(rip, position);
}

/*===========================================================================*
 *				smap					     *
 *===========================================================================*/
PUBLIC sector_t smap(struct inode *rip, off_t position)
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
	daddr_t *bnp;		/* returned filesys relative blk number	*/
	unsigned long *cnp;	/* returned cluster number		*/
#endif
	int error;
/*FIXME*/typedef int u_long;
	u_long i;
	u_long cn;
	u_long prevcn;
	u_long byteoffset;
	u_long bn;
	u_long bo;
	struct buf *bp0 = 0;
	u_long bp0_bn = -1;
	struct buf *bp1 = 0;
/*
	struct pcfsmount *pmp = dep->de_pmp;
	union fattwiddle x;
 */
	fc_bmapcalls++;

/*
 *  If they don't give us someplace to return a value
 *  then don't bother doing anything.
	if (bnp == NULL  &&  cnp == NULL)
		return 0;
 */

	i = 0;
	cn = rip->i_clust;
/*
 *  The "file" that makes up the root directory is contiguous,
 *  permanently allocated, of fixed size, and is not made up
 *  of clusters.  If the cluster number is beyond the end of
 *  the root directory, then return the number of clusters in
 *  the file.
 */
	if (cn == PCFSROOT) {
		if (dep->de_Attributes & ATTR_DIRECTORY) {
			if (findcn * pmp->pm_SectPerClust > pmp->pm_rootdirsize) {
				if (cnp)
					*cnp = pmp->pm_rootdirsize / pmp->pm_SectPerClust;
				return E2BIG;
			}
			if (bnp)
				*bnp = pmp->pm_rootdirblk + (findcn * pmp->pm_SectPerClust);
			if (cnp)
				*cnp = PCFSROOT;
			return 0;
		} else {	/* just an empty file */
			if (cnp)
				*cnp = 0;
			return E2BIG;
		}
	}

/*
 *  Rummage around in the fat cache, maybe we can avoid
 *  tromping thru every fat entry for the file.
 *  And, keep track of how far off the cache was from
 *  where we wanted to be.
 */
	fc_lookup(dep, findcn, &i, &cn);
	if ((bn = findcn - i) >= LMMAX)
		fc_largedistance++;
	else
		fc_lmdistance[bn]++;

/*
 *  Handle all other files or directories the normal way.
 */
	if (FAT12(pmp)) {	/* 12 bit fat	*/
		for (; i < findcn; i++) {
			if (PCFSEOF(cn)) {
				goto hiteof;
			}
			byteoffset = cn + (cn >> 1);
			bn = (byteoffset >> pmp->pm_bnshift) + pmp->pm_fatblk;
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
/*
 *  If the first byte of the fat entry was the last byte
 *  in the block, then we must read the next block in the
 *  fat.  We hang on to the first block we read to insure
 *  that some other process doesn't update it while we
 *  are waiting for the 2nd block.
 *  Note that we free bp1 even though the next iteration of
 *  the loop might need it.
 */
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
			cn = x.word & 0x0fff;
/*
 *  Force the special cluster numbers in the range
 *  0x0ff0-0x0fff to be the same as for 16 bit cluster
 *  numbers to let the rest of pcfs think it is always
 *  dealing with 16 bit fats.
 */
			if ((cn & 0x0ff0) == 0x0ff0)
				cn |= 0xf000;
		}
	} else {				/* 16 bit fat	*/
		for (; i < findcn; i++) {
			if (PCFSEOF(cn)) {
				goto hiteof;
			}
			byteoffset = cn << 1;
			bn = (byteoffset >> pmp->pm_bnshift) + pmp->pm_fatblk;
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
			prevcn = cn;
			cn = *(u_short *)(bp0->b_un.b_addr+bo);
		}
	}

	if (!PCFSEOF(cn)) {
		if (bp0)
			brelse(bp0);
		if (bnp)
			*bnp = cntobn(pmp, cn);
		if (cnp)
			*cnp = cn;
		fc_setcache(dep, FC_LASTMAP, i, cn);
		return 0;
	}

hiteof:;
	if (cnp)
		*cnp = i;
	if (bp0)
		brelse(bp0);
	/* update last file cluster entry in the fat cache */
	fc_setcache(dep, FC_LASTFC, i-1, prevcn);
	return E2BIG;
}

/*
 *  Find the closest entry in the fat cache to the
 *  cluster we are looking for.
 */
fc_lookup(dep, findcn, frcnp, fsrcnp)
	struct denode *dep;
	unsigned long findcn;
	unsigned long *frcnp;
	unsigned long *fsrcnp;
{
	int i;
	unsigned long cn;
	struct fatcache *closest = 0;

	for (i = 0; i < FC_SIZE; i++) {
		cn = dep->de_fc[i].fc_frcn;
		if (cn != FCE_EMPTY  &&  cn <= findcn) {
			if (closest == 0  ||  cn > closest->fc_frcn)
				closest = &dep->de_fc[i];
		}
	}
	if (closest) {
		*frcnp  = closest->fc_frcn;
		*fsrcnp = closest->fc_fsrcn;
	}
}

