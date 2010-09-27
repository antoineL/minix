/* This file contains mount and unmount functionality.
 *
 * The entry points into this file are:
 *   do_readsuper	perform the READSUPER file system request
 *   do_unmount		perform the UNMOUNT file system request
 *
 * Auteur: Antoine Leca, aout 2010.
 * Updated:
 */

#include "inc.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#ifdef   COMPAT316
#include "compat.h"	/* MUST come before <minix/sysutil.h> */
#endif
#include <minix/u64.h>
#include <minix/com.h>
#include <minix/safecopies.h>
#include <minix/syslib.h>	/* sys_safecopies{from,to} */
#include <minix/sysutil.h>	/* panic */

/* Private functions:
 *   reco_extbpb	?
 */
FORWARD _PROTOTYPE( int reco_extbpb, (struct fat_extbpb*,char fstype[])	);
FORWARD _PROTOTYPE( int reco_bootsector,
		(struct fat_bootsector *, struct fat_extbpb * *)	);
FORWARD _PROTOTYPE( int chk_bootsector, (struct fat_bootsector *)	);
FORWARD _PROTOTYPE( int find_basic_sizes, (struct fat_bootsector *)	);
FORWARD _PROTOTYPE( int find_sector_counts,
		(struct fat_bootsector *, struct fat_extbpb *)		);
FORWARD _PROTOTYPE( int chk_fatsize,
		(struct fat_bootsector *, struct fat_extbpb *)		);
FORWARD _PROTOTYPE( int calc_block_size, (void)				);

/* warning: the following lines are not failsafe macros */
#define	get_le16(arr) ((u16_t)( (arr)[0] | ((arr)[1]<<8) ))
#define	get_le32(arr) ( get_le16(arr) | ((u32_t)get_le16((arr)+2)<<16) )

/*===========================================================================*
 *				reco_extbpb				     *
 *===========================================================================*/
PRIVATE int reco_extbpb(
  struct fat_extbpb *ep,	/* is this structure an extended BPB */
  char fstype[])		/* of this proposed type ?*/
{
/* This function tries to recognizes a Extended BPB.
 * It is based on the ideas of Jonathan de Boyne Pollard,
 * http://homepage.ntlworld.com/jonathan.deboynepollard/FGA/determining-fat-widths.html
 */

  if ( ep->exBootSignature!=EXBOOTSIG
    && ep->exBootSignature!=EXBOOTSIG_ALT )
	return FALSE;	/* signature does not match */
  return ! memcmp(ep->exFileSysType, fstype, sizeof ep->exFileSysType);
}

/*===========================================================================*
 *				reco_bootsector				     *
 *===========================================================================*/
PRIVATE int reco_bootsector(
  struct fat_bootsector *bs,	/* is this structure a valid boot sector */
  struct fat_extbpb * *extbpbpp) /* where to put the extension if found */
{
/* Do the basic check of the boot sector, and locate the extended BPB (if any)
 * Also may set sb.fatmask according to the indicated FStype.
 *
 * Possible improvements:
 * could be splitted in 2, one to recognize FAT32 and the other for FAT12/16
 * the second could be called later if the first failed in some way.
 * Other way is to have argv[] option to force somehow the recognized kind.
 */
  if (memcmp(bs->bsBootSectSig, BOOTSIG, sizeof bs->bsBootSectSig)) {
/* <FUTURE_IMPROVEMENT>
 * if "veryold" (or "dos1") option is set, try to read 2nd sector,
 * and then interpret the content of the first 3 bytes
 * as being a Fx media descriptor followed by FF FF
 * if OK, subsitute the bootsector with the adequate template
 * for that media. </FUTURE_IMPROVEMENT>
 */
	DBGprintf(("mounting failed: bad magic at bytes 510-511\n"));
	return(EINVAL);
  }
  if ( (bs->bsJump[0]!=0xEB || bs->bsJump[2]!=0x90)
    && (bs->bsJump[0]!=0xE9) ) {
	DBGprintf(("mounting failed: "
		"missing jmp instruction at start: %.2X %.2X %.2X\n",
		bs->bsJump[0], bs->bsJump[1], bs->bsJump[2]));
/* FIXME: should we really abort here?
 * Right now it is a security measure to avoid consider rogue data
 * incorrectly; however, it is entirely possible to have working
 * FAT file systems without those bytes set this way;
 * as such, it should probably be possible to override it.
 */
	return(EINVAL);
  }

