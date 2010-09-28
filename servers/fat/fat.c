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

#include <stdlib.h>
#include <string.h>

#ifdef	COMPAT316
#include "compat.h"	/* MUST come before <minix/sysutil.h> */
#endif
#include <minix/sysutil.h>	/* panic */

/*
 *  This union is useful for manipulating entries
 *  in 12 bit fats.
 */
union fattwiddle {
	unsigned short word;
	unsigned char  byte[2];
}; 
/* Public functions: */
#define	FAT_GET		0x0001		/* get a fat entry		*/
#define	FAT_SET		0x0002		/* set a fat entry		*/
#define	FAT_GET_AND_SET	(FAT_GET | FAT_SET) 
/*
int clusterfree __P((struct pcfsmount *pmp, unsigned long cn));
int clusteralloc __P((struct pcfsmount *pmp, unsigned long *retcluster,
	unsigned long fillwith));
int fatentry __P((int function, struct pcfsmount *pmp,
	unsigned long cluster,
	unsigned long *oldcontents,
	unsigned long newcontents));
int freeclusterchain __P((struct pcfsmount *pmp, unsigned long startchain)); 
*/

unsigned char * inuse_bitmap;	/* ptr to bitmap of in-use clusters */ 
/* Private functions:
 *   get_mask		?
 */
/* useful only with symlinks... dropped ATM
FORWARD _PROTOTYPE( int ltraverse, (struct inode *rip, char *suffix)	);
 */
FORWARD void fc_setcache(struct inode *rip, int slot,
  cluster_t frcn,
  cluster_t abscn,
  block_t bn);
FORWARD void fc_lookup(struct inode *rip,
  cluster_t findcn,
  cluster_t * frcnp,
  cluster_t * abscnp);

FORWARD int fatentry(
/*	int function, */
/* struct pcfsmount *pmp, */
	cluster_t cluster,
	cluster_t *oldcontents,
	cluster_t newcontents);

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
 * For every free cluster found turn off its
 * corresponding bit in the inuse_bitmap.
 */
/* FIXME: if FAT12, read the FAT into specials (contiguous) buffers
 * Do not forget to increase bufs_in_use...
 */
	struct buf *bp0 = 0;
	block_t bp0_blk = NO_BLOCK;
	struct buf *bp1 = 0;
block_t cn;
	block_t whichblk;
	int whichbyte;
	int error = 0;
	union fattwiddle x;

/*
 *  Allocate memory for the bitmap of allocated clusters,
 *  and then fill it in.
 * FIXME: not needed if read-only...
 */
  if (inuse_bitmap)
	free(inuse_bitmap);
	inuse_bitmap = malloc((sb.maxClust >> 3) + 1);
	if (!inuse_bitmap) {
		error = ENOMEM;
		goto error_exit;
	}
/*
 *  Mark all clusters in use, we mark the free ones in the
 *  fat scan loop further down.
 */
	for (cn = 0; cn < (sb.maxClust >> 3) + 1; cn++)
		inuse_bitmap[cn] = 0xff;

/*
 *  Figure how many free clusters are in the filesystem
 *  by ripping thougth the fat counting the number of
 *  entries whose content is zero.  These represent free
 *  clusters.
 */
	sb.freeClust = 0;
	sb.nextClust = -1;
	for (cn = CLUST_FIRST; cn <= sb.maxClust; cn++) {
		switch (sb.fatmask) {
		case FAT12_MASK:
			whichbyte = cn + (cn >> 1);
			whichblk  = (whichbyte >> bcc.bnshift) + bcc.fatBlk;
			whichbyte = whichbyte & bcc.brelmask;
			if (whichblk != bp0_blk) {
				if (bp0)
					put_block(bp0);
/*
				error = bread(pmp->pm_devvp, whichblk,
					bcc.bpblock, NOCRED, &bp0);
 */
				if (error) {
					goto error_exit;
				}
				bp0_blk = whichblk;
			}
			x.byte[0] = bp0->b_data[whichbyte];
			if (whichbyte == (bcc.bpblock-1)) {
/*
				error = bread(pmp->pm_devvp, whichblk+1,
					bcc.bpblock, NOCRED, &bp1);
 */
				if (error)
					goto error_exit;
				x.byte[1] = bp1->b_data[0];
				put_block(bp0);
				bp0 = bp1;
				bp1 = NULL;
				bp0_blk++;
			} else {
				x.byte[1] = bp0->b_data[whichbyte + 1];
			}
			if (cn & 1)
				x.word >>= 4;
			x.word &= 0x0fff;
			break;
		case FAT16_MASK:
			whichbyte = cn << 1;
			whichblk  = (whichbyte >> bcc.bnshift) + bcc.fatBlk;
			whichbyte = whichbyte & bcc.brelmask;
			if (whichblk != bp0_blk) {
				if (bp0)
					put_block(bp0);
				bp0 = get_block(dev, whichblk, NORMAL);
				if (bp0 == NO_BLOCK) {
					DBGprintf(("FATfs: init_fat_bitmap unable to get "
						"FAT block %ld\n", (long)whichblk));
					error = EIO;
					goto error_exit;
				}
/*
				if (error)
					goto error_exit;
 */
				bp0_blk = whichblk;
			}
			x.byte[0] = bp0->b_data[whichbyte];
			x.byte[1] = bp0->b_data[whichbyte+1];
		}
		if (x.word == 0) {
			sb.freeClust++;
			inuse_bitmap[cn >> 3] &= ~(1 << (cn & 0x07));
			if (sb.nextClust < 0)
				sb.nextClust = cn;
		}
	}
	put_block(bp0);


  DBGprintf(("FATfs: FAT bitmap are %u bytes\n", (sb.maxClust >> 3) + 1));


	return OK;

