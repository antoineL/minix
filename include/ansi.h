/* The <ansi.h> header attempts to decide whether the compiler has enough
 * conformance to Standard C for Minix to take advantage of.  If so, the
 * symbol _ANSI is defined (as 31459).  Otherwise _ANSI is not defined
 * here, but it may be defined by applications that want to bend the rules.
 * The magic number in the definition is to inhibit unnecessary bending
 * of the rules.  (For consistency with the new '#ifdef _ANSI" tests in
 * the headers, _ANSI should really be defined as nothing, but that would
 * break many library routines that use "#if _ANSI".)

 * If _ANSI ends up being defined, a macro
 *
 *	_PROTOTYPE(function, params)
 *
 * is defined.  This macro expands in different ways, generating either
 * ANSI Standard C prototypes or old-style K&R (Kernighan & Ritchie)
 * prototypes, as needed.  Finally, some programs use _CONST, _VOIDSTAR etc
 * in such a way that they are portable over both ANSI and K&R compilers.
 * The appropriate macros are defined here.
 */

#ifndef _ANSI_H
#define _ANSI_H

#if __STDC__ == 1
#define _ANSI		31459	/* compiler claims full ANSI conformance */
#endif

#ifdef __GNUC__
#define _ANSI		31459	/* gcc conforms enough even in non-ANSI mode */
#endif

#define	_VOIDSTAR	void *
#define	_VOID		void

#ifdef _ANSI

/* Keep everything for ANSI prototypes. */
#define	_PROTOTYPE(function, params)	function params
#define	_ARGS(params)			params

#define	_CONST		const
#define	_VOLATILE	volatile
#define _SIZET		size_t

#else

/* Throw away the parameters for K&R prototypes. */
#define	_PROTOTYPE(function, params)	function()
#define	_ARGS(params)			()

#define	_CONST
#define	_VOLATILE
#define _SIZET		int

#endif /* _ANSI */

#if defined(__GNUC__) && !defined(__STRICT_ANSI__) || __STDC_VERSION__ >= 199901
#define __LONG_LONG_SUPPORTED 1
#endif

/* This should be defined as restrict when a C99 compiler is used. */
#define _RESTRICT

/* Setting any of _MINIX, _POSIX_C_SOURCE or _POSIX2_SOURCE implies
 * _POSIX_SOURCE.  (Seems wrong to put this here in ANSI space.)
 */
#if defined(_MINIX) || _POSIX_C_SOURCE > 0 || defined(_POSIX2_SOURCE)
#undef _POSIX_SOURCE
#define _POSIX_SOURCE	1
#endif

/* What is a va_list? */
#include <stdarg.h>
#define _BSD_VA_LIST_ va_list

#endif /* ANSI_H */
