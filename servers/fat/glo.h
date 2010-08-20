/* Global variables used by the FAT file system
 *
 * Auteur: Antoine Leca, aout 2010.
 */

#ifndef FAT_GLO_H_
#define FAT_GLO_H_

/* EXTERN should be extern except for the table file */
#ifdef _TABLE
#undef EXTERN
#define EXTERN
#endif

#include <minix/type.h>
#include <minix/ipc.h>

EXTERN _PROTOTYPE (int (*vfs_req_vec[]), (void) ); /* VFS requests table */

EXTERN message m;


EXTERN int unmountdone;

#endif
