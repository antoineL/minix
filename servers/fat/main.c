/* Main loop for the FAT file system.
 * It waits for a request, handles it, and then send a response.
 *
 * Also contains the SEF infrastructure.
 *
 * The entry points into this file are:
 *   main	main program of the FAT File System
 *   do_noop	handle requests that do nothing and succeed
 *   no_sys	handle requests that are not implemented
 *   reply	send a reply to a process after the requested work is done
*** + cb, internals
 *
 * Auteur: Antoine Leca, aout 2010.
 * Updated:
 */

/*#include "inc.h"*/
#define _POSIX_SOURCE 1
#define _POSIX_C_SOURCE 200112L	/* setenv */
#define _MINIX 1

#define _SYSTEM 1		/* for negative error values */
#include <errno.h>
#include <signal.h>		/* SIGTERM */
#include <stdlib.h>		/* exit, setenv */
#include <string.h>		/* strcmp */

#if 0

#include <assert.h>

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
#ifdef	COMPAT316
#include "compat.h"
#endif
#include <minix/sysutil.h>	/* env_setargs, panic */
#include <minix/callnr.h>	/* FS_READY */
#include <minix/sef.h>
#include <minix/vfsif.h>

/*
#include <minix/safecopies.h>
#include <minix/syslib.h>
*/

#include "const.h"
#include "type.h"
#include "proto.h"
#include "glo.h"

#include "optset.h"

#if NDEBUG
  #define DBGprintf(x)
#else
#include <stdio.h>
 #if DEBUG
  #define DBGprintf(x) printf x
 #else
  #define DBGprintf(x) if(verbose)printf x
 #endif 
#endif

PRIVATE struct optset optset_table[] = {
  { "uid",      OPT_INT,    &use_uid,         10                 },
  { "gid",      OPT_INT,    &use_gid,         10                 },
  { "fmask",    OPT_INT,    &use_file_mask,   8                  },
  { "dmask",    OPT_INT,    &use_dir_mask,    8                  },
  { "atime",    OPT_BOOL,   &keep_atime,      TRUE               },
  { "noatime",  OPT_BOOL,   &keep_atime,      FALSE              },
/*
  { "exec",     OPT_BOOL,   &prevent_exec,    TRUE               },
  { "noexec",   OPT_BOOL,   &prevent_exec,    FALSE              },
 */
  { "debug",    OPT_BOOL,   &verbose,         TRUE               },
  { NULL                                                         }
};

/* SEF functions and variables. */
FORWARD _PROTOTYPE( void sef_local_startup, (void) );
FORWARD _PROTOTYPE( int sef_cb_init_fresh, (int type, sef_init_info_t *info) );
FORWARD _PROTOTYPE( void sef_cb_signal_handler, (int signo) );

#ifndef INTERCEPT_SEF_SIGNAL_REQUESTS /* SEF before rev.6441 */
/* old stuff, needed to compile this server with older MINIX */
#include <unistd.h>		/* getsigset */
FORWARD _PROTOTYPE( int proc_event, (void) );
#endif

/*===========================================================================*
 *				main                                         *
 *===========================================================================*/
