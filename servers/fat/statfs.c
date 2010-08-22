/* This file contains handlers related to FS stats.
 *
 * The entry points into this file are:
 *   do_fstatfs		perform the FSTATFS file system call
 *   do_statvfs		perform the STATVFS file system call
 *
 * Auteur: Antoine Leca, aout 2010.
 * Updated:
 */

/*#include "inc.h"*/
#define _POSIX_SOURCE 1
#define _MINIX 1

#define _SYSTEM 1		/* for negative error values */
#include <errno.h>

#include <sys/types.h>

#include <minix/config.h>
#include <minix/const.h>
#include <minix/type.h>
#include <minix/ipc.h>

#include <minix/vfsif.h>	/* REQ_STATVFS, REQ_GRANT */
#include <minix/safecopies.h>
#include <minix/syslib.h>	/* sys_safecopies{from,to} */

#include <sys/statfs.h>
#ifdef REQ_STATVFS
#include <sys/statvfs.h>
#endif

#if 0
#include <string.h>
#include <limits.h>
#include <sys/queue.h>

#include <stdint.h>

#include <unistd.h>	/* getprocnr */

#include "fs.h"
#include <sys/stat.h>
#include "inode.h"
#include "super.h"

#include <minix/type.h>
#include <minix/com.h>
#include <minix/ipc.h>
#include <minix/callnr.h>	/* FS_READY */
#include <minix/sef.h>
#endif

/*
#include <minix/syslib.h>
#include <minix/sysutil.h>
*/

#include "const.h"
#include "type.h"
#include "proto.h"
#include "glo.h"

#if DEBUG
#include <stdio.h>
#define DBGprintf(x) printf x
#else
#define DBGprintf(x)
#endif



/*===========================================================================*
 *				do_fstatfs				     *
 *===========================================================================*/
PUBLIC int do_fstatfs(void)
{
  int r;
  struct statfs st;
#if 0
  struct inode *rip;

  if((rip = find_inode(fs_dev, ROOT_INODE)) == NULL)
#endif
	  return(EINVAL);
   
  st.f_bsize = /*rip->i_sp->s_block_size;*/ 512;
  
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
  int r;
  struct statvfs st;

#if 0
  struct super_block *sp;
  int r, scale;

  sp = get_super(fs_dev);

  scale = sp->s_log_zone_size;

  st.f_bsize =  sp->s_block_size << scale;	/* File system block size. */
  st.f_frsize = sp->s_block_size;	 /* Fundamental file system block size. */
  st.f_blocks = sp->s_zones << scale;
	 /* Total number of blocks on file system in units of f_frsize. */
  st.f_bfree = count_free_bits(sp, ZMAP) << scale;
	/* Total number of free blocks. */
  st.f_bavail = st.f_bfree;
	/* Number of free blocks available to non-privileged process */
  st.f_files = sp->s_ninodes;
	/* Total number of file serial numbers. */
  st.f_ffree = count_free_bits(sp, IMAP);
	/* Total number of free file serial numbers. */
  st.f_favail = st.f_ffree;
	/* Number of file serial numbers available to non-privileged process*/
  st.f_fsid = fs_dev;		/* File system ID */
  st.f_flag = (sp->s_rd_only == 1 ? ST_RDONLY : 0);
	/* Bit mask of f_flag values. */
  st.f_namemax = NAME_MAX;	/* Maximum filename length */
#endif

  st.f_flag |= ST_NOSUID;	/* FAT does not handle set-uid bits */

  /* Copy the struct to user space. */
  r = sys_safecopyto(m_in.m_source, m_in.REQ_GRANT, 0, (vir_bytes) &st,
		     (phys_bytes) sizeof(st), D);
  
  return(r);
}
#endif