  sb.fatmask = 0;
  *extbpbpp = NULL;
  /* Try to recognize an extended BPB; first tries FAT32 ones */
  if (reco_extbpb(& bs->u.f32bpb.ExtBPB32, FAT32_FSTYPE)) {
	*extbpbpp = & bs->u.f32bpb.ExtBPB32;
	sb.fatmask = FAT32_MASK;
	return(OK);
  }
  if (reco_extbpb(& bs->u.f32bpb.ExtBPB32, FAT_FSTYPE)) {
	*extbpbpp = & bs->u.f32bpb.ExtBPB32;
	return(OK);
  }
  /* it didn't work; then try FAT12/FAT16, at a different place */
  if (reco_extbpb(& bs->u.ExtBPB, FAT16_FSTYPE)) {
	*extbpbpp = & bs->u.ExtBPB;
	sb.fatmask = FAT16_MASK;
	return(OK);
  }
  if (reco_extbpb(& bs->u.ExtBPB, FAT12_FSTYPE)) {
	*extbpbpp = & bs->u.ExtBPB;
	sb.fatmask = FAT12_MASK;
	return(OK);
  }
  if (reco_extbpb(& bs->u.ExtBPB, FAT_FSTYPE)) {
	*extbpbpp = & bs->u.ExtBPB;
	return(OK);
  }
  if (bs->u.f32bpb.ExtBPB32.exBootSignature==EXBOOTSIG
   || bs->u.f32bpb.ExtBPB32.exBootSignature==EXBOOTSIG_ALT ) {
	/* we found a signature for an extended BPB at the
	 * right place for a FAT file system, but the file
	 * system identifier is unknown...
	 * more robust handling would be to check isprint(exFileSysType[])
	 */
	DBGprintf(("mounting failed: found FAT32-like extended BPB, "
		"but with unkown FStype: <%.8s>\n",
		bs->u.f32bpb.ExtBPB32.exFileSysType));
	return(EINVAL);		/* should try better... */
  } else if (bs->u.ExtBPB.exBootSignature==EXBOOTSIG
          || bs->u.ExtBPB.exBootSignature==EXBOOTSIG_ALT ) {
	DBGprintf(("mounting failed: found extended BPB, "
		"but with unkown FStype: <%.8s>\n",
		bs->u.ExtBPB.exFileSysType));
	return(EINVAL);		/* should try better... */
  } else {
	/* we did not found the signature for an extended BPB...
	 */
	DBGprintf(("warning while mounting: missing extended BPB\n"));
/* DO NOT
	return(EINVAL);
 */
  }
  return(OK);
}

/*===========================================================================*
 *				chk_bootsector				     *
 *===========================================================================*/
PRIVATE int chk_bootsector(struct fat_bootsector *bs)
{
/* Sanity checks... */

  if (bs->bpbBytesPerSec[0]!=0
   || bs->bpbBytesPerSec[1] & (bs->bpbBytesPerSec[1] - 1) ) {
  /* message is slighty misleading: we really checked that
   * bpbBytesPerSec is a power-of-two larger than 0x100 (256);
   * but since 512 is the overall-seen value, better left the message
   * this way to point out the bad field.
   */
	DBGprintf(("mounting failed: not using 512-byte sectors\n"));
	return(EINVAL);
  }
  if (bs->bpbSecPerClust & (bs->bpbSecPerClust - 1) ) {
	DBGprintf(("mounting failed: clust/sec should be a power of 2\n"));
	return(EINVAL);
  }
  if (bs->bpbResSectors[0]==0 && bs->bpbResSectors[1]==0) {
	DBGprintf(("mounting failed: needs at least 1 reserved sector\n"));
	return(EINVAL);
  }
  if (bs->bpbFATs==0) {
	DBGprintf(("mounting failed: needs at least 1 FAT\n"));
	return(EINVAL);
  }
  if (bs->bpbFATs>15) {
	DBGprintf(("mounting failed: too much FATs (%d)\n", bs->bpbFATs));
	return(EINVAL);
  }
  return OK;
}

/*===========================================================================*
 *				find_basic_sizes			     *
 *===========================================================================*/
