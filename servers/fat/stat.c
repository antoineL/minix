/* This file contains file metadata retrieval and manipulation routines.
 *
 * The entry points into this file are:
 *   get_mode		return a file's mode
 *   do_stat		perform the STAT file system request
 *   do_chmod		perform the CHMOD file system request
 *   do_utime		perform the UTIME file system request
 *
 * Auteur: Antoine Leca, aout 2010.
 * Updated:
 */

#include "inc.h"

#include <sys/stat.h>

#include <minix/safecopies.h>
#include <minix/syslib.h>	/* sys_safecopies{from,to} */

/*===========================================================================*
 *				get_mode				     *
 *===========================================================================*/
PUBLIC mode_t get_mode(struct inode *rip)
{
/* Return the mode for an inode, given the direntry embeeded in the inode.
 */
  int mode;

  mode = S_IRUSR | S_IXUSR | (rip->i_Attributes&ATTR_READONLY ? 0 : S_IWUSR);
  mode = mode | (mode >> 3) | (mode >> 6);

  if (IS_DIR(rip))
	mode = S_IFDIR | (mode & use_dir_mask);
  else
	mode = S_IFREG | (mode & use_file_mask);

  if (read_only)
	mode &= ~(S_IWUSR|S_IWGRP|S_IWOTH);

  return(mode);
}


/*===========================================================================*
 *				dos2unixtime				     *
 *===========================================================================*/

#if 0
/*
 *  This is the format of the contents of the deTime
 *  field in the direntry structure.
 */
struct DOStime {
	unsigned short
			dt_2seconds:5,	/* seconds divided by 2		*/
			dt_minutes:6,	/* minutes			*/
			dt_hours:5;	/* hours			*/
};

/*
 *  This is the format of the contents of the deDate
 *  field in the direntry structure.
 */
struct DOSdate {
	unsigned short
			dd_day:5,	/* day of month			*/
			dd_month:4,	/* month			*/
			dd_year:7;	/* years since 1980		*/
};

union dostime {
	struct DOStime dts;
	unsigned short dti;
};

union dosdate {
	struct DOSdate dds;
	unsigned short ddi;
};
#endif

/*
 *  Days in each month in a regular year.
 */
unsigned short regyear[] = {
	31,	28,	31,	30,	31,	30,
	31,	31,	30,	31,	30,	31
};

/*
 *  Days in each month in a leap year.
 */
unsigned short leapyear[] = {
	31,	29,	31,	30,	31,	30,
	31,	31,	30,	31,	30,	31
};

/*
 *  The number of seconds between Jan 1, 1970 and
 *  Jan 1, 1980.
 *  In that interval there were 8 regular years and
 *  2 leap years.
 */
#define	SECONDSTO1980	(((8 * 365) + (2 * 366)) * (24 * 60 * 60))

/*union dosdate*/ unsigned short lastdosdate;
unsigned long lastseconds;

