/* Fundamental constants used by the FAT file system.
 *
 * Auteur: Antoine Leca, aout 2010.
 */

#ifndef FAT_CONST_H_
#define FAT_CONST_H_

/* The type of sizeof may be (unsigned) long. Use the following macro for
 * taking the sizes of small objects so that there are no surprises like
 * (small) long constants being passed to routines expecting an int.
 */
#ifndef usizeof
#define usizeof(t) ((unsigned) sizeof(t))
#endif

/* Maximum size of names.
 * With the original FAT, names were of 8.3 form so Name_max was 12.
 * However, with LFN, longer names are accepted. The design allows 
 * up to 63*13=819 characters (!) but currently everybody uses 255.
 * Note this is independant of NAME_NAX, used in MINIX.
 */
#define OLDFAT_NAME_MAX	12
#define LFN_NAME_MAX	255
#ifndef FAT_NAME_MAX
#define FAT_NAME_MAX	LFN_NAME_MAX
#endif

/* The FAT file system is built over a "sector" size which is
 * between 128 and 8192 (with varying sources and ranges), but at
 * the moment we only consider 512, the by far commonest value.
 */
#ifndef MIN_SECTOR_SIZE
#define MIN_SECTOR_SIZE	512	/* might be less? */
#endif
#ifndef MAX_SECTOR_SIZE
#define MAX_SECTOR_SIZE	512	/* might be up to 8192 */
#endif

/* (Default) number of buffers in the cache.
 * See the comments at the start of cache.c for the explanations about the
 * block numbering.
 */
#ifndef	NUM_BUFS
#define	NUM_BUFS	200 
#endif

/* Size of buffers in the cache. */
#ifndef MIN_BLOCK_SIZE
#define MIN_BLOCK_SIZE	512
#endif
#ifndef MAX_BLOCK_SIZE
#define MAX_BLOCK_SIZE	4096	/* also the preferred size, if possible */
#endif

/* Number of inodes.
 * See the comments at the start of inode.c for the explanations about the
 * inode numberings and their uses.
 *
 * The following number must not exceed 16, the i_index field is only a short.
 * Also, it is wise to be close to the number of inodes in VFS, currently 512.
 */
#define NUM_INODE_BITS		9

/* Size of the inodes[] array. */
#define NUM_INODES	(1 << NUM_INODE_BITS)

/* Number of entries in the inode hashtable (for clusters). */
#define NUM_HASH_SLOTS   1023

/* Some handy macros to manage the synthetised inode numbers.
 * warning: the following line is not a proper macro
 */
#if 1
#define INODE_NR(i)	(((i)->i_gen << NUM_INODE_BITS) | (i)->i_index)
#else
#define INODE_SYNTHETIC	0x40000000
#define IS_SYNTHETIC(n)	((n) & INODE_SYNTHETIC)

#define INODE_NR(i)	((i)->i_clust ? (i)->i_clust \
	: ((i)->i_gen << NUM_INODE_BITS) | (i)->i_index | INODE_SYNTHETIC)
#endif

#define INODE_INDEX(n)	( (n) & ((1 << NUM_INODE_BITS) - 1) )
#define INODE_GEN(n)	(((n) >> NUM_INODE_BITS) & 0xffff)

/* We should avoid inode number 0. Since the root inode is never freed,
 * we give it the [0] slot, and we make sure it gets a non-zero generation
 * number, so the resulting combined inode number is non-zero.
 */
#define ROOT_GEN_NR	04007
#define ROOT_INODE_NR	(ROOT_GEN_NR << NUM_INODE_BITS)

#endif