PRIVATE int find_basic_sizes(struct fat_bootsector *bs)
{
/* Fill the sb instance with the basic sizes.
 * The blk/block stuff would be done later, when block size is fixed.
 */
  int i;
  unsigned long bit;

  sb.bpsector = get_le16(bs->bpbBytesPerSec);
  if (sb.bpsector < MIN_SECTOR_SIZE) {
	DBGprintf(("mounting failed: sector size %lu too small\n",
		(unsigned long) sb.bpsector));
	return(EINVAL);
  } else if (sb.bpsector > MAX_SECTOR_SIZE) {
	DBGprintf(("mounting failed: sector size %lu too big\n",
		(unsigned long) sb.bpsector));
	return(EINVAL);
  }
  bit = 1;
  for (i=0; i<16; bit<<=1,++i) {
	if (bit & sb.bpsector) {
		assert(! (bit ^ sb.bpsector));	/* should be power-of-2 */
		sb.snshift = i;
		break;
	}
  }
  sb.srelmask = sb.bpsector-1;
  sb.secpcluster = bs->bpbSecPerClust;
  bit = 1;
  for (i=0; i<8; bit<<=1,++i) {
	if (bit & sb.secpcluster) {
		assert(! (bit ^ sb.secpcluster)); /* should be power-of-2 */
		sb.cnshift = sb.snshift + i;
		break;
	}
  }
  if (sb.cnshift>15) {
	DBGprintf(("mounting failed: clusters too big (%d KiB)\n",
		1<<(sb.cnshift-10)));
	return(EINVAL);
  }
  sb.csshift = sb.cnshift - sb.snshift;
  assert(bs->bpbSecPerClust == (1<<sb.csshift));
  sb.bpcluster = sb.secpcluster * sb.bpsector;
  assert(sb.bpcluster == (1<<sb.cnshift)); /* paranoia */
  sb.crelmask = sb.bpcluster-1;
  assert(sizeof(struct fat_direntry) == 32); /* paranoia */
  assert(sizeof(struct fat_lfnentry) == 32);
  assert(DIR_ENTRY_SIZE == 32);
  sb.depsec = sb.bpsector / DIR_ENTRY_SIZE;
  sb.depclust = sb.depsec << sb.cnshift;
  return OK;
}

/*===========================================================================*
 *				find_sector_counts			     *
 *===========================================================================*/
PRIVATE int find_sector_counts(
  struct fat_bootsector *bs,
  struct fat_extbpb *extbpbp)
{
/* ... */
  sector_t systemarea;

  sb.nFATs = bs->bpbFATs;

  sb.resCnt = get_le16(bs->bpbResSectors);
  if ( (sb.secpfat = get_le16(bs->bpbFATsecs)) == 0) {
	if (extbpbp == &bs->u.ExtBPB
	 || sb.fatmask && sb.fatmask != FAT32_MASK) {
		DBGprintf(("mounting failed: needs at least 1 sector/FAT\n"));
		return(EINVAL);
	}
	if (extbpbp != &bs->u.f32bpb.ExtBPB32)
		DBGprintf(("warning: mounting FAT32 without extended BPB\n"));
	sb.secpfat = get_le32(bs->u.f32bpb.bpbBigFATsecs);
	if (sb.secpfat <= 0) {
		/* still 0 ??? */
		DBGprintf(("mounting failed: needs at least 1 sector/FAT\n"));
		return(EINVAL);
	}
  }
  if (INT_MAX<65535 && get_le16(bs->bpbRootDirEnts) > (unsigned)INT_MAX) {
	DBGprintf(("mounting failed: too much root dir entries (%u)\n",
		get_le16(bs->bpbRootDirEnts)));
	return(EINVAL);
  }
  sb.rootEntries = get_le16(bs->bpbRootDirEnts);
  sb.rootCnt = (sb.rootEntries+sb.depsec-1) / sb.depsec;
#if 0
/* FIXME: rewrite (FAT32MASK) or move later */
  if (extbpbp == &bs->u.f32bpb.ExtBPB32) {
	DBGprintf(("warning: FAT32-style extended BPB with fixed root directory!?\n"));
	/* return(EINVAL); */
  }

/* A bad case is if a FAT16 fs was written (minimally) over a preexisting FAT32,
 * in a way such that the FAT32 extended BPB is still there...
 * Our logic above detects the FAT32 BPB, but then with the number we should consider
 * it as FAT16... but we will disregard the possible FAT16 extended BPB...
 * (note that FAT16 ext.BPB lays until 0x3D, while FAT32 ext. BPB starts at 0x40)
 * A "warn-me" case can be when byte 0x26 (& bs->u.ExtBPB) contains 0x29 (EXBOOTSIG)
 */
#endif
  if ( (sb.totalSecs = get_le16(bs->bpbSectors)) == 0) {
	sb.totalSecs = get_le32(bs->bpbHugeSectors);
  }
  sb.fatsCnt = sb.nFATs * sb.secpfat;
  systemarea = sb.resCnt + sb.fatsCnt + sb.rootCnt;
  if ( (systemarea + sb.secpcluster) >= sb.totalSecs) {
	DBGprintf(("mounting failed: incoherent sizes, "
		"sysarea=%lu total=%lu\n",
		(unsigned long)systemarea, (unsigned long)sb.totalSecs));
	return(EINVAL);
  }
  sb.clustCnt = (sb.totalSecs - systemarea) & ~sb.crelmask;
  sb.maxFilesize = (ULONG_MAX>>sb.snshift) < sb.clustCnt
	? ULONG_MAX : sb.clustCnt<<sb.snshift;

