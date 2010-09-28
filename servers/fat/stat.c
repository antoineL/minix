/* This file contains file metadata retrieval and manipulation routines.
 *
 * The entry points into this file are:
 *   get_mode		return a file's mode
 *   do_stat		perform the STAT file system request
 *   do_chmod		perform the CHMOD file system request
 *   do_utime		perform the UTIME file system request
 *
 * Warning: this code is not reentrant (use static local variables, without mutex)
 *
 * Auteur: Antoine Leca, aout 2010.
 * Updated:
 */

#include "inc.h"

#include <string.h>
#include <sys/stat.h>

#include <minix/safecopies.h>
#include <minix/syslib.h>	/* sys_safecopies{from,to} */

/* Private functions:
 *   get_mask		?
 */
/* useful only with symlinks... dropped ATM
FORWARD _PROTOTYPE( int ltraverse, (struct inode *rip, char *suffix)	);
 */

/*===========================================================================*
 *				get_mode				     *
 *===========================================================================*/
PUBLIC mode_t get_mode(struct inode *rip)
{
/* Return the mode for an inode, given the direntry embeeded in the inode.
 */
  int mode;

  mode = S_IRUSR | S_IXUSR
       | (rip->i_Attributes&ATTR_READONLY || read_only ? 0 : S_IWUSR);
  mode = mode | (mode >> 3) | (mode >> 6);

  if (IS_DIR(rip))
	mode = S_IFDIR | (mode & use_dir_mask);
  else
	mode = S_IFREG | (mode & use_file_mask);

  return(mode);
}

/*===========================================================================*
 *				do_stat					     *
 *===========================================================================*/
PUBLIC int do_stat(void)
{
/* Retrieve inode statistics.
 * FIXME: use+update i_?time members
 * FIXME: POSIX: The stat() function shall update any time-related fields
 * (as described in XBD File Times Update ), before writing
 */
  int r;
  struct inode *rip;
  struct fat_direntry *dp;
  ino_t ino_nr;
  struct stat stat;

  ino_nr = m_in.REQ_INODE_NR;

  /* Don't increase the inode refcount: it's already open anyway */
  if ((rip = fetch_inode(ino_nr)) == NULL)
	return(EINVAL);
  dp = & rip->i_direntry;

  memset(&stat, '\0', sizeof stat);	/* Avoid leaking any data */
  stat.st_dev = dev;
  stat.st_ino = ino_nr;
  stat.st_mode = get_mode(rip);
  stat.st_uid = use_uid;
  stat.st_gid = use_gid;
  stat.st_rdev = NO_DEV;	/* do not support block/char. specials */
#if 0
  stat.st_size = ex64hi(rip->i_size) ? ULONG_MAX : ex64lo(rip->i_size);
#elif 0
  stat.st_size = rip->i_size;
#elif 1
  stat.st_size = rip->i_size > LONG_MAX ? LONG_MAX : rip->i_size;
#else
  stat.st_size = IS_DIR(rip) ? 65535 : rip->i_size;
#endif

  if ( rip->i_mtime < TIME_UNKNOWN
    || rip->i_atime < TIME_UNKNOWN
    || rip->i_ctime < TIME_UNKNOWN )
#if 1
	update_times(rip, rip->i_mtime, rip->i_atime, rip->i_ctime);
#else
	update_times(rip, 0, 0, 0);
#endif
  stat.st_mtime = rip->i_mtime;
  stat.st_atime = rip->i_atime;
  stat.st_ctime = rip->i_ctime;

#if 1
  stat.st_nlink = rip->i_flags & I_ORPHAN ? 0 : 1;
#elif 0
  stat.st_nlink = 0;
  if (rip->i_parent != NULL) stat.st_nlink++;
#elif 0
  stat.st_nlink = 0;
  if (rip->i_parent_clust != 0) stat.st_nlink++;
#else
  stat.st_nlink = 0;
  stat.st_nlink++;
#endif
  if (IS_DIR(rip)) {
  /* We could make this more accurate by iterating over directory inodes'
   * children, counting how many of those are directories as well.
   * It's just not worth it.
   */
	stat.st_nlink++;
	if (HAS_CHILDREN(rip)) stat.st_nlink++;
  }

  DBGprintf(("FATfs: stat ino=%lo, @%d, ['%.8s.%.3s'], mode=%o, size=%ld...\n",
	ino_nr, rip->i_index, rip->i_Name, rip->i_Extension, stat.st_mode, stat.st_size));

  return sys_safecopyto(m_in.m_source, m_in.REQ_GRANT, 0,
	(vir_bytes) &stat, sizeof(stat), D);
}

