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

/* The FAT file system is built over a "sector" size which is
 * between 128 and 8192 (with varying sources and ranges), but at
 * the moment we only consider 512, the by far commonest value.
 */
#define SECTOR_SIZE	512

/* Size of buffers in the cache. */
#ifndef MAX_BLOCK_SIZE
#define MAX_BLOCK_SIZE	8192	/* not the real size */
#endif

/* Default number of buffers in the cache. */
#ifndef	NR_BUFS
#define	NR_BUFS		100 
#endif

/* Number of inodes, and inode numbers.
 * There are no natural value in FAT file systems to represent inode numbers.
 * So we synthetise a number which is passed back to VFS.
 * In order to catch phasing errors, that number is built using bitmasks,
 * combining the index in an array for quick reference, and a generation
 * number which is hopefully not reused quickly.
 * Some useful macros are defined below.
 *
 * The following number must not exceed 16, the i_index field is only a short.
 * Also, it is wise to be close to the number of inodes in VFS, currently 512.
 */
#define NUM_INODE_BITS		9

#if 0
/* We cannot use inode number 0, so to be able to use bitmasks to combine
 * inode and generation numbers, we have to use one fewer than the maximum of
 * inodes possible by using NUM_INODE_BITS bits.
 */
#define NUM_INODES	((1 << NUM_INODE_BITS) - 1)

#define ROOT_INODE_NR	1

#else
/* Size of the inodes[] array.
 * We should avoid inode number 0. Since the root inode is never freed,
 * we give it the [0] slot, and we make sure it gets a non-zero generation
 * number, so the resulting combined inode number is non-zero.
 */
#define NUM_INODES	(1 << NUM_INODE_BITS)

#define ROOT_GEN_NR	04007
#define ROOT_INODE_NR	(ROOT_GEN_NR << NUM_INODE_BITS)

#endif	/* !0 */

/* Some handy macros to manage the synthetised inode numbers.
 * warning: the following line is not a proper macro
 */
#define INODE_NR(i)	(((i)->i_gen << NUM_INODE_BITS) | (i)->i_index)
#if 0
#define INODE_INDEX(n)	(((n) & ((1 << NUM_INODE_BITS) - 1)) - 1)
#else
#define INODE_INDEX(n)	( (n) & ((1 << NUM_INODE_BITS) - 1) )
#endif
#define INODE_GEN(n)	(((n) >> NUM_INODE_BITS) & 0xffff)

#if 0
#define IS_ROOT(i)	((i)->i_num == ROOT_INODE_NR)
#else
#define IS_ROOT(i)	((i)->i_index == (ROOT_INODE_NR & 0xffff))
#endif

#endif
