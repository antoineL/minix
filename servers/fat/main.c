/* Main loop for the FAT file system. Initialize the server;
 * then waits for a request, handles it, and then send a response.
 * Then returns to the preceding line, until killed by signal.
 *
 * Also contains the SEF infrastructure.
 *
 * The entry points into this file are:
 *   main	main program of the FAT File System
 *   do_nothing	handle requests that do nothing and succeed
 *   readonly	handle requests that do not apply to ROFS
 *   no_sys	handle requests that are not implemented
 *
 * Auteur: Antoine Leca, aout 2010.
 * Updated:
 */

#define _POSIX_C_SOURCE 200112L	/* setenv */

#include "inc.h"

#include <signal.h>		/* SIGTERM */
#include <stdlib.h>		/* exit, setenv */
#include <string.h>		/* strcmp */

#ifdef	COMPAT316
#include "compat.h"	/* MUST come before <minix/sysutil.h> */
#endif
#include <minix/com.h>
#include <minix/sysutil.h>	/* env_setargs, panic */
#include <minix/callnr.h>	/* FS_READY */
#include <minix/sef.h>

#include "optset.h"

/* Private read-only global variables: */
PRIVATE struct optset optset_table[] = {
  { "uid",      OPT_INT,    &use_uid,         10                 },
  { "gid",      OPT_INT,    &use_gid,         10                 },
  { "fmask",    OPT_INT,    &use_file_mask,   8                  },
  { "dmask",    OPT_INT,    &use_dir_mask,    8                  },
#ifdef MORE_OPTIONS
/* FIXME: TODO! Also look in glo.h for even more unimplemented options. */
  { "atime",    OPT_BOOL,   &keep_atime,      TRUE               },
  { "noatime",  OPT_BOOL,   &keep_atime,      FALSE              },
  { "exec",     OPT_BOOL,   &prevent_exec,    TRUE               },
  { "noexec",   OPT_BOOL,   &prevent_exec,    FALSE              },
#endif
  { "debug",    OPT_BOOL,   &verbose,         TRUE               },
  { NULL                                                         }
};

#define TRACEREQNM
#ifdef TRACEREQNM
PRIVATE char* vfs_req_name[] = {
	"?",
	"(was getnode)",
	"putnode",
	"slink",
	"ftrunc",
	"chown",
	"chmod",
	"inhibread",
	"stat",
	"utime",
	"fstatfs",
	"bread",
	"bwrite",
	"unlink",
	"rmdir",
	"unmount",
	"sync",
	"new_driver",
	"flush",
	"read",
	"write",
	"mknod",
	"mkdir",
	"create",
	"link",
	"rename",
	"lookup",
	"mountpoint",
	"readsuper",
	"newnode",
	"rdlink",
	"getdents",
	"statvfs",
};
#endif

/* Private SEF functions:
 *   sef_local_startup	?
 *   sef_cb_init_fresh	?
 *   sef_cb_signal_handler ?
 */
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
#ifdef MORE_OPTIONS
/* FIXME: TODO! Also look in glo.h for even more unimplemented options. */
  keep_atime = FALSE;
#endif
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
	who_e = m_in.m_source;
	req_nr = m_in.m_type;

	if (who_e != VFS_PROC_NR) {

#ifndef INTERCEPT_SEF_SIGNAL_REQUESTS /* SEF before rev.6441 */
                 /* Is this PM telling us to shut down? */
	                 if (who_e == PM_PROC_NR && is_notify(req_nr))
	                         if (proc_event()) break; 
#endif

		DBGprintf(("FATfs: get %d from %d; dropped\n", req_nr,who_e));
		continue;
	}

	if (state == MOUNTED
	 || state == NAKED && req_nr == REQ_READSUPER) { 
		req_nr -= VFS_BASE;

#ifdef TRACEREQNM
		DBGprintf(("FATfs: req %d:%s... ", req_nr,
vfs_req_name[(unsigned)req_nr<sizeof vfs_req_name/sizeof vfs_req_name[0]?req_nr:0]));
#endif

		if ((unsigned)req_nr < NREQS)
		/* Process the request calling the appropriate function. */
			error = (*vfs_req_vec[req_nr])();
		else
			error = no_sys();

#ifdef TRACEREQNM
		DBGprintf(("req %d results %d\n", req_nr, error));
#endif
	}
	else error = EINVAL;	/* protocol error */

	m_out.m_type = error; 
	/* returns the response to VFS */
	if (OK != send(who_e, &m_out))
		printf("FATfs(%d) was unable to send reply\n", SELF_E);

	if (error == OK) 
		read_ahead();		/* do block read ahead */
  }
}

/*===========================================================================*
 *				sef_local_startup			     *
 *===========================================================================*/
PRIVATE void sef_local_startup()
{
/* SEF local startup (done once). */

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
/* Called-back function when initialized for the first time. */
   int i, r;

  /* SELF_E will contain the id of this process */
  SELF_E = getprocnr();

  setenv("TZ","",1);		/* Used to calculate local times */

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
/* Called-back function when a signal is received. */

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
/* handle requests that do nothing and succeed. */

  return(OK);			/* Trivially succeeds */
}

/*===========================================================================*
 *				readonly				     *
 *===========================================================================*/
PUBLIC int readonly(void)
{
/* handle requests that do not apply to read-only file system,
 * returning EROFS
 */
  DBGprintf(("FATfs: catch unexpected request while mounted read-only!\n"));
  return(EROFS);		/* unsupported because we are read only */
}

/*===========================================================================*
 *				no_sys					     *
 *===========================================================================*/
PUBLIC int no_sys(void)
{
/* Somebody has used an illegal system call number */
  DBGprintf(("FATfs: catch illegal request number!\n"));
  return(EINVAL);
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
