/* This file contains file metadata retrieval and manipulation routines.
 *
 * The entry points into this file are:
 *   get_mode		return a file's mode
 *   do_stat		perform the STAT file system request
 *   do_chmod		perform the CHMOD file system request
 *   do_utime		perform the UTIME file system request
 *
 * Auteur: Antoine Leca, aout 2010.
 * Slavishly copied from ../hgfs/stat.c (D.C. van Moolenbroek)
 * Updated:
 */

#include "inc.h"

#include <sys/stat.h>

#include <minix/safecopies.h>
#include <minix/syslib.h>	/* sys_safecopies{from,to} */

	#include "inode.h"

/*===========================================================================*
 *				get_mode				     *
 *===========================================================================*/
PUBLIC mode_t get_mode(ino, mode)
struct inode *ino;
int mode;
{
/* Return the mode for an inode, given the inode and the HGFS retrieved mode.
 */

  mode &= S_IRWXU;
  mode = mode | (mode >> 3) | (mode >> 6);

  if (IS_DIR(ino))
	mode = S_IFDIR | (mode & use_dir_mask);
  else
	mode = S_IFREG | (mode & use_file_mask);

  if (read_only)
	mode &= ~(S_IWUSR|S_IWGRP|S_IWOTH);

  return mode;
}

/*===========================================================================*
 *				do_stat					     *
 *===========================================================================*/
PUBLIC int do_stat()
{
/* Retrieve inode statistics.
 */
  int r;
  struct stat stat;
  ino_t ino_nr;
  struct inode *ino;
#if 0
  struct hgfs_attr attr;
#endif
  char path[PATH_MAX];

  ino_nr = m_in.REQ_INODE_NR;

  /* Don't increase the inode refcount: it's already open anyway */
  if ((ino = fetch_inode(ino_nr)) == NULL)
	return EINVAL;

#if 0
  attr.a_mask = HGFS_ATTR_MODE | HGFS_ATTR_SIZE | HGFS_ATTR_ATIME |
		HGFS_ATTR_MTIME | HGFS_ATTR_CTIME;

  if ((r = verify_inode(ino, path, &attr)) != OK)
	return r;
#endif

  stat.st_dev = dev;
  stat.st_ino = ino_nr;
  stat.st_mode = get_mode(ino, /*FIXME attr.a_mode*/ 0 );
  stat.st_uid = use_uid;
  stat.st_gid = use_gid;
  stat.st_rdev = NO_DEV;
#if 0
  stat.st_size = ex64hi(attr.a_size) ? ULONG_MAX : ex64lo(attr.a_size);
#else
  stat.st_size = ino->i_size;
#endif
#if 0
  stat.st_atime = attr.a_atime;
  stat.st_mtime = attr.a_mtime;
  stat.st_ctime = attr.a_ctime;
#endif

  /* We could make this more accurate by iterating over directory inodes'
   * children, counting how many of those are directories as well.
   * It's just not worth it.
   */
  stat.st_nlink = 0;
  if (ino->i_parent != NULL) stat.st_nlink++;
  if (IS_DIR(ino)) {
	stat.st_nlink++;
	if (HAS_CHILDREN(ino)) stat.st_nlink++;
  }

  return sys_safecopyto(m_in.m_source, m_in.REQ_GRANT, 0,
	(vir_bytes) &stat, sizeof(stat), D);
}

/*===========================================================================*
 *				do_chmod				     *
 *===========================================================================*/
PUBLIC int do_chmod()
{
/* Change file mode.
 */
  struct inode *ino;
#if 0
  char path[PATH_MAX];
  struct hgfs_attr attr;
#endif
  int r;

  if (read_only)
	return EROFS;

  if ((ino = fetch_inode(m_in.REQ_INODE_NR)) == NULL)
	return EINVAL;

#if 0
  if ((r = verify_inode(ino, path, NULL)) != OK)
	return r;

  /* Set the new file mode. */
  attr.a_mask = HGFS_ATTR_MODE;
  attr.a_mode = m_in.REQ_MODE; /* no need to convert in this direction */

  if ((r = hgfs_setattr(path, &attr)) != OK)
	return r;

  /* We have no idea what really happened. Query for the mode again. */
  if ((r = verify_path(path, ino, &attr, NULL)) != OK)
	return r;
#endif

  m_out.RES_MODE = get_mode(ino, /*FIXME attr.a_mode*/ 0 );

  return OK;
}

/*===========================================================================*
 *				do_utime				     *
 *===========================================================================*/
PUBLIC int do_utime()
{
/* Set file times.
 */
  struct inode *ino;
#if 0
  char path[PATH_MAX];
  struct hgfs_attr attr;
#endif
  int r;

  if (read_only)
	return EROFS;

  if ((ino = fetch_inode(m_in.REQ_INODE_NR)) == NULL)
	return EINVAL;

#if 0
  if ((r = verify_inode(ino, path, NULL)) != OK)
	return r;

  attr.a_mask = HGFS_ATTR_ATIME | HGFS_ATTR_MTIME;
  attr.a_atime = m_in.REQ_ACTIME;
  attr.a_mtime = m_in.REQ_MODTIME;

  return hgfs_setattr(path, &attr);
#endif
  return EINVAL;
}
