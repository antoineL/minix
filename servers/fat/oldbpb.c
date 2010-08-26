/* This file contains the table to rebuild missing the superblocks
 * when reading very old (DOS 1.x) FAT file systems.
 *
 * Auteur: Antoine Leca, aout 2010.
 * Updated:
 */

#include "fat.h"

/* In DOS 1.x, the BPB as we know presently was not present on disk.
 * In fact, the first sector was entirelly reserved to the bootstrap.
 * The system figured the fundamental constants of the file system
 * just by interpreting the first byte of the FAT area, called the
 * "media descriptor", at the start of the second sector.
 *
 * In order to deal which such floppies, we supply an image of
 * the superblock, based on the value of the descriptor; the values
 * below are correct only for the most readily available floppy
 * format used with DOS 1.x and 2.x; note that it will not work with
 * 8" floppies (which used FE and FD descriptors, but with meaning
 * very different from the one indicated here, and not even
 * consistent.)
 *
 * Note that only the first values, which are the ones set up by
 * DOS 2.0, are initialized: everything else will take the default
 * 0 value, and the software have to figure itself. This is not a
 * problem though.
 */

#define MIN_OLD_MEDIADESC	0xF8	/* minimum value we can guess */

struct fat_bootsector oldformat_bootsectors[0x100-MIN_OLD_MEDIADESC] = {
/* Media FF: DSDD floppy with 8 sectors/track, a.k.a. 320 KB */
{ /* bsJump */		"\xEB\x1C\x90",
  /* bsOemName */	"MEDIA FF",
  /* bpbBytesPerSec */	"\0\2",		/* 512 bytes/sector */
  /* bpbSecPerClust */	2,
  /* bpbResSectors */	"\1\0",
  /* bpbFATs */		2,
  /* bpbRootDirEnts */	"\x70\0",	/* 112 entries */
  /* bpbSectors */	"\x80\2",	/* 640 sectors */
  /* bpbMedia */	0xFF,
  /* bpbFATsecs */ 	"\1\0",		/* 316 clusters, so fits in 1 sect.*/
  /* bpbSecPerTrack */ 	"\x08\0",
  /* bpbHeads */ 	"\2\0"
},

/* Media FE: SSDD floppy with 8 sectors/track, a.k.a. 160 KB */
{ /* bsJump */		"\xEB\x1C\x90",
  /* bsOemName */	"MEDIA FE",
  /* bpbBytesPerSec */	"\0\2",		/* 512 bytes/sector */
  /* bpbSecPerClust */	1,
  /* bpbResSectors */	"\1\0",
  /* bpbFATs */		2,
  /* bpbRootDirEnts */	"\x40\0",	/* 64 entries */
  /* bpbSectors */	"\x40\1",	/* 320 sectors */
  /* bpbMedia */	0xFE,
  /* bpbFATsecs */ 	"\1\0",		/* 314 clusters, so fits in 1 sect.*/
  /* bpbSecPerTrack */ 	"\x08\0",
  /* bpbHeads */ 	"\1\0"
},

/* Media FD: DSDD floppy with 9 sectors/track, a.k.a. 360 KB */
{ /* bsJump */		"\xEB\x1C\x90",
  /* bsOemName */	"MEDIA FD",
  /* bpbBytesPerSec */	"\0\2",		/* 512 bytes/sector */
  /* bpbSecPerClust */	2,
  /* bpbResSectors */	"\1\0",
  /* bpbFATs */		2,
  /* bpbRootDirEnts */	"\x70\0",	/* 112 entries */
  /* bpbSectors */	"\xD0\2",	/* 720 sectors */
  /* bpbMedia */	0xFD,
  /* bpbFATsecs */ 	"\2\0",		/* 355 clusters, 2 sectors */
  /* bpbSecPerTrack */ 	"\x09\0",
  /* bpbHeads */ 	"\2\0"
},

/* Media FC: SSDD floppy with 9 sectors/track, a.k.a. 180 KB */
{ /* bsJump */		"\xEB\x1C\x90",
  /* bsOemName */	"MEDIA FF",
  /* bpbBytesPerSec */	"\0\2",		/* 512 bytes/sector */
  /* bpbSecPerClust */	1,
  /* bpbResSectors */	"\1\0",
  /* bpbFATs */		2,
  /* bpbRootDirEnts */	"\x70\0",	/* 112 entries */
  /* bpbSectors */	"\x68\1",	/* 360 sectors */
  /* bpbMedia */	0xFC,
  /* bpbFATsecs */ 	"\2\0",		/* 316 clusters, so fits in 1 sect.*/
  /* bpbSecPerTrack */ 	"\x09\0",
  /* bpbHeads */ 	"\1\0"
},

/* Media FB: DSQD floppy with 8 sectors/track, a.k.a. 640 KB */
{ /* bsJump */		"\xEB\x1C\x90",
  /* bsOemName */	"MEDIA FB",
  /* bpbBytesPerSec */	"\0\2",		/* 512 bytes/sector */
  /* bpbSecPerClust */	2,
  /* bpbResSectors */	"\1\0",
  /* bpbFATs */		2,
  /* bpbRootDirEnts */	"\x70\0",	/* 112 entries */
  /* bpbSectors */	"\x00\5",	/* 1280 sectors */
  /* bpbMedia */	0xFB,
  /* bpbFATsecs */ 	"\2\0",		/* 634 clusters, so fits in 2 sect.*/
  /* bpbSecPerTrack */ 	"\x08\0",
  /* bpbHeads */ 	"\2\0"
},

/* Media FA: SSQD floppy with 8 sectors/track; seldom used;
 * This media identifier was later reused for ram-disks.
 */
{ /* bsJump */		"\xEB\x1C\x90",
  /* bsOemName */	"MEDIA FA",
  /* bpbBytesPerSec */	"\0\2",		/* 512 bytes/sector */
  /* bpbSecPerClust */	1,
  /* bpbResSectors */	"\1\0",
  /* bpbFATs */		2,
  /* bpbRootDirEnts */	"\x70\0",	/* 112 entries */
  /* bpbSectors */	"\x80\2",	/* 640 sectors */
  /* bpbMedia */	0xFA,
  /* bpbFATsecs */ 	"\1\0",		/* 628 clusters, so fits in 2 sect.*/
  /* bpbSecPerTrack */ 	"\x08\0",
  /* bpbHeads */ 	"\1\0"
},

/* Media F9: DSQD floppy with 9 sectors/track, a.k.a. 720 KB;
 * This media identifier was later reused for the HD floppy
 * also known as 1.2M (with 15 sectors/track); however this was
 * contemporaneous of DOS 3, and nobody used DOS 1 on HD floppies.
 */
{ /* bsJump */		"\xEB\x1C\x90",
  /* bsOemName */	"MEDIA F9",
  /* bpbBytesPerSec */	"\0\2",		/* 512 bytes/sector */
  /* bpbSecPerClust */	2,
  /* bpbResSectors */	"\1\0",
  /* bpbFATs */		2,
  /* bpbRootDirEnts */	"\x70\0",	/* 112 entries */
  /* bpbSectors */	"\xA0\5",	/* 1440 sectors */
  /* bpbMedia */	0xF9,
  /* bpbFATsecs */ 	"\3\0",		/* 714 clusters, so fits in 3 sect.*/
  /* bpbSecPerTrack */ 	"\x09\0",
  /* bpbHeads */ 	"\2\0"
},

/* Media F8: SSQD floppy with 9 sectors/track; used on some DEC Rainbow.
 * This media identifier was also used for hard disks; however harddisks
 * because of their variety, require the parameters to be explicited.
 */
{ /* bsJump */		"\xEB\x1C\x90",
  /* bsOemName */	"MEDIA F8",
  /* bpbBytesPerSec */	"\0\2",		/* 512 bytes/sector */
  /* bpbSecPerClust */	1,
  /* bpbResSectors */	"\1\0",
  /* bpbFATs */		2,
  /* bpbRootDirEnts */	"\x70\0",	/* 112 entries */
  /* bpbSectors */	"\xD0\2",	/* 720 sectors */
  /* bpbMedia */	0xF8,
  /* bpbFATsecs */ 	"\3\0",		/* 706 clusters, so fits in 1 sect.*/
  /* bpbSecPerTrack */ 	"\x09\0",
  /* bpbHeads */ 	"\1\0"
}
};
