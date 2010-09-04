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

#include <minix/ds.h>

#include "super.h"
#include "inode.h"
#include "fat.h"
/* warning: the following lines are not failsafe macros */
#define	get_le16(arr) ((u16_t)( (arr)[0] | ((arr)[1]<<8) ))
#define	get_le32(arr) ( get_le16(arr) | ((u32_t)get_le16((arr)+2)<<16) )

/*===========================================================================*
 *				recognize_extbpb			     *
 *===========================================================================*/
PRIVATE int recognize_extbpb(
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
 *				recognize_bootsector			     *
 *===========================================================================*/
PRIVATE int recognize_bootsector(
  struct fat_bootsector *bs,
  struct fat_extbpb *extbpbp
  )		/* of this proposed type ?*/
{
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
	return(EINVAL);
  }

  sb.fatmask = 0;
  extbpbp = NULL;
  /* Try to recognize an extended BPB; first tries FAT32 ones */
  if (recognize_extbpb(&(bs->u.f32bpb.ExtBPB32), FAT32_FSTYPE)) {
	extbpbp = &bs->u.f32bpb.ExtBPB32;
	sb.fatmask = FAT32_MASK;
  } else if (recognize_extbpb(&bs->u.f32bpb.ExtBPB32, FAT_FSTYPE)) {
	extbpbp = &bs->u.f32bpb.ExtBPB32;
  }
  /* it didn't work; then try FAT12/FAT16, at a different place */
  else   if (recognize_extbpb(&bs->u.ExtBPB, FAT16_FSTYPE)) {
	extbpbp = &bs->u.ExtBPB;
	sb.fatmask = FAT16_MASK;
  } else if (recognize_extbpb(&bs->u.ExtBPB, FAT12_FSTYPE)) {
	extbpbp = &bs->u.ExtBPB;
	sb.fatmask = FAT12_MASK;
  } else if (recognize_extbpb(&bs->u.ExtBPB, FAT_FSTYPE)) {
	extbpbp = &bs->u.ExtBPB;
  } else if (bs->u.f32bpb.ExtBPB32.exBootSignature==EXBOOTSIG
          || bs->u.f32bpb.ExtBPB32.exBootSignature==EXBOOTSIG_ALT ) {
	/* we found a signature for an extended BPB at the
	 * right place for a FAT32 file system, but the file
	 * system identifier is unknown...
	 * more robust handling would be to check isprint(exFileSysType[])
	 */
	DBGprintf(("mounting failed: found FAT32-like extended BPB, "
		"but with unkown FStype: <%.8s>\n",
		bs->u.f32bpb.ExtBPB32.exFileSysType));
	return(EINVAL);
  } else if (bs->u.ExtBPB.exBootSignature==EXBOOTSIG
          || bs->u.ExtBPB.exBootSignature==EXBOOTSIG_ALT ) {
	DBGprintf(("mounting failed: found extended BPB, "
		"but with unkown FStype: <%.8s>\n",
		bs->u.ExtBPB.exFileSysType));
	return(EINVAL);
  } else {
	/* we did not found the signature for an extended BPB...
	 */
	DBGprintf(("warning while mounting: missing extended BPB\n"));
/* DO NOT
	return(EINVAL);
 */
  }
  return OK;
}

/*===========================================================================*
 *				check_bootsector			     *
 *===========================================================================*/
