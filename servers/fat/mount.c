/* This file contains mount and unmount functionality.
 *
 * The entry points into this file are:
 *   do_readsuper	perform the READSUPER file system request
 *   do_unmount		perform the UNMOUNT file system request
 *
 * Auteur: Antoine Leca, aout 2010.
 * Updated:
 */

#define _POSIX_SOURCE 1
#define _MINIX 1

#define _SYSTEM 1		/* for negative error values */
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>

#include <minix/config.h>
#include <minix/const.h>
#include <minix/type.h>
#include <minix/ipc.h>

#include <minix/vfsif.h>
#include <minix/safecopies.h>
#include <minix/syslib.h>	/* sys_safecopies{from,to} */

#include <minix/ds.h>

#include "fat.h"

#include "const.h"
#include "type.h"
#include "proto.h"
#include "glo.h"

#if DEBUG
#define DBGprintf(x) printf x
#else
#define DBGprintf(x)
#endif

/*
#include "inc.h"
 */

/*===========================================================================*
 *				do_readsuper				     *
 *===========================================================================*/
PUBLIC int do_readsuper()
{
/* This function reads the superblock of the partition, gets the root inode
 * and sends back the details of them. Note, that the FS process does not
 * know the index of the vmnt object which refers to it, whenever the pathname 
 * lookup leaves a partition an ELEAVEMOUNT error is transferred back 
 * so that the VFS knows that it has to find the vnode on which this FS 
 * process' partition is mounted on.
 */
  int r;
  endpoint_t driver_e;
  struct inode *root_ip;
  cp_grant_id_t label_gid;
  size_t label_len;

  DBGprintf(("FATfs: readsuper (dev %x, flags %x)\n",
	(dev_t) m_in.REQ_DEV, m_in.REQ_FLAGS));

  if (m_in.REQ_FLAGS & REQ_ISROOT) {
	printf("FATfs: attempt to mount as root device\n");

	return EINVAL;
  }

  read_only = !!(m_in.REQ_FLAGS & REQ_RDONLY);
  dev = m_in.REQ_DEV;

  label_gid = (cp_grant_id_t) m_in.REQ_GRANT;
  label_len = (size_t) m_in.REQ_PATH_LEN;

  if (label_len >= sizeof(fs_dev_label))
	return(EINVAL);
  memset(fs_dev_label, 0, sizeof(fs_dev_label));

  r = sys_safecopyfrom(m_in.m_source, label_gid, (vir_bytes) 0,
		       (vir_bytes) fs_dev_label, label_len, D);
  if (r != OK) {
	printf("FATfs %s:%d safecopyfrom failed: %d\n", __FILE__, __LINE__, r);
	return(EINVAL);
  }
  if (strlen(fs_dev_label) > sizeof(fs_dev_label)-1)
	/* label passed by VFS was not 0-terminated: abort.
	 * note that all memset+strlen dance can be changed to use strnlen
	 * memchr(,'\0',) can do the trick too, but is less clear
	 */
	return(EINVAL);

#ifndef DS_DRIVER_UP
  r = ds_retrieve_label_num(fs_dev_label, (unsigned long*)&driver_ep);
#else
  r = ds_retrieve_label_endpt(fs_dev_label, &driver_ep);
#endif
  if (r != OK) {
	printf("FATfs %s:%d ds_retrieve_label_endpt failed for '%s': %d\n",
		__FILE__, __LINE__, fs_dev_label, r);
	return(EINVAL);
  }

  /* Open the device the file system lives on. */
  if (dev_open(driver_ep, dev, driver_ep,
  	       read_only ? R_BIT : (R_BIT|W_BIT) ) != OK) {
        return(EINVAL);
  }

#if 0
  /* Fill in the super block. */
  superblock.s_dev = fs_dev;	/* read_super() needs to know which dev */
  r = read_super(&superblock);

{
/* Read a superblock. */
  dev_t dev;
  unsigned int magic;
  int version, native, r;
  static char *sbbuf;
  block_t offset;

  STATICINIT(sbbuf, _MIN_BLOCK_SIZE);

  dev = sp->s_dev;		/* save device (will be overwritten by copy) */
  if (dev == NO_DEV)
  	panic("request for super_block of NO_DEV");
  
  r = block_dev_io(MFS_DEV_READ, dev, SELF_E, sbbuf, cvu64(SUPER_BLOCK_BYTES),
  		   _MIN_BLOCK_SIZE);
  if (r != _MIN_BLOCK_SIZE) 
  	return(EINVAL);
  
  memcpy(sp, sbbuf, sizeof(*sp));
  sp->s_dev = NO_DEV;		/* restore later */
  magic = sp->s_magic;		/* determines file system type */

  /* Get file system version and type. */
  if (magic == SUPER_MAGIC || magic == conv2(BYTE_SWAP, SUPER_MAGIC)) {
	version = V1;
	native  = (magic == SUPER_MAGIC);
  } else if (magic == SUPER_V2 || magic == conv2(BYTE_SWAP, SUPER_V2)) {
	version = V2;
	native  = (magic == SUPER_V2);
  } else if (magic == SUPER_V3) {
	version = V3;
  	native = 1;
  } else {
	return(EINVAL);
  }

  /* If the super block has the wrong byte order, swap the fields; the magic
   * number doesn't need conversion. */
  sp->s_ninodes =           (ino_t) conv4(native, (int) sp->s_ninodes);
  sp->s_nzones =          (zone1_t) conv2(native, (int) sp->s_nzones);
  sp->s_imap_blocks =       (short) conv2(native, (int) sp->s_imap_blocks);
  sp->s_zmap_blocks =       (short) conv2(native, (int) sp->s_zmap_blocks);
  sp->s_firstdatazone_old =(zone1_t)conv2(native,(int)sp->s_firstdatazone_old);
  sp->s_log_zone_size =     (short) conv2(native, (int) sp->s_log_zone_size);
  sp->s_max_size =          (off_t) conv4(native, sp->s_max_size);
  sp->s_zones =             (zone_t)conv4(native, sp->s_zones);

  /* In V1, the device size was kept in a short, s_nzones, which limited
   * devices to 32K zones.  For V2, it was decided to keep the size as a
   * long.  However, just changing s_nzones to a long would not work, since
   * then the position of s_magic in the super block would not be the same
   * in V1 and V2 file systems, and there would be no way to tell whether
   * a newly mounted file system was V1 or V2.  The solution was to introduce
   * a new variable, s_zones, and copy the size there.
   *
   * Calculate some other numbers that depend on the version here too, to
   * hide some of the differences.
   */
  if (version == V1) {
  	sp->s_block_size = _STATIC_BLOCK_SIZE;
	sp->s_zones = (zone_t) sp->s_nzones;	/* only V1 needs this copy */
	sp->s_inodes_per_block = V1_INODES_PER_BLOCK;
	sp->s_ndzones = V1_NR_DZONES;
	sp->s_nindirs = V1_INDIRECTS;
  } else {
  	if (version == V2)
  		sp->s_block_size = _STATIC_BLOCK_SIZE;
  	if (sp->s_block_size < _MIN_BLOCK_SIZE) {
  		return EINVAL;
	}
	sp->s_inodes_per_block = V2_INODES_PER_BLOCK(sp->s_block_size);
	sp->s_ndzones = V2_NR_DZONES;
	sp->s_nindirs = V2_INDIRECTS(sp->s_block_size);
  }

  /* For even larger disks, a similar problem occurs with s_firstdatazone.
   * If the on-disk field contains zero, we assume that the value was too
   * large to fit, and compute it on the fly.
   */
  if (sp->s_firstdatazone_old == 0) {
	offset = START_BLOCK + sp->s_imap_blocks + sp->s_zmap_blocks;
	offset += (sp->s_ninodes + sp->s_inodes_per_block - 1) /
		sp->s_inodes_per_block;

	sp->s_firstdatazone = (offset + (1 << sp->s_log_zone_size) - 1) >>
		sp->s_log_zone_size;
  } else {
	sp->s_firstdatazone = (zone_t) sp->s_firstdatazone_old;
  }

  if (sp->s_block_size < _MIN_BLOCK_SIZE) 
  	return(EINVAL);
  
  if ((sp->s_block_size % 512) != 0) 
  	return(EINVAL);
  
  if (SUPER_SIZE > sp->s_block_size) 
  	return(EINVAL);
  
  if ((sp->s_block_size % V2_INODE_SIZE) != 0 ||
     (sp->s_block_size % V1_INODE_SIZE) != 0) {
  	return(EINVAL);
  }

  /* Limit s_max_size to LONG_MAX */
  if ((unsigned long)sp->s_max_size > LONG_MAX) 
	sp->s_max_size = LONG_MAX;

  sp->s_isearch = 0;		/* inode searches initially start at 0 */
  sp->s_zsearch = 0;		/* zone searches initially start at 0 */
  sp->s_version = version;
  sp->s_native  = native;

  /* Make a few basic checks to see if super block looks reasonable. */
  if (sp->s_imap_blocks < 1 || sp->s_zmap_blocks < 1
				|| sp->s_ninodes < 1 || sp->s_zones < 1
				|| sp->s_firstdatazone <= 4
				|| sp->s_firstdatazone >= sp->s_zones
				|| (unsigned) sp->s_log_zone_size > 4) {
  	printf("not enough imap or zone map blocks, \n");
  	printf("or not enough inodes, or not enough zones, \n"
  		"or invalid first data zone, or zone size too large\n");
	return(EINVAL);
  }
  sp->s_dev = dev;		/* restore device number */
  return(OK);
}


  /* Is it recognized as a Minix filesystem? */
  if (r != OK) {
	superblock.s_dev = NO_DEV;
  	dev_close(driver_e, fs_dev);
	return(r);
  }

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


/* Mount the file system.
 */
/*
  char path[PATH_MAX];
  struct inode *ino;
  struct hgfs_attr attr; */

  init_dentry();
  ino = init_inode();

  attr.a_mask = HGFS_ATTR_MODE | HGFS_ATTR_SIZE;

  /* We cannot continue if we fail to get the properties of the root inode at
   * all, because we cannot guess the details of the root node to return to
   * VFS. Print a (hopefully) helpful error message, and abort the mount.
   */
  if ((r = verify_inode(ino, path, &attr)) != OK) {
	if (r == EAGAIN)
		printf("FATfs: shared folders disabled\n");
	else if (opt.prefix[0] && (r == ENOENT || r == EACCES))
		printf("FATfs: unable to access the given prefix directory\n");
	else
		printf("FATfs: unable to access shared folders\n");

	return r;
  }

  m_out.RES_INODE_NR = INODE_NR(ino);
  m_out.RES_MODE = get_mode(ino, attr.a_mode);
  m_out.RES_FILE_SIZE_HI = ex64hi(attr.a_size);
  m_out.RES_FILE_SIZE_LO = ex64lo(attr.a_size);
#endif
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
  struct inode *ino;

  DBGprintf(("FATfs: do_unmount\n"));

#if 0
  /* Decrease the reference count of the root inode. */
  if ((ino = find_inode(ROOT_INODE_NR)) == NULL)
	return EINVAL;

  put_inode(ino);

  /* There should not be any referenced inodes anymore now. */
  if (have_used_inode())
	printf("FATfs: in-use inodes left at unmount time!\n");

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

  /* force any cached blocks out of memory */
  (void) fs_sync();
#endif

  /* Close the device the file system lives on. */
  dev_close(driver_ep, dev);

  /* Finish off the unmount. */
  dev = NO_DEV;
  state = UNMOUNTED;

  return OK;
}
