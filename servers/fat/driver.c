/* This file contains the interface with the block device driver.
 *
 * The entry points into this file are:
 *   dev_open		open the driver
 *   dev_close		close the driver
 *   do_newdriver	perform the NEWDRIVER file system request
 *   seqblock_dev_io	do I/O operations with the device
 *   scattered_dev_io	do I/O operations, targetting several buffers
 *
 * Auteur: Antoine Leca, aout 2010.
 * Updated:
 */

#include "inc.h"

#include <stdio.h>
#include <string.h>

#ifdef	COMPAT316
#include "compat.h"	/* MUST come before <minix/sysutil.h> */
#endif
#include <minix/u64.h>
#include <minix/com.h>
#include <minix/sysutil.h>	/* panic */
#include <minix/safecopies.h>
#include <minix/syslib.h>	/* sys_safecopies{from,to} */

#ifndef EDEADEPT
#define EDEADEPT EDEADSRCDST
#endif

/* Private functions:
 *   update_dev_status	update the driver status based on its answer 
 */

FORWARD _PROTOTYPE( int update_dev_status, (int task_status,
		endpoint_t driver_e, message *mess_ptr) );

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
  r = sendrec(driver_e, &dev_mess);

  return(update_dev_status(r, driver_e, &dev_mess));
}

/*===========================================================================*
 *				dev_close				     *
 *===========================================================================*/
PUBLIC void dev_close(endpoint_t driver_e, dev_t dev)
{
  int r;
  message dev_mess;

  dev_mess.m_type   = DEV_CLOSE;
  dev_mess.DEVICE   = (dev >> MINOR) & BYTE;
  dev_mess.IO_ENDPT = 0;
  dev_mess.COUNT    = 0;

  /* Call the driver. */
  r = sendrec(driver_e, &dev_mess);

  /* ignore the value given by dev_mess.REP_STATUS... */
}

/*===========================================================================*
 *				do_new_driver   			     *
 *===========================================================================*/
PUBLIC int do_new_driver(void)
{
 /* New driver endpoint for this device */

  assert(dev == (dev_t) m_in.REQ_DEV);
  driver_e = (endpoint_t) m_in.REQ_DRIVER_E;

/* need to odev_open it ??? */

  return(OK);
}

/*===========================================================================*
 *				seqblock_dev_io				     *
 *===========================================================================*/
PUBLIC int seqblock_dev_io(
  int op,			/* DEV_READ_S or DEV_WRITE_S */
/*  dev_t dev,			 * major-minor device number */
/*  int proc_e,			 * in whose address space is buf? */
  void *buf,			/* virtual address of the buffer */
  u64_t pos,			/* byte position on disk */
  int bytes			/* how many bytes to transfer */
)
{
/* Read from or write to the device a single chunk.
 * The parameter 'dev' used to tell which device. Now fixed.
 * The parameter 'proc_e' used to tell the address space. Now fixed.
 */
  int r;
  message m;
  cp_grant_id_t gid = GRANT_INVALID;

  /* See if driver is roughly valid. */
  if (driver_e == NONE) return(EDEADEPT);

  if((gid=cpf_grant_direct(driver_e, (vir_bytes) buf, 
		bytes, op == DEV_READ_S ? CPF_WRITE : CPF_READ)) < 0) {
	panic("cpf_grant_direct for seq_io failed");
  }

  /* Set up rest of the message. */
  m.m_type   = op;
  m.IO_ENDPT = SELF_E;
  m.DEVICE   = (dev >> MINOR) & BYTE;
  m.IO_GRANT = (char *) gid;
  m.ADDRESS  = buf;
  m.POSITION = ex64lo(pos);
  m.HIGHPOS  = ex64hi(pos);
  m.COUNT    = bytes;

  /* Call the task. */
  r = sendrec(driver_e, &m);

  /* As block I/O never SUSPENDs, safecopies cleanup must be done
   * whether the I/O succeeded or not. */
  cpf_revoke(gid);

  /* Task has completed.  See how call completed. */
  return(update_dev_status(r, driver_e, &m));
}

/*===========================================================================*
 *				scattered_dev_io			     *
 *===========================================================================*/