/*===========================================================================*
 *				do_chmod				     *
 *===========================================================================*/
PUBLIC int do_chmod(void)
{
/* Change file mode.
 */
  struct inode *rip;
  int r;

  if (read_only) return(EROFS);	/* paranoia */

  /* Don't increase the inode refcount: it's already open anyway */
  if ((rip = fetch_inode(m_in.REQ_INODE_NR)) == NULL)
	return(EINVAL);

  /* The only thing we can do is to toggle ATTR_READONLY */
  rip->i_Attributes &= ~ATTR_READONLY; /*clear old value */
  if ( ! (m_in.REQ_MODE & S_IWUSR) )
	rip->i_Attributes |= ATTR_READONLY;

  rip->i_ctime = TIME_UPDATED;
  rip->i_flags |= I_DIRTY;	/* inode is thus now dirty */

  /* Recalc it */
  m_out.RES_MODE = rip->i_mode = get_mode(rip);

  return(OK);
}

/*===========================================================================*
 *				do_chown				     *
 *===========================================================================*/
PUBLIC int do_chown(void)
{
/* We are requested to change file owner or group. We cannot actually.
 *
 * Note that there is no ctime field on FAT file systems, so we cannot
 * update it (unless we fake it in inodes[] just to pass the tests...)
 */
  struct inode *rip;
  int r;

  if (read_only) return(EROFS);	/* paranoia */

  /* Don't increase the inode refcount: it's already open anyway */
  if ((rip = fetch_inode(m_in.REQ_INODE_NR)) == NULL)
	return(EINVAL);

  if (m_in.REQ_UID != use_uid && m_in.REQ_UID != (uid_t)-1
   || m_in.REQ_GID != use_gid && m_in.REQ_GID != (gid_t)-1) {
	/* This will not work; however the "correct" error code,
	 * (EINVAL or EPERM) depends on whoever tries (root or not).
	 * And the VFS protocol does not says who is the caller...
	 * An intent to change uid to root is only allowed to
	 * succeed for a root process, so gives EINVAL then.
	 * Any other change is given EPERM, assuming non-root.
	 */
	return(m_in.REQ_UID ? EINVAL : EPERM);
  }

  rip->i_ctime = TIME_UPDATED;
  rip->i_flags |= I_DIRTY;	/* inode is thus now dirty */

  m_out.RES_MODE = rip->i_mode;	/* mode did not change */

  return(OK);
}

/*===========================================================================*
 *				do_utime				     *
 *===========================================================================*/
PUBLIC int do_utime(void)
{
/* Set file times.
 */
  struct inode *rip;
  int r;

  if (read_only) return(EROFS);	/* paranoia */

  /* Don't increase the inode refcount: it's already open anyway */
  if ((rip = fetch_inode(m_in.REQ_INODE_NR)) == NULL)
	return(EINVAL);

  update_times(rip, 
	m_in.REQ_MODTIME < TIME_UNKNOWN ? TIME_UNKNOWN : m_in.REQ_MODTIME, 
	m_in.REQ_ACTIME  < TIME_UNKNOWN ? TIME_UNKNOWN : m_in.REQ_ACTIME, 
	TIME_UPDATED);
#if 0
  if (rip->i_atime )
	rip->i_atime = TIME_UNKNOWN;
  unix2dostime(rip->i_atime, dp->deADate, NULL);

  rip->i_mtime = m_in.REQ_MODTIME;
  if (rip->i_mtime < TIME_UNKNOWN)
	rip->i_mtime = TIME_UNKNOWN;
  unix2dostime(rip->i_mtime, dp->deMDate, dp->deMTime);

  rip->i_ctime = clock_time(NULL);

  rip->i_flags |= I_DIRTY;	/* inode is thus now dirty */
#endif
  return EINVAL;
}