PRIVATE int check_bootsector(
  struct fat_bootsector *bs

  )		/* of this proposed type ?*/
{
  /* Sanity checks... */
  if (bs->bpbBytesPerSec[0]!=0 || bs->bpbBytesPerSec[1]!=2) {
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
PRIVATE int find_basic_sizes(
  struct fat_bootsector *bs
  )		/* of this proposed type ?*/
{
  /* Fill the superblock */
  int i;
  unsigned long bit;

  sb.bpblock = sb.bpsector = get_le16(bs->bpbBytesPerSec);
  bit = 1;
  for (i=0; i<16; bit<<=1,++i) {
	if (bit & sb.bpsector) {
		if (bit ^ sb.bpsector) {
			DBGprintf(("mounting failed: "
				"sector size must be power of 2\n"));
			return(EINVAL);
		}
		sb.bnshift = sb.snshift = i;
		break;
	}
  }
  sb.brelmask = sb.srelmask = sb.bpsector-1;
  sb.blkpcluster = sb.secpcluster = bs->bpbSecPerClust;
  bit = 1;
  for (i=0; i<8; bit<<=1,++i) {
	if (bit & sb.secpcluster) {
		assert(! (bit ^ sb.secpcluster));
		sb.cnshift = sb.snshift + i;
		break;
	}
  }
  if (sb.cnshift>15) {
	DBGprintf(("mounting failed: clusters too big (%d KiB)\n",
		1<<(sb.cnshift-10)));
	return(EINVAL);
  }
  sb.bpcluster = sb.secpcluster * sb.bpsector;
  assert(sb.bpcluster == (1<<sb.cnshift)); /* paranoia */
  sb.crelmask = sb.bpcluster-1;
  assert(sizeof(struct fat_direntry) == 32); /* paranoia */
  assert(sizeof(struct fat_lfnentry) == 32);
  assert(DIR_ENTRY_SIZE == 32);
  sb.depsec = sb.depblk = sb.bpsector / DIR_ENTRY_SIZE;
  sb.depclust = sb.depsec << sb.secpcluster;
  return OK;
}

/*===========================================================================*
 *				find_sector_counts			     *
 *===========================================================================*/
PRIVATE int find_sector_counts(
  struct fat_bootsector *bs,
  struct fat_extbpb *extbpbp
  )		/* of this proposed type ?*/
{
  sector_t systemarea;

  sb.nFATs = bs->bpbFATs;

  sb.resCnt   = sb.resSiz   = get_le16(bs->bpbResSectors);
  if ( (sb.secpfat = get_le16(bs->bpbFATsecs)) == 0) {
	if (extbpbp != &bs->u.f32bpb.ExtBPB32)
		DBGprintf(("warning: mounting FAT32 without extended BPB\n"));
	sb.secpfat = get_le32(bs->u.f32bpb.bpbBigFATsecs);
  }
  sb.blkpfat = sb.secpfat;
  if (INT_MAX<65535 && get_le16(bs->bpbRootDirEnts) > (unsigned)INT_MAX) {
	DBGprintf(("mounting failed: too much root dir entries (%u)\n",
		get_le16(bs->bpbRootDirEnts)));
	return(EINVAL);
  }
  sb.rootEntries = get_le16(bs->bpbRootDirEnts);
  sb.rootCnt = sb.rootSiz = (sb.rootEntries+sb.depsec-1) / sb.depsec;
#if 0
  if (extbpbp == &bs->u.f32bpb.ExtBPB32) {
	DBGprintf(("warning: FAT32 with fixed root directory!?\n"));
	/* return(EINVAL); */
  }
#endif
  if ( (sb.totalSecs = get_le16(bs->bpbSectors)) == 0) {
	sb.totalSecs = get_le32(bs->bpbHugeSectors);
  }
  sb.totalSiz = sb.totalSecs;
  sb.fatsCnt  = sb.fatsSiz  = sb.nFATs * sb.secpfat;
  systemarea = sb.resCnt + sb.fatsCnt + sb.rootCnt;
  if ( (systemarea + sb.secpcluster) >= sb.totalSecs) {
	DBGprintf(("mounting failed: incoherent sizes, "
		"sysarea=%lu total=%lu\n",
		(unsigned long)systemarea, (unsigned long)sb.totalSecs));
	return(EINVAL);
  }
  sb.clustCnt = sb.clustSiz = sb.totalSecs-systemarea;
  sb.maxFilesize = (LONG_MAX>>sb.snshift) < sb.clustCnt
	? LONG_MAX : sb.clustCnt<<sb.snshift;

  sb.resSec   = sb.resBlk   = 0;
  sb.fatSec   = sb.fatBlk   = sb.resCnt;
  sb.rootSec  = sb.rootBlk  = sb.fatSec + sb.fatsCnt;
  sb.clustSec = sb.clustBlk = sb.rootSec + sb.rootCnt;
  assert( (sb.clustSec + sb.clustCnt) <= sb.totalSecs);
  return OK;
}

/*===========================================================================*
 *				check_fatsize				     *
 *===========================================================================*/
PRIVATE int check_fatsize(
  struct fat_bootsector *bs,
  struct fat_extbpb *extbpbp)		/* of this proposed type ?*/
{
  /* Final check about FAT sizes */
  int nibble_per_clust;	/* nibble (half-octet) per cluster: 3, 4 or 8 */
  sector_t slack;	/* number of useless sector in each FAT */

  sb.maxClust = (sb.clustCnt / sb.secpcluster) + 1;
#define COMPUTE_SLACK(usedFATsize)	\
	sb.secpfat - ( (usedFATsize) -1 + sb.bpsector ) / sb.bpsector

  if (sb.fatmask == FAT12_MASK && ! FS_IS_FAT12(sb.maxClust)
   || sb.fatmask == FAT16_MASK && ! FS_IS_FAT16(sb.maxClust)
   || sb.fatmask == FAT32_MASK && ! FS_IS_FAT32(sb.maxClust)) {
	DBGprintf(("mounting failed: extended BPB says "
		"FStype=<%.8s> but maxClust=%lu\n",
		extbpbp->exFileSysType, (unsigned long)sb.maxClust));
	return(EINVAL);
  }
  if (FS_IS_FAT12(sb.maxClust)) {
	sb.fatmask = FAT12_MASK;	sb.eofmask = CLUSTMASK_EOF12;
	assert(!FS_IS_FAT16(sb.maxClust));	/* paranoia */
	assert(!FS_IS_FAT32(sb.maxClust));
	if ( (sb.maxClust*3+2)/2 >= sb.secpfat*sb.bpsector) {
		DBGprintf(("mounting failed: "
			"FAT12 with only %ld sectors/FAT but %ld clusters\n",
			(long)sb.secpfat, (long) sb.maxClust));
		return(EINVAL);
	}
	nibble_per_clust = 3;
	slack = COMPUTE_SLACK((sb.maxClust*3+2)/2);
/* PLUS virtual methods... +++ */
  }
  else if (FS_IS_FAT16(sb.maxClust)) {
	sb.fatmask = FAT16_MASK;	sb.eofmask = CLUSTMASK_EOF16;
	assert(!FS_IS_FAT12(sb.maxClust));	/* paranoia */
	assert(!FS_IS_FAT32(sb.maxClust));
	if (sb.maxClust*2 >= sb.secpfat*sb.bpsector) {
		DBGprintf(("mounting failed: "
			"FAT16 with only %ld sectors/FAT but %ld clusters\n",
			(long)sb.secpfat, (long) sb.maxClust));
		return(EINVAL);
	}
	nibble_per_clust = 4;
	slack = COMPUTE_SLACK(sb.maxClust*2);
  }
  else if (FS_IS_FAT12(sb.maxClust)) {
	sb.fatmask = FAT32_MASK;	sb.eofmask = CLUSTMASK_EOF32;
	assert(!FS_IS_FAT12(sb.maxClust));	/* paranoia */
	assert(!FS_IS_FAT16(sb.maxClust));
	if (sb.maxClust*4 >= sb.secpfat*sb.bpsector) {
		DBGprintf(("mounting failed: "
			"FAT32 with only %ld sectors/FAT but %ld clusters\n",
			(long)sb.secpfat, (long) sb.maxClust));
		return(EINVAL);
	}
	nibble_per_clust = 8;
	slack = COMPUTE_SLACK(sb.maxClust*4);
  }
  assert(sb.fatmask != 0);

  DBGprintf(("FATfs: mounting on %s, %ld clusters, %d rootdir entries\n",
		fs_dev_label, (long)sb.maxClust-1, sb.rootEntries));
  DBGprintf(("FATfs: FATs are %ld sectors, slack=%ld\n",
		(long)sb.secpfat, (long)slack));

  return OK;
}

/*===========================================================================*
 *				do_readsuper				     *
 *===========================================================================*/
PUBLIC int do_readsuper(void)
{
/* This function reads the superblock of the partition, builds the root inode
 * and sends back the details of them.
 *
 * CHECKME if the following is still relevant?
 * Note, that the FS process does not know the index of the vmnt object which
 * refers to it, whenever the pathname lookup leaves a partition an
 * ELEAVEMOUNT error is transferred back so that the VFS knows that it has
 * to find the vnode on which this FS process' partition is mounted on.
 */
  int r;
  struct inode *root_ip;
  cp_grant_id_t label_gid;
  size_t label_len;
  struct fat_extbpb *extbpbp;
  off_t rootDirSize;
  static struct fat_bootsector *bs;  

  STATICINIT(bs, SECTOR_SIZE);	/* allocated once, made contiguous */

  DBGprintf(("FATfs: readsuper (dev %x, flags %x)\n",
	(dev_t) m_in.REQ_DEV, m_in.REQ_FLAGS));

  if (m_in.REQ_FLAGS & REQ_ISROOT) {
	printf("FATfs: attempt to mount as root device\n");
	return EINVAL;
  }

  read_only = !!(m_in.REQ_FLAGS & REQ_RDONLY);
  dev = m_in.REQ_DEV;
  if (dev == NO_DEV)
	panic("request for mounting FAT fs on NO_DEV");

  label_gid = (cp_grant_id_t) m_in.REQ_GRANT;
  label_len = (size_t) m_in.REQ_PATH_LEN;

  if (label_len >= sizeof(fs_dev_label))
	return(EINVAL);
  memset(fs_dev_label, 0, sizeof(fs_dev_label));

  r = sys_safecopyfrom(m_in.m_source, label_gid, (vir_bytes) 0,
		       (vir_bytes) fs_dev_label, label_len, D);
  if (r != OK) {
	printf("FAT`%s:%d safecopyfrom failed: %d\n", __FILE__, __LINE__, r);
	return(EINVAL);
  }
  if (strlen(fs_dev_label) > sizeof(fs_dev_label)-1)
	/* label passed by VFS was not 0-terminated: abort.
	 * note that all memset+strlen dance can be changed to use strnlen
	 * memchr(,'\0',) can do the trick too, but is less clear
	 */
	return(EINVAL);

#ifndef DS_DRIVER_UP
  r = ds_retrieve_label_num(fs_dev_label, (unsigned long*)&driver_e);
#else
  r = ds_retrieve_label_endpt(fs_dev_label, &driver_e);
#endif
  if (r != OK) {
	printf("FATfs %s:%d ds_retrieve_label_endpt failed for '%s': %d\n",
		__FILE__, __LINE__, fs_dev_label, r);
	return(EINVAL);
  }

  /* Open the device the file system lives on. */
  if (dev_open(driver_e, dev, driver_e,
	       read_only ? R_BIT : (R_BIT|W_BIT) ) != OK) {
	return(EINVAL);
  }

  /* Fill in the boot sector. */
  assert(sizeof *bs == SECTOR_SIZE);	/* paranoia */
  r = seqblock_dev_io(DEV_READ_S, bs, cvu64(0), SECTOR_SIZE);
  if (r != SECTOR_SIZE) 
	return(EINVAL);

/* Now check the various fields and fill the superblock
 * structure which will keep them available for future use.
 */
  extbpbp = NULL;
  if ( (r = recognize_bootsector(bs, extbpbp)) != OK
    || (r = check_bootsector(bs)) != OK
    || (r = find_basic_sizes(bs)) != OK
    || (r = find_sector_counts(bs, extbpbp)) != OK
    || (r = check_fatsize(bs, extbpbp)) != OK )
	return(r);

/*FIXME sb.rootCluster et le reste des checks FAT32 (vers0 etc) */

  sb.freeClustValid = sb.freeClust = sb.nextClust = 0;

#if 0
	if (version == V2)
		sp->s_block_size = _STATIC_BLOCK_SIZE;
	if (sp->s_block_size < _MIN_BLOCK_SIZE) {
		return EINVAL;
	}
	sp->s_inodes_per_block = V2_INODES_PER_BLOCK(sp->s_block_size);
	sp->s_ndzones = V2_NR_DZONES;
	sp->s_nindirs = V2_INDIRECTS(sp->s_block_size);

  if (sp->s_block_size < _MIN_BLOCK_SIZE) 
	return(EINVAL);
  
  if ((sp->s_block_size % 512) != 0) 
	return(EINVAL);
  sp->s_isearch = 0;		/* inode searches initially start at 0 */
  sp->s_zsearch = 0;		/* zone searches initially start at 0 */
  sp->s_version = version;
  sp->s_native  = native;

  set_blocksize(superblock.s_block_size);
  
  /* Get the root inode of the mounted file system. */
  if( (root_ip = get_inode(fs_dev, ROOT_INODE)) == NULL)  {
	  printf("MFS: couldn't get root inode\n");
	  superblock.s_dev = NO_DEV;
	  dev_close(driver_e, fs_dev);
	  return(EINVAL);
  }
  
  if(root_ip->i_mode == 0) {
	  printf("%s:%d zero mode for root inode?\n", __FILE__, __LINE__);
	  put_inode(root_ip);
	  superblock.s_dev = NO_DEV;
	  dev_close(driver_e, fs_dev);
	  return(EINVAL);
  }

  superblock.s_rd_only = readonly;
  superblock.s_is_root = isroot;
  
*** now the HGFS view ***

  init_dentry();
  ino = init_inode();

  m_out.RES_INODE_NR = INODE_NR(ino);
  m_out.RES_MODE = get_mode(ino, attr.a_mode);
#endif

/* FIXME if FAT32... */
  rootDirSize = (long)sb.rootEntries * DIR_ENTRY_SIZE;

  root_ip = init_inode();

  m_out.RES_INODE_NR = INODE_NR(root_ip);
  m_out.RES_MODE = /*FIXME!!! get_mode(ino, attr.a_mode)*/ 0 ;
  assert(rootDirSize <= 0xffffffff);
  m_out.RES_FILE_SIZE_HI = 0;
  m_out.RES_FILE_SIZE_LO = rootDirSize;
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

  DBGprintf(("FATfs: do_unmount\n"));

#if 1
  /* Decrease the reference count of the root inode. */
  if ((root_ip = fetch_inode(ROOT_INODE_NR)) == NULL) {
	panic("couldn't find root inode while unmounting\n");
	return EINVAL;
  }

  put_inode(root_ip);

  /* There should not be any referenced inodes anymore now. */
  if (have_used_inode())
	printf("in-use inodes left at unmount time!\n");
 	/* this is NOT clean unmounting! */
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
 * we should mark FAT[1] as "umounted clean" (after flush)
 */

  /* force any cached blocks out of memory */
/* CHECKME: perhaps better to call explicitely
	flush_inodes()+flushall() +? invalidate()
 */
  do_sync();

  /* Close the device the file system lives on. */
  dev_close(driver_e, dev);

  /* Finish off the unmount. */
  dev = NO_DEV;
  state = UNMOUNTED;

  return OK;
}