  sb.resSec   = 0;
  sb.fatSec   = sb.resCnt;
  sb.rootSec  = sb.fatSec + sb.fatsCnt;
  sb.clustSec = sb.rootSec + sb.rootCnt;
  assert( (sb.clustSec + sb.clustCnt) <= sb.totalSecs);
  return OK;
}

/*===========================================================================*
 *				chk_fatsize				     *
 *===========================================================================*/
PRIVATE int chk_fatsize(
  struct fat_bootsector *bs,
  struct fat_extbpb *extbpbp)
{
/* Final check about FAT sizes */
  sector_t slack;	/* number of useless sector(s) in each FAT */

  sb.maxClust = (sb.clustCnt / sb.secpcluster) + 1;
  if (sb.maxClust > CLUSTMASK_EOF32) {
	DBGprintf(("mounting failed: too much clusters, maxClust=%lu=%#.8lx\n",
		(unsigned long)sb.maxClust, (unsigned long)sb.maxClust));
	return(EINVAL);
  }
  if (sb.fatmask == FAT12_MASK && ! FS_IS_FAT12(sb.maxClust)
   || sb.fatmask == FAT16_MASK && ! FS_IS_FAT16(sb.maxClust)
   || sb.fatmask == FAT32_MASK && ! FS_IS_FAT32(sb.maxClust)) {
	DBGprintf(("mounting failed: extended BPB says "
		"FStype=<%.8s> but maxClust=%lu, not compatible\n",
		extbpbp->exFileSysType, (unsigned long)sb.maxClust));
	return(EINVAL);
  }

  if (FS_IS_FAT12(sb.maxClust)) {
	sb.fatmask = FAT12_MASK;	sb.eofmask = CLUSTMASK_EOF12;
	assert(!FS_IS_FAT16(sb.maxClust));	/* paranoia */
	assert(!FS_IS_FAT32(sb.maxClust));
	sb.nibbles = 3;
/* FIXME: PLUS virtual methods... +++ */
  }
  else if (FS_IS_FAT16(sb.maxClust)) {
	sb.fatmask = FAT16_MASK;	sb.eofmask = CLUSTMASK_EOF16;
	assert(!FS_IS_FAT12(sb.maxClust));	/* paranoia */
	assert(!FS_IS_FAT32(sb.maxClust));
	sb.nibbles = 4;
  }
  else if (FS_IS_FAT12(sb.maxClust)) {
	sb.fatmask = FAT32_MASK;	sb.eofmask = CLUSTMASK_EOF32;
	assert(!FS_IS_FAT12(sb.maxClust));	/* paranoia */
	assert(!FS_IS_FAT16(sb.maxClust));
	sb.nibbles = 8;
  }
  assert(sb.fatmask != 0);
  assert(sb.maxClust < sb.fatmask);
  assert( (sb.maxClust & ~sb.fatmask) == 0);
  assert(sb.maxClust < (sb.fatmask & CLUST_BAD) );
  if ( (sb.maxClust*sb.nibbles+1)/2 >= sb.secpfat*sb.bpsector) {
	DBGprintf(("mounting failed: "
		"FAT%d with only %ld sectors/FAT but %ld clusters\n",
		sb.nibbles*4, (long)sb.secpfat, (long) sb.maxClust));
	return(EINVAL);
  }
  DBGprintf(("FATfs: mounting on %s, %ld clusters, %d rootdir entries\n",
		fs_dev_label, (long)sb.maxClust-1, sb.rootEntries));

