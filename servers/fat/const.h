/* Constants used by the FAT file system
 *
 * Auteur: Antoine Leca, aout 2010.
 */

#ifndef FAT_CONST_H_
#define FAT_CONST_H_

/* The FAT file system is built over a "sector" size
 * which is officially between 128 and ?
 * but we only consider 512, the by far commonest value
 */
#define SBLOCK_SIZE	512

#ifndef	NR_BUFS
#define	NR_BUFS		100 
#endif

#endif
