/* This file contains the interface with the block device driver.
 *
 * The entry points into this file are:
 *   do_newdriver	perform the NEWDRIVER file system request
 *   dev_open		open the driver
 *   dev_close		close the driver
 *   block_dev_io	do I/O operations with the device
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
#include <minix/u64.h>

#include <minix/type.h>
#include <minix/ipc.h>
#include <minix/com.h>
#include <minix/vfsif.h>
#include <minix/sysutil.h>	/* panic */
#include <minix/safecopies.h>
#include <minix/syslib.h>	/* sys_safecopies{from,to} */

#include "const.h"
#include "type.h"
#include "proto.h"
#include "glo.h"

#if DEBUG
#define DBGprintf(x) printf x
#else
#define DBGprintf(x)
#endif

#ifndef ME
 #ifdef MINIX3less60xx
 #define ME "FATfs`"__FILE__, 
 #define NIL ,0
 #else
 #define ME
 #define NIL
 #endif
#else
 #define NIL ,0
#endif

#ifndef EDEADEPT
#define EDEADEPT EDSTDIED
#endif

/*
#include "inc.h"
 */

PRIVATE int dummyproc;

FORWARD _PROTOTYPE( int safe_io_conversion, (endpoint_t, cp_grant_id_t *,
					     int *, cp_grant_id_t *, int,
					     endpoint_t *, void **, int *,
					     vir_bytes));
FORWARD _PROTOTYPE( void safe_io_cleanup, (cp_grant_id_t, cp_grant_id_t *,
					   int));
FORWARD _PROTOTYPE( int gen_io, (endpoint_t task_nr, message *mess_ptr));


/*===========================================================================*
 *				dev_open				     *
 *===========================================================================*/
PUBLIC int dev_open(
  endpoint_t driver_e,
  dev_t dev,			/* device to open */
  int proc,			/* process to open for */
  int flags			/* mode bits and flags */
)
{
  int major, r;
  message dev_mess;

#if 0
  /* Determine the major device number call the device class specific
   * open/close routine.  (This is the only routine that must check the
   * device number for being in range.  All others can trust this check.)
   */
  major = (dev >> MAJOR) & BYTE;
  if (major >= NR_DEVICES) major = 0;
#endif

  dev_mess.m_type   = DEV_OPEN;
  dev_mess.DEVICE   = (dev >> MINOR) & BYTE;
  dev_mess.IO_ENDPT = proc;
  dev_mess.COUNT    = flags;

  /* Call the driver. */
  gen_io(driver_e, &dev_mess);

  r = dev_mess.REP_STATUS;
  if (r == SUSPEND) panic(ME "suspend on open from" NIL);
  return(r);
}

/*===========================================================================*
 *				dev_close				     *
 *===========================================================================*/
PUBLIC void dev_close(endpoint_t driver_e, dev_t dev)
{
  message dev_mess;

  dev_mess.m_type   = DEV_CLOSE;
  dev_mess.DEVICE   = (dev >> MINOR) & BYTE;
  dev_mess.IO_ENDPT = 0;
  dev_mess.COUNT    = 0;

  /* Call the driver. */
  gen_io(driver_e, &dev_mess);

  /* ignore the value given by dev_mess.REP_STATUS... */
}

/*===========================================================================*
 *				do_new_driver   			     *
 *===========================================================================*/
PUBLIC int do_new_driver(void)
{
 /* New driver endpoint for this device */

  ASSERT(dev == (dev_t) m_in.REQ_DEV);
  driver_ep = (endpoint_t) m_in.REQ_DRIVER_E;
  return(OK);
}

/*===========================================================================*
 *				safe_io_conversion			     *
 *===========================================================================*/
PRIVATE int safe_io_conversion(driver, gid, op, gids, gids_size,
	io_ept, buf, vec_grants, bytes)
