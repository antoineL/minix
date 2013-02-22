/*	$NetBSD: cdefs.h,v 1.9 2012/01/20 14:08:06 joerg Exp $	*/

#ifndef	_I386_CDEFS_H_
#define	_I386_CDEFS_H_

#if defined(_STANDALONE)
#ifndef __PCC__
#define	__compactcall	__attribute__((__regparm__(3)))
#else
#define	__compactcall	/* PCC does not support it; and protests verbosely */
#endif
#endif

#define __ALIGNBYTES	(sizeof(int) - 1)

#endif /* !_I386_CDEFS_H_ */
