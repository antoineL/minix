/* Compatibility hack.
 *
 * Auteur: Antoine Leca, aout 2010.
 */

#ifndef COMPAT_H_
#define COMPAT_H_

#ifdef	COMPAT316

/* added in revision 6129 (February 10, hence posterior to 3.1.6) */
_PROTOTYPE(int free_contig, (void *addr, size_t len));

/* in revision 6347 (March 5, 2010, hence posterior to 3.1.6),
 * the prototype for panic() was modified.
 * To keep the source clean (with the new semantic), this file
 * uses cheating hacks to hide the reality to the compiler...
 */

/* first we include the official prototype... */
#ifdef _MINIX_SYSUTIL_H 
#error "compat.h" must be #included before <minix/sysutil.h>
#endif

#define panic HIDEpanic
#include <minix/sysutil.h>
#undef panic
#undef ASSERT

/* Now introduce the new panic()... */
_PROTOTYPE( void MYpanic, (const char *fmt, ...));
#define panic MYpanic
#define ASSERT(c) if(!(c)) { panic("%s:%d: assert %s failed", __FILE__, __LINE__, #c); }

#endif	/* defined COMPAT316 */

#endif
