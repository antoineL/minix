/* Basic includes used by the FAT file system
 *
 * Auteur: Antoine Leca, aout 2010.
 */

#define _POSIX_SOURCE 1
#define _MINIX 1

#include <minix/config.h>	/* MUST be first */
#include <ansi.h>		/* MUST be second */

#define _SYSTEM 1		/* for negative error values */
#include <errno.h>
#include <assert.h>

#include <minix/types.h>	/* MINIX version of <sys/types.h> */
#include <minix/const.h>
#include <minix/type.h>
#include <minix/ipc.h>

#include <minix/vfsif.h>

#include "const.h"
#include "type.h"
#include "proto.h"
#include "glo.h"

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

#ifndef static_assert
#define JOIN_VALUES(x, y) JOIN(x, y)
#define JOIN(x, y) x ## y

#define static_assert(e, msg) \
typedef char JOIN_VALUES(assertion_failed_at_line_, __LINE__) [(e) ? 1 : -1]
#endif