PRIVATE time_t dos2unixtime(
	unsigned short * ddp,
	unsigned short * dtp)
{
/*
 *  Convert from dos' idea of time to unix'.
 *  This will probably only be called from the
 *  stat(), and fstat() system calls
 *  and so probably need not be too efficient.
 */
	unsigned long seconds;
	unsigned long dosmonth;
	unsigned long month;
	unsigned long yr;
	unsigned long days;
	unsigned short *months;

  if (*ddp == 0) return 0;	/* return the Epoch if uninitialized */

  if (dtp) {
	seconds = ((*dtp & DT_2SECONDS_MASK) >> DT_2SECONDS_SHIFT) * 2
	    + ((*dtp & DT_MINUTES_MASK) >> DT_MINUTES_SHIFT) * 60
	    + ((*dtp & DT_HOURS_MASK) >> DT_HOURS_SHIFT) * 3600;
/*
	seconds = (dtp->dts.dt_2seconds << 1) +
		  (dtp->dts.dt_minutes * 60) +
		  (dtp->dts.dt_hours * 60 * 60);
 */
  } else seconds = 0;

/*
 *  If the year, month, and day from the last conversion
 *  are the same then use the saved value.
 */
	if (lastdosdate != *ddp) {
		lastdosdate = *ddp;
		days = 0;
#if 0
		for (yr = 0; yr < ddp->dds.dd_year; yr++) {
#else
		for (yr = 0; yr < (*ddp & DD_YEAR_MASK) >> DD_YEAR_SHIFT; yr++) {
#endif
			days += yr & 0x03 ? 365 : 366;
		}
		months = yr & 0x03 ? regyear : leapyear;
/*
 *  Prevent going from 0 to 0xffffffff in the following
 *  loop.
 */
#if 0
		if (ddp->dds.dd_month == 0) {
			printf("dos2unixtime(): month value out of range (%d)\n",
				ddp->dds.dd_month);
			ddp->dds.dd_month = 1;
		}
		for (month = 0; month < ddp->dds.dd_month-1; month++) {
			days += months[month];
		}
#else
		dosmonth = (*ddp & DD_MONTH_MASK) >> DD_MONTH_SHIFT;
		if (dosmonth == 0) {
			printf("dos2unixtime(): month value out of range (%d)\n",
				dosmonth);
			dosmonth = 1;
		}
		for (month = 0; month < dosmonth; month++) {
			days += months[month];
		}
#endif
#if 0
		days += ddp->dds.dd_day - 1;
#else
		days += ((*ddp & DD_DAY_MASK) >> DD_DAY_SHIFT) - 1;
#endif
		lastseconds = (days * 24 * 60 * 60) + SECONDSTO1980;
	}
#if 0
	tvp->tv_sec = seconds + lastseconds + (tz.tz_minuteswest * 60)
		/* -+ daylight savings time correction */;
	tvp->tv_usec = 0;
#else
  return seconds + lastseconds /* -+ daylight savings time correction */ ;
#endif
}

/*===========================================================================*
 *				do_stat					     *
 *===========================================================================*/
PUBLIC int do_stat(void)
{
/* Retrieve inode statistics.
 */
  int r;
  struct stat stat;
  ino_t ino_nr;
  struct inode *rip;

  ino_nr = m_in.REQ_INODE_NR;

  /* Don't increase the inode refcount: it's already open anyway */
  if ((rip = fetch_inode(ino_nr)) == NULL)
	return(EINVAL);

  memset(&stat, '\0', sizeof stat);	/* Avoid leaking any data */
  stat.st_dev = dev;
  stat.st_ino = ino_nr;
  stat.st_mode = get_mode(rip);
  stat.st_uid = use_uid;
  stat.st_gid = use_gid;
  stat.st_rdev = NO_DEV;
#if 0
  stat.st_size = ex64hi(attr.a_size) ? ULONG_MAX : ex64lo(attr.a_size);
#elif 0
  stat.st_size = rip->i_size;
#else
  stat.st_size = IS_DIR(rip) ? 65535 : rip->i_size;
#endif
  stat.st_atime = dos2unixtime( (unsigned short *) &rip->i_direntry.deADate, NULL) ;
  stat.st_mtime = dos2unixtime( (unsigned short *) &rip->i_direntry.deMDate, (unsigned short *) &rip->i_direntry.deMTime);
  stat.st_ctime = stat.st_mtime; /* no better idea with FAT */

  /* We could make this more accurate by iterating over directory inodes'
   * children, counting how many of those are directories as well.
   * It's just not worth it.
   */
  stat.st_nlink = 0;
#if 0
  if (rip->i_parent != NULL) stat.st_nlink++;
#elif 0
  if (rip->i_dirref.dr_clust != 0) stat.st_nlink++;
#else
  stat.st_nlink++;
#endif
  if (IS_DIR(rip)) {
	stat.st_nlink++;
	if (HAS_CHILDREN(rip)) stat.st_nlink++;
  }

  DBGprintf(("FATfs: stat ino=%lo, mode=%o, size=%ld...\n",
	ino_nr, stat.st_mode, stat.st_size));

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
#if 0
  char path[PATH_MAX];
  struct hgfs_attr attr;
#endif
  int r;

  if (read_only)
	return(EROFS);

  /* Don't increase the inode refcount: it's already open anyway */
  if ((rip = fetch_inode(m_in.REQ_INODE_NR)) == NULL)
	return(EINVAL);

/* FIXME: the only thing we can do is to toggle ATTR_READONLY */
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

  m_out.RES_MODE = get_mode(rip);

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

  if (read_only)
	return(EROFS);

  /* Don't increase the inode refcount: it's already open anyway */
  if ((rip = fetch_inode(m_in.REQ_INODE_NR)) == NULL)
	return(EINVAL);

  if (m_in.REQ_UID != use_uid || m_in.REQ_GID != use_gid)
	return(EPERM);

/* FIXME: just i_mode also does the trick... */
  m_out.RES_MODE = get_mode(rip);

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
#if 0
  char path[PATH_MAX];
  struct hgfs_attr attr;
#endif
  int r;

  if (read_only)
	return(EROFS);

  /* Don't increase the inode refcount: it's already open anyway */
  if ((rip = fetch_inode(m_in.REQ_INODE_NR)) == NULL)
	return(EINVAL);

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
