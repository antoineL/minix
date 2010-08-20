/* Basic includes used by the FAT file system
 *
 * Auteur: Antoine Leca, aout 2010.
 */

#define _POSIX_SOURCE 1
#define _MINIX 1

#define _SYSTEM 1		/* for negative error values */
#include <errno.h>

#include <minix/config.h>
#include <minix/const.h>

#include <minix/vfsif.h>

#include "const.h"
#include "type.h"
#include "proto.h"
#include "glo.h"