PUBLIC int scattered_dev_io(
  int op,			/* DEV_GATHER_S or DEV_SCATTER_S */
/*  dev_t dev,			 * major-minor device number */
/*  int proc_e,			 * in whose address space is buf? */
  iovec_t iovec[],		/* vector of buffers */
  u64_t pos,			/* byte position on disk */
  int cnt			/* how many elements to transfer */
)
{
/* Gather (read) several blocks from the device, or scattered (write) to it.
 * On disk, the blocks still are in sequential order, starting at pos.
 * In memory on the other hand, they might be in various places, the
 * blocks are not constrained to be in order.
 * The parameter 'dev' used to tell which device. Now fixed.
 * The parameter 'proc_e' used to tell the address space. Now fixed.
 */
  int r, j;
  message m;
  iovec_t *v;
  cp_grant_id_t gid = GRANT_INVALID;
/* UGLY STUFF: move to globals, or keep on stack (reentrancy)... */
  static cp_grant_id_t gids[NR_IOREQS];
  static iovec_t grants_vec[NR_IOREQS];
  int vec_grants;

  assert(cnt<=NR_IOREQS);

  /* See if driver is roughly valid. */
  if (driver_e == NONE) return(EDEADEPT);

  /* Grant access to my new "i/o vector". */
  if((gid=cpf_grant_direct(driver_e, (vir_bytes) grants_vec, 
		cnt * sizeof(iovec_t), CPF_READ | CPF_WRITE)) < 0) {
	panic("cpf_grant_direct of vector failed");
  }

  /* Grant access to i/o buffers, and pack into the "I/O vector" */
  for(j = 0, v = iovec; j < cnt; ++j, ++v) {
	grants_vec[j].iov_addr = gids[j] =
		cpf_grant_direct(driver_e, (vir_bytes) v->iov_addr,
		v->iov_size, op == DEV_GATHER_S ? CPF_WRITE : CPF_READ);
	if(!GRANT_VALID(gids[j])) {
		panic("grant to iovec buf failed");
	}
	grants_vec[j].iov_size = v->iov_size;
  }

  /* Set up rest of the message. */
  m.m_type   = op;
  m.IO_ENDPT = SELF_E;
  m.DEVICE   = (dev >> MINOR) & BYTE;
  m.IO_GRANT = (char *) gid;
  m.ADDRESS  = (void *) grants_vec;
  m.POSITION = ex64lo(pos);
  m.HIGHPOS  = ex64hi(pos);
  m.COUNT    = cnt;

  /* Call the task. */
  r = sendrec(driver_e, &m);

  /* As block I/O never SUSPENDs, safecopies cleanup must be done
   * whether the I/O succeeded or not. */
  cpf_revoke(gid);
  for(j = 0; j < cnt; j++)
	cpf_revoke(gids[j]);

  /* Task has completed.  See how call completed. */
  return(update_dev_status(r, driver_e, &m));
}

/*===========================================================================*
 *				update_dev_status			     *
 *===========================================================================*/
PRIVATE int update_dev_status(
  int task_status,		/* what sendrec() returned in the first place */
  endpoint_t driver_e,		/* which driver was called */
  message *mess_ptr		/* pointer to message received */
)
{
/* Task has completed.  See if call completed correctly.
 * RECOVERY PROCESS:
 * - send back dead driver number
 * - VFS unmaps it, waits for new driver
 * - VFS sends the new driver endp for the FS proc and the request again 
 */
  int r = task_status;

  if(r == OK && mess_ptr->REP_STATUS == ERESTART) r = EDEADEPT;

  if (r != OK) {
	if (r == EDEADSRCDST || r == EDEADEPT) {
		printf("FATfs: dead driver %d\n", driver_e);
		/* panic("should handle crashed drivers");
		 * dmap_unmap_by_endpt(driver_e); */
		driver_e = NONE;
		return(r);
	}
	if (r == ELOCKED) {
		printf("FATfs: ELOCKED talking to %d\n", driver_e);
		return(r);
	}
	else
		panic("can't send/receive: %d", r);
  }

  /* Did the process we did the sendrec() for get a result? */
  if (mess_ptr->REP_ENDPT != SELF_E) {
	printf("FATfs: strange device reply from %d, "
		"type = %d, proc = %d (not %d): ignored\n",
		mess_ptr->m_source,
		mess_ptr->m_type,
		mess_ptr->REP_ENDPT,
		SELF_E);
	return(EIO);
  }

  /* Task has completed.  See if call completed. */
  if (mess_ptr->REP_STATUS == SUSPEND) {
	panic("driver returned SUSPEND");
  }

  return(mess_ptr->REP_STATUS);
}
