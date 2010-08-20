/* This file contains mount and unmount functionality.
 *
 * The entry points into this file are:
 *   do_readsuper	perform the READSUPER file system call
 *   do_unmount		perform the UNMOUNT file system call
 *
 * Auteur: Antoine Leca, aout 2010.
 * Slavishly copied from ../hgfs/mount.c (D.C. van Moolenbroek)
 * Updated:
 */

#define _POSIX_SOURCE 1
#define _MINIX 1
#define _SYSTEM 1		/* for negative error values */

#include <assert.h>
#include <errno.h>

#include <minix/config.h>
#include <minix/const.h>

#include <minix/vfsif.h>

#include "const.h"
#include "type.h"
#include "proto.h"
#include "glo.h"

/*
#include "inc.h"
 */

/*===========================================================================*
 *				do_readsuper				     *
 *===========================================================================*/
PUBLIC int do_readsuper()
{
/* Mount the file system.
 */
#if 0
  char path[PATH_MAX];
  struct inode *ino;
  struct hgfs_attr attr;
  int r;

  dprintf(("HGFS: readsuper (dev %x, flags %x)\n",
	(dev_t) m_in.REQ_DEV, m_in.REQ_FLAGS));

  if (m_in.REQ_FLAGS & REQ_ISROOT) {
	printf("HGFS: attempt to mount as root device\n");

	return EINVAL;
  }

  state.read_only = !!(m_in.REQ_FLAGS & REQ_RDONLY);
  state.dev = m_in.REQ_DEV;

  init_dentry();
  ino = init_inode();

  attr.a_mask = HGFS_ATTR_MODE | HGFS_ATTR_SIZE;

  /* We cannot continue if we fail to get the properties of the root inode at
   * all, because we cannot guess the details of the root node to return to
   * VFS. Print a (hopefully) helpful error message, and abort the mount.
   */
  if ((r = verify_inode(ino, path, &attr)) != OK) {
	if (r == EAGAIN)
		printf("HGFS: shared folders disabled\n");
	else if (opt.prefix[0] && (r == ENOENT || r == EACCES))
		printf("HGFS: unable to access the given prefix directory\n");
	else
		printf("HGFS: unable to access shared folders\n");

	return r;
  }

  m_out.RES_INODE_NR = INODE_NR(ino);
  m_out.RES_MODE = get_mode(ino, attr.a_mode);
  m_out.RES_FILE_SIZE_HI = ex64hi(attr.a_size);
  m_out.RES_FILE_SIZE_LO = ex64lo(attr.a_size);
  m_out.RES_UID = opt.uid;
  m_out.RES_GID = opt.gid;
  m_out.RES_DEV = NO_DEV;

  state.mounted = TRUE;
#endif

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

#if 0
  dprintf(("HGFS: do_unmount\n"));

  /* Decrease the reference count of the root inode. */
  if ((ino = find_inode(ROOT_INODE_NR)) == NULL)
	return EINVAL;

  put_inode(ino);

  /* There should not be any referenced inodes anymore now. */
  if (have_used_inode())
	printf("HGFS: in-use inodes left at unmount time!\n");

  state.mounted = FALSE;
#endif

  return OK;
}