endpoint_t driver;
cp_grant_id_t *gid;
int *op;
cp_grant_id_t *gids;
int gids_size;
endpoint_t *io_ept;
void **buf;
int *vec_grants;
vir_bytes bytes;
{
	int j;
	iovec_t *v;
	static iovec_t new_iovec[NR_IOREQS];

	/* Number of grants allocated in vector I/O. */
	*vec_grants = 0;

	/* Driver can handle it - change request to a safe one. */

	*gid = GRANT_INVALID;

#if 0
	switch(*op) {
		case MFS_DEV_READ:
		case MFS_DEV_WRITE:
			/* Change to safe op. */
			*op = *op == MFS_DEV_READ ? DEV_READ_S : DEV_WRITE_S;

			if((*gid=cpf_grant_direct(driver, (vir_bytes) *buf, 
				bytes, *op == DEV_READ_S ? CPF_WRITE : 
				CPF_READ)) < 0) {
					panic(ME "cpf_grant_magic of buffer failed" NIL);
			}

			break;
		case MFS_DEV_GATHER:
		case MFS_DEV_SCATTER:
			/* Change to safe op. */
			*op = *op == MFS_DEV_GATHER ?
				DEV_GATHER_S : DEV_SCATTER_S;

			/* Grant access to my new i/o vector. */
			if((*gid = cpf_grant_direct(driver,
			  (vir_bytes) new_iovec, bytes * sizeof(iovec_t),
			  CPF_READ | CPF_WRITE)) < 0) {
				panic(ME "cpf_grant_direct of vector failed" NIL);
			}
			v = (iovec_t *) *buf;
			/* Grant access to i/o buffers. */
			for(j = 0; j < bytes; j++) {
			   if(j >= NR_IOREQS) 
				panic(ME "vec too big: %d", bytes);
			   new_iovec[j].iov_addr = gids[j] =
			     cpf_grant_direct(driver, (vir_bytes)
			     v[j].iov_addr, v[j].iov_size,
			     *op == DEV_GATHER_S ? CPF_WRITE : CPF_READ);
			   if(!GRANT_VALID(gids[j])) {
				panic(ME "mfs: grant to iovec buf failed" NIL);
			   }
			   new_iovec[j].iov_size = v[j].iov_size;
			   (*vec_grants)++;
			}

			/* Set user's vector to the new one. */
			*buf = new_iovec;
			break;
	}
#endif

	/* If we have converted to a safe operation, I/O
	 * endpoint becomes FS if it wasn't already.
	 */
	if(GRANT_VALID(*gid)) {
	  /*
		*io_ept = SELF_E;
	   */
		return 1;
	}

	/* Not converted to a safe operation (because there is no
	 * copying involved in this operation).
	 */
	return 0;
}


/*===========================================================================*
 *			safe_io_cleanup					     *
 *===========================================================================*/
PRIVATE void safe_io_cleanup(gid, gids, gids_size)
cp_grant_id_t gid;
cp_grant_id_t *gids;
int gids_size;
{
/* Free resources (specifically, grants) allocated by safe_io_conversion(). */
  int j;

  cpf_revoke(gid);

  for(j = 0; j < gids_size; j++)
	cpf_revoke(gids[j]);

  return;
}


/*===========================================================================*
 *			block_dev_io					     *
 *===========================================================================*/