PUBLIC int main(int argc, char *argv[])
{
  int i, r, req_nr, error;
  endpoint_t who_e;

  env_setargs(argc, argv);

  /* Defaults for options */
  use_uid = use_gid = 0;
  use_file_mask = use_dir_mask = 0755; /* we cannot use umask(2) */
  keep_atime = FALSE;
  /* If we have been given an options string, parse options from there. */
  for (i = 1; i < argc - 1; i++)
	if (!strcmp(argv[i], "-o"))
		optset_parse(optset_table, argv[++i]);

  /* SEF local startup. */
  sef_local_startup();

  for (;;) {

	/* Wait for request message. */
	if (OK != (r = sef_receive(ANY, &m_in)))
		panic("FATfs: sef_receive failed: %d", r);

	error = OK;
#if 0
	caller_uid = -1;	/* To trap errors */
	caller_gid = -1;
#endif
	who_e = m_in.m_source;
	req_nr = m_in.m_type;

	if (who_e != VFS_PROC_NR) {

#ifndef INTERCEPT_SEF_SIGNAL_REQUESTS /* SEF before rev.6441 */
                 /* Is this PM telling us to shut down? */
	                 if (who_e == PM_PROC_NR && is_notify(req_nr))
	                         if (proc_event()) break; 
#endif

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

	m_out.m_type = error; 
	reply(who_e, &m_out);	 	/* returns the response to VFS */

  if (error == OK) 
	read_ahead();		/* do block read ahead */
  }
}

/*===========================================================================*
 *				sef_local_startup			     *
 *===========================================================================*/
PRIVATE void sef_local_startup()
{
/* ...
 */
  /* Register init callbacks. */
  sef_setcb_init_fresh(sef_cb_init_fresh);	/* see below */

  /* No live update support for now. */

#ifndef INTERCEPT_SEF_SIGNAL_REQUESTS /* SEF before rev.6441 */
  sef_setcb_init_restart(sef_cb_init_restart_fail);
#else
  sef_setcb_init_restart(sef_cb_init_fail);	/* default handler */

  /* Register signal callbacks. */
  sef_setcb_signal_handler(sef_cb_signal_handler);
#endif

  /* Let SEF perform startup. */
  sef_startup();
}

/*===========================================================================*
 *				sef_cb_init_fresh                            *
 *===========================================================================*/
PRIVATE int sef_cb_init_fresh(int type, sef_init_info_t *info)
{
/* ...
 */
   int i, r;

#if 0
   /* Init driver mapping */
   for (i = 0; i < NR_DEVICES; ++i) 
       driver_endpoints[i].driver_e = NONE;
#endif
  /* SELF_E will contain the id of this process */
  SELF_E = getprocnr();

/*    hash_init(); */			/* Init the table with the ids */
   setenv("TZ","",1);		/* Used to calculate the time */

/*    init_inodes(); */
  init_cache(NR_BUFS);

   m_out.m_type = FS_READY;
   if ((r = send(VFS_PROC_NR, &m_out)) != OK) {
       panic("FATfs: Error sending login to VFS: %d", r);
   }

   return(OK);
}

/*===========================================================================*
 *				sef_cb_signal_handler			     *
 *===========================================================================*/
PRIVATE void sef_cb_signal_handler(int signo)
{
/* ...
 */
  /* Only check for termination signal, ignore anything else. */
  if (signo != SIGTERM) return;

  /* No need to do a sync, as this is a read-only file system. */

  /* If the file system has already been unmounted, exit immediately.
   * We might not get another message.
   */
  if (state == UNMOUNTED) exit(0);
}

/*===========================================================================*
 *				do_nothing				     *
 *===========================================================================*/
PUBLIC int do_nothing(void)
{
/* ...
 */
  return(OK);			/* Trivially succeeds */
}

/*===========================================================================*
 *				readonly				     *
 *===========================================================================*/
PUBLIC int readonly(void)
{
/* ...
 */
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
/* ...
 */
  if (OK != send(who, m_ptr))
#if 0
	printf("FATfs(%d) was unable to send reply\n", SELF_E);
#else
	printf("FATfs: unable to send reply\n");
#endif
}

#ifndef INTERCEPT_SEF_SIGNAL_REQUESTS /* SEF before rev.6441... */
/*===========================================================================*
 *                              proc_event                                   *
 *===========================================================================*/
PRIVATE int proc_event(void)
{
/* We got a notification from PM; see what it's about.
 * Return TRUE if this server has been told to shut down.
 */
  sigset_t set;
  int r;

  if ((r = getsigset(&set)) != OK) {
	printf("FATfs: unable to get pending signals from PM (%d)\n", r);
	
	return FALSE;
  }

  if (sigismember(&set, SIGTERM)) {
	if (state != UNMOUNTED) {
		DBGprintf(("FATfs: got SIGTERM, still mounted\n"));
		
		return FALSE;
	}
	
	DBGprintf(("FATfs: got SIGTERM, shutting down\n"));
	
	return TRUE;
  }

  return FALSE;
}
#endif
