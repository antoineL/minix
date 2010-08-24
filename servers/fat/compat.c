/* Compatibility hacks to run new code on "old" MINIX 3.1.6.
 *
 * Free_contig added in revision 6129 (February 10)
 *
 * In revision 6347 (March 5, 2010),
 * the prototype for panic() was modified.
 * To keep the source clean (with the new semantic), this file
 * uses cheating hacks to hide the reality to the compiler...
 *
 * Auteur: Antoine Leca, aout 2010.
 */

/* We really want the prototype for vsnprintf() and strlcat()... */
#define _POSIX_SOURCE 1
#define _MINIX 1

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sys/mman.h>

/* the real panic(), inside libsys.a */
_PROTOTYPE( void panic, (char *who, char *mess, int num));

#ifndef	NO_NUM
#define NO_NUM	0x8000
#endif


/* Wed Feb 10 13:56:26 2010 UTC (6 months, 1 week ago)
 * Log Message:	
 * new free_contig() and changes to make drivers use it; so now we
 * have malloc/free, alloc_contig/free_contig and mmap/munmap nicely
 * paired up.
 */
/*===========================================================================*
 *				free_contig				     *
 *===========================================================================*/
int free_contig(void *addr, size_t len)
{
  return munmap(addr, len);
}


/* Here is the implementation of the function which appears as panic()
 * for our code.
 */

/*===========================================================================*
 *				MYpanic					     *
 *===========================================================================*/
void MYpanic(const char *fmt, ...)
{
/* Something awful happened. */

  static int panicing= 0;
  va_list args;
  char msg[200];

  if(panicing) return;
  panicing= 1;

  if(fmt) {
	va_start(args, fmt);
	vsnprintf(msg, sizeof msg, fmt, args);
	va_end(fmt);
	strlcat(msg, "\n", sizeof msg);
  } else {
	strcpy("(no message)\n", msg);
  }

  /* now calls the old panic */
  panic("", msg, NO_NUM);

  /* If panic() returns from the deads for some reason, hang. */
  for(;;) { }
}