  slack = sb.secpfat
        - ( (sb.maxClust*sb.nibbles+1)/2 -1 + sb.bpsector ) / sb.bpsector;
  DBGprintf(("FATfs: FATs are %ld sectors, slack=%ld\n",
		(long)sb.secpfat, (long)slack));

  return OK;
}

/*===========================================================================*
 *				calc_block_size				     *
 *===========================================================================*/
PRIVATE int calc_block_size(void)
{
/* Compute the best possible size of the block as used in the cache system.
 * Fill all the related ("Blk", "Siz", etc.) fieds of sb accordingly.
 *
 * The easiest way is to stick on sectors, since everything is computed
 * as sector count in a FAT file system. However it pratically prevents
 * to use the VM-based level2 cache (on i386), which is based on 4K-pages.
 *
 * The second best option is to check if by chance, the FATs and the data
 * blocks are aligned on 4KB boundaries: this practically means we were
 * using FAT32, since most FAT12 and FAT16 file systems only have 1 reserved
 * sector at the beginning, and everything then is mis-aligned.
 *
 * A third option (not implemented) is to choose the bigger size such as
 * the data area is still aligned, and to compensate the misalignement
 * the FAT area and the root directory when computing block numbers;
 * this means additional complexity when updating, when flushing...
 */
  unsigned bsize;		/* proposed block size */
  int shift;			/* shift a sector number right this amount
				 * to get a block number. Positive or negative
				 */
  int relmask;			/* these bits in a sector number will
				 * be merged into a unique block
				 */
  int i;
  unsigned long bit;

  bsize = sb.bpsector;
  shift = 0;
  /* First, reduce bsize to fit the maximum manageable size for cache. */
  while (bsize > MAX_BLOCK_SIZE) {
	bsize /= 2;
	shift--;
	/* note that assuming bsize is a power-of-two,
	 * we shall not enter the while() loop below.
	 */
  }

  relmask = 0;
  /* Now try to increase the size while keeping everything block-aligned */
  while (bsize < MAX_BLOCK_SIZE && bsize < sb.bpcluster) {
	/* If possible, we will try to double bsize. */

	 /* Compute the resulting relmask, and see what would happen */
	relmask = (relmask<<1) + 1;

	/* The FAT should begin on a block boundary */
	if (sb.fatSec & relmask)
		break;
	/* Each FAT should be an integral number of blocks.
	 *   This one could be dropped, but would require
	 *   special handling to update the (unaligned) FAT mirror(s).
	 */
	if ( sb.secpfat & relmask)
		break;
	/* The root directory, if any, should begin on a block boundary */
	if (sb.rootCnt && sb.rootSec & relmask)
		break;
	/* The root directory should be an integer count of blocks
	 * Not needed, could be done side effect of its neighbours...
	 */
	if (sb.rootCnt & relmask)
		break;
	/* The data area should begin on a block boundary */
	if (sb.clustSec & relmask)
		break;
	/* Note that we do not require the total number of sectors
	 * to be an integral multiple; this is because anything after
	 * the last full block would be an incomplete cluster, so
	 * unavailable for allocation as part of the FAT file system.
	 * (it is still a problem for bread/bwrite, though).
	 */

	/* OK, all tests are positive, double the block_size */
	bsize *= 2;
	shift++;
	assert(bsize <= MAX_BLOCK_SIZE && bsize <= sb.bpcluster);
  }

  if (bsize < MIN_BLOCK_SIZE) {
	DBGprintf(("mounting failed: block size %u too small\n", bsize));
	return(EINVAL);
  }

  bcc.bpblock = bsize;
  bit = 1;
  for (i=0; i<16; bit<<=1,++i) {
	if (bit & bcc.bpblock) {
		assert(! (bit ^ bcc.bpblock));	/* should be power-of-2 */
		bcc.bnshift = i;
		break;
	}
  }
  assert(bcc.bnshift == sb.snshift + shift);
  bcc.brelmask = bcc.bpblock-1;
  bcc.blkpcluster = sb.secpcluster >>shift;

  bcc.cbshift = sb.csshift + shift;
  assert(bcc.cbshift == sb.cnshift - bcc.bnshift);
  assert(bcc.blkpcluster == (1<<bcc.cbshift));
  bcc.depblk = bcc.bpblock / DIR_ENTRY_SIZE;

  bcc.resBlk = sb.resSec;
  bcc.resSiz = bcc.fatBlk = sb.resCnt >>shift;
  bcc.blkpfat = sb.secpfat >>shift;
/* Because of possible unalignment of mirror FAT, do not do
 * bcc.fatsSiz = sb.nFATs * bcc.blkpfat;
 */
  bcc.fatsSiz = sb.fatsCnt >>shift;
  bcc.rootBlk = bcc.fatBlk + bcc.fatsSiz;
  assert(bcc.rootBlk == sb.rootSec >>shift);
  bcc.rootSiz = sb.rootCnt >>shift;
  bcc.clustBlk = bcc.rootBlk + bcc.rootSiz;
  assert(bcc.clustBlk == sb.clustSec >>shift);

  bcc.totalBlks = sb.totalSecs >>shift;
  bcc.clustSiz = sb.clustCnt >>shift;
  return OK;
}

