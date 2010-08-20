/* Main loop for the FAT file system.
 * It waits for a request, handles it, and then send a response.
 *
 * Also contains the SEF infrastructure.
 *
 * The entry points into this file are:
 *   do_noop		handle requests that do nothing and succeed
 *   no_sys		handle requests that are not implemented
 *
 * Auteur: Antoine Leca, aout 2010.
 * Updated:
 */

/*#include "inc.h"*/
#define _POSIX_SOURCE 1
#define _MINIX 1
#define _SYSTEM 1		/* for negative error values */

#include <assert.h>
#include <errno.h>
#include <signal.h>		/* SIGTERM */

#if 0
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/queue.h>

#include <stdint.h>

#include <unistd.h>	/* getprocnr */
#endif

#include <minix/config.h>
#include <minix/const.h>

#include <minix/type.h>
#include <minix/com.h>
#include <minix/ipc.h>
#include <minix/callnr.h>	/* FS_READY */
#include <minix/sef.h>
#include <minix/vfsif.h>

/*
#include <minix/safecopies.h>
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

/* SEF functions and variables. */
FORWARD _PROTOTYPE( void sef_local_startup, (void) );
FORWARD _PROTOTYPE( int sef_cb_init_fresh, (int type, sef_init_info_t *info) );
FORWARD _PROTOTYPE( void sef_cb_signal_handler, (int signo) );

/*===========================================================================*
 *				main                                         *
 *===========================================================================*/
PUBLIC int main(int argc, char *argv[])
{
  int r, req_nr, error;
  endpoint_t who_e;

  env_setargs(argc, argv);

  /* SEF local startup. */
  sef_local_startup();

  for (;;) {

	/* Wait for request message. */
	if (OK != (r = sef_receive(ANY, &m)))
		panic("FATfs: sef_receive failed: %d", r);

	error = OK;
#if 0
	caller_uid = -1;	/* To trap errors */
	caller_gid = -1;
#endif
	who_e = m.m_source;
	req_nr = m.m_type;

	if (who_e != VFS_PROC_NR) {
		DBGprintf(("FATfs: get %d from %d\n", req_nr, who_e));
		continue;
	}

#if 0
	if (req_nr < VFS_BASE) {

	if (req_nr < VFS_BASE) {
		m_in.m_type += VFS_BASE;
		req_nr = fs_m_in.m_type;
	}

	ind = req_nr-VFS_BASE;

	if (ind < 0 || ind >= NREQS) {
		DBGprintf(("FATfs: get %d from VFS\n", req_nr));
		error = EINVAL; 
	} else
		error = (*vfs_req_vec[ind])(); /* Process the request calling
						* the appropriate function. */
#endif

	m.m_type = error; 
	reply(who_e, &m);	 	/* returns the response to VFS */
  }
}

/*===========================================================================*
 *				sef_local_startup			     *
 *===========================================================================*/
PRIVATE void sef_local_startup()
{
  /* Register init callbacks. */
  sef_setcb_init_fresh(sef_cb_init_fresh);	/* see below */
  sef_setcb_init_restart(sef_cb_init_fail);	/* default handler */

  /* No live update support for now. */

  /* Register signal callbacks. */
  sef_setcb_signal_handler(sef_cb_signal_handler);

  /* Let SEF perform startup. */
  sef_startup();
}

/*===========================================================================*
 *		            sef_cb_init_fresh                                *
 *===========================================================================*/
PRIVATE int sef_cb_init_fresh(int type, sef_init_info_t *info)
{
   int i, r;

#if 0
   /* Init driver mapping */
   for (i = 0; i < NR_DEVICES; ++i) 
       driver_endpoints[i].driver_e = NONE;
   /* SELF_E will contain the id of this process */
   SELF_E = getprocnr();
#endif
/*    hash_init(); */			/* Init the table with the ids */
   setenv("TZ","",1);		/* Used to calculate the time */

   m.m_type = FS_READY;
   if ((r = send(VFS_PROC_NR, &m)) != OK) {
       panic("FATfs: Error sending login to VFS: %d", r);
   }

   return(OK);
}

/*===========================================================================*
 *				sef_cb_signal_handler			     *
 *===========================================================================*/
PRIVATE void sef_cb_signal_handler(int signo)
{
  /* Only check for termination signal, ignore anything else. */
  if (signo != SIGTERM) return;

  /* No need to do a sync, as this is a read-only file system. */

  /* If the file system has already been unmounted, exit immediately.
   * We might not get another message.
   */
  if (unmountdone) exit(0);
}

/*===========================================================================*
 *				do_new_driver   			     *
 *===========================================================================*/
PUBLIC int do_new_driver(void)
{
 /* New driver endpoint for this device */
  dev_t dev;

  dev = (dev_t) m.REQ_DEV;
/*
  driver_endpoints[major(dev)].driver_e =
*/
 (endpoint_t) m.REQ_DRIVER_E;
  return(OK);
}

/*===========================================================================*
 *				do_nothing				     *
 *===========================================================================*/
PUBLIC int do_nothing(void)
{
  return(OK);			/* Trivially succeeds */
}

/*===========================================================================*
 *				readonly				     *
 *===========================================================================*/
PUBLIC int readonly(void)
{
  return(EROFS);		/* unsupported because we are read only */
}

/*===========================================================================*
 *				no_sys					     *
 *===========================================================================*/
PUBLIC int no_sys(void)
{
/* Somebody has used an illegal system call number */
  return(EINVAL);
}

/*===========================================================================*
 *				reply					     *
 *===========================================================================*/
PUBLIC void reply(int who, message *m_ptr)
{
  if (OK != send(who, m_ptr))
#if 0
	printf("FATfs(%d) was unable to send reply\n", SELF_E);
#else
	printf("FATfs: unable to send reply\n");
#endif
}