error_exit:;
	if (bp0)
		put_block(bp0);
	if (bp1)
		put_block(bp1);
	return error;
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
	assert(IS_ROOT(rip));
	if (findbn > bcc.rootBlk) {
		return /*E2BIG*/ NO_BLOCK;
	}
	return bcc.rootBlk + findbn;
  }

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
#if 0
			cn = *(uint16_t *)(bp0->b_data+bo);
#else
			cn = get_le16(bp0->b_zfat16[bo]);
#endif
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

/*===========================================================================*
 *				xxx					     *
 *===========================================================================*/
/*
 *  Updating entries in 12 bit fats is a pain in the butt.
 *  So, we have a function to hide this ugliness.
 *
 *  The following picture shows where nibbles go when
 *  moving from a 12 bit cluster number into the appropriate
 *  bytes in the FAT.
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
 *    m = n + (n >> 2)
 *
 *  This function is written for little endian machines.
 *  least significant byte stored into lowest address.
 */
void
setfat12slot(
	struct buf *bp0,
	struct buf *bp1,
	int oddcluster,
	int byteoffset,
	int newvalue)
{
	unsigned char *b0;
	unsigned char *b1;
	union fattwiddle x;

/*
 *  If we have a 2nd buf header and the byte offset is not the
 *  last byte in the buffer, then something is fishy.  Better
 *  tell someone.
 */
	if (bp1  &&  byteoffset != 511)
		printf("setfat12slot(): bp1 %08x, byteoffset %d shouldn't happen\n",
			bp1, byteoffset);

/*
 *  Get address of 1st byte and 2nd byte setup
 *  so we don't worry about which buf header to
 *  be poking around in.
 */
	b0 = (unsigned char *)&bp0->b_data[byteoffset];
	if (bp1)
		b1 = (unsigned char *)&bp1->b_data[0];
	else
		b1 = b0 + 1;

/*printf("setfat12(): offset %d, old %02x%02x, new %04x\n", byteoffset, *b0, *b1, newvalue); */

	if (oddcluster) {
		x.word = newvalue << 4;
		*b0 = (*b0 & 0x0f) | x.byte[0];
		*b1 = x.byte[1];
	} else {
		x.word = newvalue & 0x0fff;
		*b0 = x.byte[0];
		*b1 = (*b1 & 0xf0) | x.byte[1];
	}
/*printf("setfat12(): result %02x%02x\n", *b0, *b1); */
}

/*===========================================================================*
 *				fatentry				     *
 *===========================================================================*/
/*
 *  Get or Set or 'Get and Set' the cluster'th entry in the
 *  fat.
 *  function - whether to get or set a fat entry
Always do SET; do GET_AND_SET if oldcontents
 *  pmp - address of the pcfsmount structure for the
 *    filesystem whose fat is to be manipulated.
 *  cluster - which cluster is of interest
 *  oldcontents - address of a word that is to receive
 *    the contents of the cluster'th entry if this is
 *    a get function
 *  newcontents - the new value to be written into the
 *    cluster'th element of the fat if this is a set
 *    function.
 *
 *  This function can also be used to free a cluster
 *  by setting the fat entry for a cluster to 0.
 *
 *  All copies of the fat are updated if this is a set
 *  function.
 *  NOTE:
 *    If fatentry() marks a cluster as free it does not
 *    update the inusemap in the pcfsmount structure.
 *    This is left to the caller.
 */