/*===========================================================================*
 *				do_readsuper				     *
 *===========================================================================*/
PUBLIC int do_readsuper(void)
{
/* This function reads the superblock of the partition, builds the root inode
 * and sends back the details of it.
 *
 * CHECKME if the following is still relevant?
 * Note, that the FS process does not know the index of the vmnt object which
 * refers to it, whenever the pathname lookup leaves a partition an
 * ELEAVEMOUNT error is transferred back so that the VFS knows that it has
 * to find the vnode on which this FS process' partition is mounted on.
 *
 * Warning: this code is not reentrant (use static local variable, without mutex)
 */
  int r;
  struct inode *root_ip;
  struct fat_extbpb *extbpbp;
  off_t rootDirSize;
  static struct fat_bootsector *bs;  

  STATICINIT(bs, MAX_SECTOR_SIZE);	/* allocated once, made contiguous */

  if (state == MOUNTED)
         return(EINVAL);

  DBGprintf(("FATfs: readsuper (dev %#.4x, flags %x)\n",
	(dev_t) m_in.REQ_DEV, m_in.REQ_FLAGS));

  if (m_in.REQ_FLAGS & REQ_ISROOT) {
	/* FAT do not support being used as root file system on MINIX.
	 * Detailled explanations are at the beginning of type.h.
	 */
	printf("FATfs: attempt to mount as root device (/)\n");
	return EINVAL;
  }

  read_only = !!(m_in.REQ_FLAGS & REQ_RDONLY);
  dev = m_in.REQ_DEV;
  if (dev == NO_DEV)
	panic("request for mounting FAT fs on NO_DEV");

  /* Get the label and open the driver */
  if ( (r = label_to_driver(m_in.REQ_GRANT, m_in.REQ_PATH_LEN)) != OK )
	return(r);
  hard_errors = 0;

  /* Fill in the boot sector. */
  assert(sizeof *bs >= 512);	/* paranoia */
  r = seqblock_dev_io(DEV_READ_S, bs, cvu64(0), sizeof(*bs));
  if (r != sizeof(*bs))
	return(EINVAL);

  /* Now check the various fields and fill the superblock
   * structure which will keep them available for future use.
   */
  extbpbp = NULL;
  if ( (r = reco_bootsector(bs, &extbpbp)) != OK
    || (r = chk_bootsector(bs)) != OK
    || (r = find_basic_sizes(bs)) != OK
    || (r = find_sector_counts(bs, extbpbp)) != OK
    || (r = chk_fatsize(bs, extbpbp)) != OK )
	return(r);

/*FIXME sb.rootCluster et le reste des checks FAT32 (vers0 etc) */

  sb.freeClustValid = sb.freeClust = sb.nextClust = 0;
  rootDirSize = (long)sb.rootEntries * DIR_ENTRY_SIZE;

  /* Compute block_sizes */
  if ( (r = calc_block_size()) != OK )
	return(r);
  if (bcc.bpblock < MIN_BLOCK_SIZE) 
	return(EINVAL);
  if ((bcc.bpblock % 512) != 0) 
	return(EINVAL);

/* FIXME: compute num_blocks (bcc.nbufs) with knowledge of global FS size...*/
  init_cache(bcc.nbufs=NUM_BUFS, bcc.bpblock);

  if ( (r = init_fat_bitmap()) != OK )
	return(r);

  root_ip = init_inodes(NUM_INODES);

/* FIXME: use sb.rootCluster */
  if (rootDirSize) {
	/* the fixed root directory exists */
	root_ip->i_clust = CLUST_CONVROOT;	/* conventionally 1 */
	assert(rootDirSize <= 0xffffffff);
	root_ip->i_size = rootDirSize;
	root_ip->i_flags |= I_DIRSIZED;
  } else {
	/* better be FAT32 and have long BPB... */
	assert(extbpbp == &bs->u.f32bpb.ExtBPB32);
	/* FIXME: better have checked !=0 */
	root_ip->i_clust = get_le32(bs->u.f32bpb.bpbRootClust);

	/* We do not know the size of the root directory...
	 * We could enumerate the FAT chain to learn how long
	 * the directory really is; right now we do not do that,
	 * and just place a faked value of one cluster;
	 * anyway this directory will be looked up quickly...
	 */
	root_ip->i_size == sb.bpcluster;
	root_ip->i_flags |= I_DIRNOTSIZED;
  }
  root_ip->i_mode = get_mode(root_ip);
/* FIXME above... */
  sb.rootCluster = root_ip->i_clust;

/* FIXME: fake entries . and .. in root dir; will fix
  root_ip->i_entrypos = ???
 */

  /* Future work: may fetch the volume name (either extBPB or root dir) */

  m_out.RES_INODE_NR = INODE_NR(root_ip);
  m_out.RES_MODE = root_ip->i_mode;
  m_out.RES_FILE_SIZE_HI = 0;
  m_out.RES_FILE_SIZE_LO = root_ip->i_size;
  m_out.RES_UID = use_uid;
  m_out.RES_GID = use_gid;
  m_out.RES_DEV = dev;

  state = MOUNTED;

  return OK;
}

