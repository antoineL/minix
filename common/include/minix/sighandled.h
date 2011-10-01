
#ifndef _MINIX_SIGHANDLED_H
#define _MINIX_SIGHANDLED_H

#include <minix/endpoint.h>

_PROTOTYPE( int sighandled, (endpoint_t who, int signo)			);

/* Which behaviour would the process have in front of some signal: */
#define	SIG_IS_IGNORED	0x0001	/* signal is to be ignored */
#define	SIG_IS_CAUGHT	0x0002	/* there is a signal handler */
#define	SIG_IS_KILLER	0x0004	/* signal kills the process */
#define	SIG_IS_STOPPER	0x0008	/* signal stops the process */
#define	SIG_IS_CONT	0x0010	/* signal restarts the process */
#define	SIG_IS_SYSMSG	0x0020	/* signal is transformed into message */
#define	SIG_IS_BLOCKED	0x0080	/* signal is blocked/masked */

#define	SIG_IS_PENDING	0x0100	/* a new signal is pending delivery */

#define	PROC_IS_STOPPED	0x1000	/* the process is stopped (job control) */
#define	PROC_IS_PAUSED	0x2000	/* the process waits (pause, sigsuspend) */
#define	PGRP_IS_ORPHAN	0x8000	/* its process group is orphaned */

#endif

