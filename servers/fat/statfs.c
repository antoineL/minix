/* This file contains handlers related to FS statistical requests.
 *
 * The entry points into this file are:
 *   do_fstatfs		perform the FSTATFS file system call
 *   do_statvfs		perform the STATVFS file system call
 *
 * Auteur: Antoine Leca, aout 2010.
 * Updated:
 */

#include "inc.h"

#include <minix/safecopies.h>
#include <minix/syslib.h>	/* sys_safecopies{from,to} */

#include <sys/statfs.h>
#ifdef REQ_STATVFS
#include <sys/statvfs.h>
#endif

/*===========================================================================*
 *				do_fstatfs				     *
 *===========================================================================*/
PUBLIC int do_fstatfs(void)
{
/* Performs the (old-fashioned) DO_STATFS request. */
  int r;
  struct statfs st;

  if (state != MOUNTED)
	  return(EINVAL);
   
  st.f_bsize = bcc.bpBlock;
  
  /* Copy the struct to user space. */
  r = sys_safecopyto(m_in.m_source, (cp_grant_id_t) m_in.REQ_GRANT,
  		     (vir_bytes) 0, (vir_bytes) &st, (size_t) sizeof(st), D);

  return(r);
}

#ifdef REQ_STATVFS
/*===========================================================================*
 *				do_statvfs				     *
 *===========================================================================*/
PUBLIC int do_statvfs(void)
{
/* Performs the (newer, X/Open-compatible) DO_STATVFS request. */
  int r;
  struct statvfs st;

  if (state != MOUNTED)
	  return(EINVAL);

/* The FS server operates with three different units:
 *   the blocks, used in the cache (which is alike the rest of MINIX)
 *   the sectors, the fundamental unit of FAT file systems
 *   the clusters, the allocation unit.
 * Clearly, the f_bsize member points to the clusters, as it is the
 * granularity unit.
 * And logically, b_frsize should refer to sectors, since it is the unit
 * in the outer world.
 * This means that the block size, which is the real unit used in most of
 * the server, does not appear in the structure returned by this call.
 */
  st.f_bsize =  sb.bpCluster;	/* File system block size. */
  st.f_frsize = sb.bpSector;	/* Fundamental file system block size:sector*/
  st.f_blocks = sb.totalSecs;
	  /* Total number of blocks on file system in units of f_frsize. */

  if (sb.freeClustValid)
	st.f_bfree = sb.freeClust << sb.csShift;
	  /* Total number of free blocks (in units of f_frsize). */
  else
	/* This could be computed with a traversal of the whole FAT.
	 * We will spare the time for it for the moment.
	 */
	st.f_bfree = 0;
  st.f_bavail = st.f_bfree;
	  /* Number of free blocks available to non-privileged process */
/* CHECKME: The following one is quite debatable... */
/* Furthermore, our generated inode numbers cannot be compared to it. */
  st.f_files = sb.maxClust-2;	/* Total number of file serial numbers. */
  /* This cannot be computed on FAT, it does not have much sense: */
  st.f_ffree = 0;		/* Total number of free file serial numbers*/
  st.f_favail = st.f_ffree;
	/* Number of file serial numbers available to non-privileged process*/
  st.f_fsid = dev;		/* File system ID */
  st.f_flag = (read_only ? ST_RDONLY : 0); /* Bit mask of f_flag values. */

  st.f_flag |= ST_NOSUID;	/* FAT does not handle set-uid bits */

#if FAT_NAME_MAX < NAME_MAX
  st.f_namemax = FAT_NAME_MAX;	/* Maximum filename length */
#else
  st.f_namemax = NAME_MAX;	/* Maximum filename length (topped) */
#endif
#ifdef ST_NOTRUNC
  st.f_flag |= ST_NOTRUNC;	/* We accept dealing with longer names. */
#endif

  /* Copy the struct to user space. */
  r = sys_safecopyto(m_in.m_source, m_in.REQ_GRANT, 0, (vir_bytes) &st,
		     (phys_bytes) sizeof(st), D);
  
  return(r);
}
#endif