/*===========================================================================*
 *				do_unmount				     *
 *===========================================================================*/
PUBLIC int do_unmount()
{
/* Unmount the file system.
 */
  struct inode *root_ip;

  if (state != MOUNTED)
         return(EINVAL);

#if 1
  /* Decrease the reference count of the root inode.
   * Do not increase the inode refcount.
   */
  if ((root_ip = fetch_inode(ROOT_INODE_NR)) == NULL) {
	panic("couldn't find root inode while unmounting\n");
	return EINVAL;
  }
/* FIXME: check root_ip->i_ref==1 */
  put_inode(root_ip);

  /* There should not be any referenced inodes anymore now. */
  if (have_used_inode()) {
 	/* this is NOT clean unmounting! */
	printf("in-use inodes left at unmount time!\n");
  #if 0
	/* MFS even does: */
	return(EBUSY);		/* can't umount a busy file system */
  #endif
  }
#endif

#if 0
/* now the MFS view */
  /* See if the mounted device is busy.  Only 1 inode using it should be
   * open --the root inode-- and that inode only 1 time. */
  count = 0;
  for (rip = &inode[0]; rip < &inode[NR_INODES]; rip++) 
	  if (rip->i_count > 0 && rip->i_dev == fs_dev) count += rip->i_count;

  if ((root_ip = find_inode(fs_dev, ROOT_INODE)) == NULL) {
	panic("MFS: couldn't find root inode\n");
	return(EINVAL);
  }
   
  if (count > 1) return(EBUSY);	/* can't umount a busy file system */
  put_inode(root_ip);
#endif

/* FAT specifics:
 * we should update the FSInfo sector
 * unless "no_clean_check":
 *	we should mark FAT[1] as "umounted clean" (after flush), in 2 places
 *	if hard_errors == 0, mark "no harderrs", in 2 places
 */
/* FIXME: if FAT12, deref the special blocks containing the FAT */

  done_fat_bitmap();

  /* force any cached blocks out of memory */
/* CHECKME: perhaps better to call explicitely
	flush_inodes()+flushall()+invalidate()
 * CHECME fsync()? (will imply invalidate)
 */
  do_sync();

  /* Close the device the file system lives on. */
  dev_close(driver_e, dev);

  /* Finish off the unmount. */
  dev = NO_DEV;
  state = UNMOUNTED;

  return OK;
}
