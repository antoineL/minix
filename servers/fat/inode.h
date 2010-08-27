/* Inode management
 *
 * Auteur: Antoine Leca, aout 2010.
 * Updated:
 */

#ifndef INODE_H_
#define INODE_H_

/* Number of inodes, and inode numbers.
 * There are no natural value in FAT file systems to represent inode numbers.
 * So we synthetise a number which is passed back to VFS.
 * In order to catch phasing errors, that number is built using bitmasks,
 * combining the index in an array for quick reference, and a generation
 * number which is hopefully not reused quickly.
 * Also some useful macros are defined below.
 *
 * The following number must not exceed 16, the i_index field is only a short.
 * Also, it is good to be close to the number of inodes in VFS, currently 512.
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

#define ROOT_GEN_NR	0xFA7
#define ROOT_INODE_NR	(ROOT_GEN_NR << NUM_INODE_BITS)

#endif

#include <sys/queue.h>

#ifndef CLUSTER_T
#define CLUSTER_T
typedef	zone_t	cluster_t;	/* similar concept in Minix FS */
#endif

struct direntryref {
  cluster_t	de_clust;	/* cluster pointing the directory */
  unsigned	de_entrypos;	/* position of the entry within */
};

struct inode {
  struct inode *i_parent;		/* parent inode pointer */
  LIST_HEAD(child_head, inode) i_child;	/* child inode anchor */
  LIST_ENTRY(inode) i_next;		/* sibling inode chain entry */
  LIST_ENTRY(inode) i_hash;		/* hashtable chain entry */
  unsigned short i_index;			/* inode number for quick reference */
  unsigned short i_gen;			/* inode generation number */
  unsigned short i_ref;			/* VFS reference count */
  unsigned short i_flags;		/* any combination of I_* flags */
  union {
	TAILQ_ENTRY(inode) u_free;	/* free list chain entry */
	struct direntryref u_direntry;	/* position of dir.entry */
  } i_u;
  char i_name[NAME_MAX+1];		/* entry name in parent directory */

  cluster_t i_clust;			/* number of first cluster of data */
  off_t i_size;				/* current file size in bytes */

  mode_t i_mode;		/* file type, protection, etc. */
  char i_mountpoint;		/* true if mounted on */
};

#define i_free		i_u.u_free
#define i_direntry	i_u.u_direntry

#define I_DIR		0x01		/* this inode represents a directory */
#define I_HANDLE	0x02		/* this inode has an open handle */

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

#define IS_DIR(i)	((i)->i_flags & I_DIR)
#define HAS_CHILDREN(i)	(!LIST_EMPTY(& (i)->i_child))

#define MODE_TO_DIRFLAG(m)	(S_ISDIR(m) ? I_DIR : 0)

#endif /* INODE_H_ */