PUBLIC int block_dev_io(
  int op,			/* DEV_READ_S, DEV_WRITE_S, etc. */
  dev_t dev,			/* major-minor device number */
  int proc_e,			/* in whose address space is buf? */
  void *buf,			/* virtual address of the buffer */
  u64_t pos,			/* byte position */
  int bytes,			/* how many bytes to transfer */
  int flags			/* special flags, like O_NONBLOCK */
)
{
/* Read or write from a device.  The parameter 'dev' tells which one. */
  int r, safe;
  message m;
  cp_grant_id_t gid = GRANT_INVALID;
  int vec_grants;
  int op_used;
  void *buf_used;
  static cp_grant_id_t gids[NR_IOREQS];

  /* See if driver is roughly valid. */
  if (driver_ep == NONE) return(EDEADEPT);

#if 0
  /* The io vector copying relies on this I/O being for FS itself. */
  if(proc_e != SELF_E) {
      printf("ISOFS(%d) doing block_dev_io for non-self %d\n", SELF_E, proc_e);
      panic(ME "doing block_dev_io for non-self: %d", proc_e);
  }
#endif
  
  /* By default, these are right. */
  m.IO_ENDPT = proc_e;
  m.ADDRESS  = buf;
  buf_used = buf;

  /* Convert parameters to 'safe mode'. */
  op_used = op;
  safe = safe_io_conversion(driver_ep, &gid,
          &op_used, gids, NR_IOREQS, &m.IO_ENDPT, &buf_used,
          &vec_grants, bytes);

  /* Set up rest of the message. */
  if (safe) m.IO_GRANT = (char *) gid;

  m.m_type   = op_used;
  m.DEVICE   = (dev >> MINOR) & BYTE;
  m.POSITION = ex64lo(pos);
  m.COUNT    = bytes;
  m.HIGHPOS  = ex64hi(pos);

  /* Call the task. */
  r = sendrec(driver_ep, &m);
  if(r == OK && m.REP_STATUS == ERESTART) r = EDEADEPT;

  /* As block I/O never SUSPENDs, safe cleanup must be done whether
   * the I/O succeeded or not. */
  if (safe) safe_io_cleanup(gid, gids, vec_grants);
  
  /* RECOVERY:
   * - send back dead driver number
   * - VFS unmaps it, waits for new driver
   * - VFS sends the new driver endp for the FS proc and the request again 
   */
  if (r != OK) {
      if (r == EDEADSRCDST || r == EDEADEPT) {
          printf("FATfs(%s): dead driver %d\n", fs_dev_label, driver_ep);
          driver_ep = NONE;
          return(r);
      }
      else if (r == ELOCKED) {
          return(r);
      }
      else 
          panic(ME "call_task: can't send/receive: %d", r);
  } else {
      /* Did the process we did the sendrec() for get a result? */
      if (m.REP_ENDPT != proc_e) {
          printf("ISOFS strange device reply from %d, type = %d, proc = %d (not %d) (2) ignored\n",
		m.m_source, m.m_type, proc_e, m.REP_ENDPT);
          r = EIO;
      }
  }

  /* Task has completed.  See if call completed. */
  if (m.REP_STATUS == SUSPEND) {
      panic(ME "ISOFS block_dev_io: driver returned SUSPEND" NIL);
  }

  if(buf != buf_used && r == OK) {
      memcpy(buf, buf_used, bytes * sizeof(iovec_t));
  }

  return(m.REP_STATUS);
}

/*===========================================================================*
 *				gen_opcl				     *
 *===========================================================================*/
PRIVATE int gen_opcl(
  endpoint_t driver_e,
  int op,			/* operation, DEV_OPEN or DEV_CLOSE */
  dev_t dev,			/* device to open or close */
  int proc_e,			/* process to open/close for */
  int flags			/* mode bits and flags */
)
{
/* Called from the dmap struct in table.c on opens & closes of special files.*/
  message dev_mess;

  dev_mess.m_type   = op;
  dev_mess.DEVICE   = (dev >> MINOR) & BYTE;
  dev_mess.IO_ENDPT = proc_e;
  dev_mess.COUNT    = flags;

  /* Call the task. */
  gen_io(driver_e, &dev_mess);

  return(dev_mess.REP_STATUS);
}

/*===========================================================================*
 *				gen_io					     *
 *===========================================================================*/
PRIVATE int gen_io(task_nr, mess_ptr)
endpoint_t task_nr;		/* which task to call */
message *mess_ptr;		/* pointer to message for task */
{
/* All file system I/O ultimately comes down to I/O on major/minor device
 * pairs.  These lead to calls on the following routines via the dmap table.
 */

  int r, proc_e;

  proc_e = mess_ptr->IO_ENDPT;

  r = sendrec(task_nr, mess_ptr);
  if(r == OK && mess_ptr->REP_STATUS == ERESTART) r = EDEADEPT;
	if (r != OK) {
		if (r == EDEADSRCDST || r == EDEADEPT) {
			printf("fs: dead driver %d\n", task_nr);
			panic(ME "should handle crashed drivers" NIL);
			/* dmap_unmap_by_endpt(task_nr); */
			return r;
		}
		if (r == ELOCKED) {
			printf("fs: ELOCKED talking to %d\n", task_nr);
			return r;
		}
		panic(ME "call_task: can't send/receive: %d", r);
	}

  	/* Did the process we did the sendrec() for get a result? */
  	if (mess_ptr->REP_ENDPT != proc_e) {
		printf(
		"fs: strange device reply from %d, type = %d, proc = %d (not %d) (2) ignored\n",
			mess_ptr->m_source,
			mess_ptr->m_type,
			proc_e,
			mess_ptr->REP_ENDPT);
		return(EIO);
	}

  return(OK);
}