PRIVATE int fatentry(
/*	int function, */
/* struct pcfsmount *pmp, */
	cluster_t cluster,
	cluster_t *oldcontents,
	cluster_t newcontents)
{
	int error;
	u_long whichbyte;
	u_long whichblk;
	struct buf *bp0 = 0;
	struct buf *bp1 = 0;
	union fattwiddle x;
/*printf("fatentry(func %d, pmp %08x, clust %d, oldcon %08x, newcon %d)\n",
	function, pmp, cluster, oldcontents, newcontents);*/

/*
 *  Be sure they asked us to do something.
 *
	if ((function & (FAT_SET | FAT_GET)) == 0) {
		printf("fatentry(): function code doesn't specify get or set\n");
		return EINVAL;
	}
 *
 *  If they asked us to return a cluster number
 *  but didn't tell us where to put it, give them
 *  an error.
 *
	if ((function & FAT_GET)  &&  oldcontents == NULL) {
		printf("fatentry(): get function with no place to put result\n");
		return EINVAL;
	}
 *
 *  Be sure the requested cluster is in the filesystem.
 */
  if (cluster < CLUST_FIRST || cluster > sb.maxClust)
	return EINVAL;

  switch (sb.fatmask) {
  case FAT12_MASK:
	whichbyte = cluster + (cluster >> 1);
	whichblk  = (whichbyte >> bcc.bnshift) + bcc.fatBlk;
	whichbyte &= bcc.brelmask;
/*
*  Read in the fat block containing the entry of interest.
*  If the entry spans 2 blocks, read both blocks.
*/
/*
	error = bread(pmp->pm_devvp, whichblk,
	    bcc.bpblock, NOCRED, &bp0);
*/
	if (error) {
		put_block(bp0);
		return error;
	}
	if (whichbyte == (bcc.bpblock-1)) {
/*
		error = bread(pmp->pm_devvp, whichblk+1,
		    bcc.bpblock, NOCRED, &bp1);
*/
		if (error) {
			put_block(bp0);
			return error;
		}
	}
	if (/*function & FAT_GET*/ oldcontents) {
		x.byte[0] = bp0->b_data[whichbyte];
		x.byte[1] = bp1 ? bp1->b_data[0] :
				  bp0->b_data[whichbyte+1];
		if (cluster & 1)
			x.word >>= 4;
		x.word &= 0x0fff;
		/* map certain 12 bit fat entries to 16 bit */
		if ((x.word & 0x0ff0) == 0x0ff0)
			x.word |= 0xf000;
		*oldcontents = x.word;
	}
	/* if (function & FAT_SET) */ {
		setfat12slot(bp0, bp1, cluster & 1, whichbyte,
			newcontents);
/*
		updateotherfats(pmp, bp0, bp1, whichblk);
*/

/*
 *  Write out the first fat last.
 */
/*
		if (pmp->pm_waitonfat)
			bwrite(bp0);
		else
			bdwrite(bp0);
*/
		bp0 = NULL;
		if (bp1) {
/*
			if (pmp->pm_waitonfat)
				bwrite(bp1);
			else
				bdwrite(bp1);
*/
			bp1 = NULL;
		}
/*
		pmp->pm_fmod++;
*/
	}
	break;
  case FAT16_MASK:
	whichbyte = cluster << 1;
	whichblk  = (whichbyte >> bcc.bnshift) + bcc.fatBlk;
	whichbyte &= bcc.brelmask;
/*
	error = bread(pmp->pm_devvp, whichblk,
	    bcc.bpblock, NOCRED, &bp0);
*/
	if (error) {
		put_block(bp0);
		return error;
	}
	if (/*function & FAT_GET*/ oldcontents) {
		*oldcontents = *((u_short *)(bp0->b_data +
			whichbyte));
	}
	/* if (function & FAT_SET) */ {
		*(u_short *)(bp0->b_data+whichbyte) = newcontents;
/*
		updateotherfats(pmp, bp0, 0, whichblk);
*/
#if 0
		if (pmp->pm_waitonfat)
			bwrite(bp0);	/* write out blk from the 1st fat */
		else
			bdwrite(bp0);
#endif
		bp0 = NULL;
/*
		pmp->pm_fmod++;
*/
	}
	break;
  case FAT32_MASK:
	assert(sb.fatmask==32);
  }
  if (bp0)
	put_block(bp0);
  if (bp1)
	put_block(bp1);
  return OK;
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

/* Do not try to extend the root directory */
	 /* if (IS_ROOT(rip)) { */
/* FIXME: can do it for FAT32... */
  if (rip->i_clust == CLUST_CONVROOT) {
	DBGprintf(("extendfile(): attempt to extend root directory\n"));
	return ENOSPC;
  }

/*
	fc_fileextends++;
 */
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

/*
 *  Allocate another cluster and chain onto the end of the file.
 *  If the file is empty we make de_StartCluster point to the
 *  new block.  Note that de_StartCluster being 0 is sufficient
 *  to be sure the file is empty since we exclude attempts to
 *  extend the root directory above, and the root dir is the
 *  only file with a startcluster of 0 that has blocks allocated
 *  (sort of).
 */
	if (error = clusteralloc(/*pmp, */ &cn, CLUST_EOFE))
		return error;
	if (rip->i_clust == 0) {
		rip->i_clust = cn;
		frcn = 0;
	} else {
		error = fatentry(/*FAT_SET,*/ /*pmp,*/ rip->i_fc[FC_LASTFC].fc_abscn,
			NULL, cn);
		if (error) {
			clusterfree(/*pmp,*/ cn);
			return error;
		}

		frcn = rip->i_fc[FC_LASTFC].fc_frcn + 1;
	}

/*
 *  Get the buf header for the new block of the file.
 */
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
	return 0;
}

/*===========================================================================*
 *				lastcluster				     *
 *===========================================================================*/
PUBLIC cluster_t lastcluster(struct inode *rip,
  cluster_t *frcnp)
{
}

/*===========================================================================*
 *				peek_bmap				     *
 *===========================================================================*/
PUBLIC block_t peek_bmap(struct inode *rip,
  unsigned long position)
{
}
