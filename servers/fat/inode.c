/* This file deals with inode management.
 * The inode content is mostly abstract here; the part which deals with
 * the actual content is in direntry.c.
 *
 * The entry points into this file are:
 *   init_inodes	initialize the inode table, return the root inode
 *   fetch_inode	find an inode based on its VFS inode number
 *   find_inode		find an inode based on its cluster number
 *   cluster_to_inode	find an inode based on its cluster number
 *   dirref_to_inode	find an inode based on its coordinates of entry

 *   enter_inode	allocate a new inode corresponding to an entry
 *   get_inode		find or load an inode, increasing its reference count
 *   put_inode		decrease the reference count of an inode
 *   rehash_inode	replace an inode into the hash queue
 *   link_inode		link an inode as a directory entry to another inode
 *   unlink_inode	unlink an inode from its parent directory
 *   get_free_inode	return a free inode object
 *   have_free_inode	check whether there is a free inode available
 *   have_used_inode	check whether any inode is still in use
 *   flush_inodes	flush all dirty inodes
 *
 * Auteur: Antoine Leca, aout 2010.
 * Updated:
 */

/* Inode numbers.
 * There is a real challenge in the FAT file systems to get a number which
 * represent inodes (or files, or directory entries).
 * The most obvious candidate, the starting cluster number, cannot be used
 * because all the empty files have 0 as starting cluster (including after
 * a ftruncate(2) call).
 * Since inode are in fact directory entries, a second candidate can be
 * the coordinates of the entry, which are stored inside a struct direntryref.
 * However the scheme will not work for unlink(2)ed files, which under MINIX
 * can still be accessed, even if there is no directory entry any more;
 * and it is defeated also by the rename(2) call, which may move entries
 * along and as such will modify the coordinates of a given inode.
 *
 * So we synthetise a number which is passed back to VFS.
 * In order to catch phasing errors, that number is built using bitmasks,
 * combining the index in an array for quick reference, and a generation
 * number which is hopefully not reused quickly.
 * Some useful macros are defined in const.h.
 */

/* FIXME:
 * explain why inode, purpose etc.
 * explain that deleted entries should be kept around here _and_ in the
 * FAT chains, until the last open reference in VFS is closed,
 * and /then/ (which means in put_node) can be wiped from the disk copy...
 */

/* Special files.
 * In the FAT file system, there are only regular files and directories.
 * There are no files representing character or block specials, unless
 * some extension are used; several schemes exist, but none is universal
 * enough to warrant its support here.
 * There are no symlink (at file system level) either; again there are
 * several scheme to circumvent this lacking, but nothing is universal.
 * Because of all these shortcomings, we cannot allow the FAT file system
 * to be used as root file system on MINIX.
 *
 * Links.
 * In the FAT file system, regular files have one and only one directory
 * entry; hence their link count is always 1, and cannot be incremented.
 * Directories (except the root) have always at least one entry in its
 * parent directory, plus the first entry named '.' in itself; in addition
 * it has one more link for each subdirectory it may have, where the
 * second entry named '..' will also have the same cluster number.
 */

/* The main portion of the inode array forms a fully linked tree, providing a
 * cached partial view of what the server believes is on the host system. Each
 * inode contains only a pointer to its parent and its path component name, so
 * a path for an inode is constructed by walking up to the root. Inodes that
 * are in use as directory for a child node must not be recycled; in this case,
 * the i_child list is not empty. Naturally, inodes for which VFS holds a
 * reference must also not be recycled; the i_ref count takes care of that.
 *
 * Multiple hard links to a single file do not exist; that is why an inode is
 * also a directory entry (when in IN USE or CACHED state). Notifications about
 * modifications on the host system are not part of the protocol, so sometimes
 * the server may discover that some files do not exist anymore. In that case,
 * they are marked as DELETED in the inode table. Such files may still be used
 * because of open file handles, but cannot be referenced by path anymore.
 * Unfortunately the HGFS v1 protocol is largely path-oriented, so even
 * truncating a deleted file is not possible. This has been fixed in v2/v3, but
 * we currently use the v1 protocol for VMware backwards compatibility reasons.
 *
 * An inode is REFERENCED iff it has a reference count > 0 *or* has children.
 * An inode is LINKED IN iff it has a parent.
 *
 * An inode is IN USE iff it is REFERENCED and LINKED IN.
 * An inode is CACHED iff it is NOT REFERENCED and LINKED IN.
 * An inode is DELETED iff it is REFERENCED and NOT LINKED IN.
 * An inode is FREE iff it is NOT REFERENCED and NOT LINKED IN.
 *
 * An inode may have an open file handle if it is IN USE or DELETED.
 * An inode may have children if it is IN USE (and is a directory).
 * An inode is in the names hashtable iff it is IN USE or CACHED.
 * An inode is on the free list iff it is CACHED or FREE.
 *
 * - An IN USE inode becomes DELETED when it is either deleted explicitly, or
 *   when it has been determined to have become unreachable by path name on the 
 *   host system (the verify_* functions take care of this).
 * - An IN USE inode may become CACHED when there are no VFS references to it
 *   anymore (i_ref == 0), and it is not a directory with children.
 * - A DELETED inode cannot have children, but may become FREE when there are
 *   also no VFS references to it anymore.
 * - A CACHED inode may become IN USE when either i_ref or i_link is increased
 *   from zero. Practically, it will always be i_ref that gets increased, since
 *   i_link cannot be increased by VFS without having a reference to the inode.
 * - A CACHED or FREE inode may be reused for other purposes at any time.
 */

/* FIXME: recover hash linking */

#include "inc.h"

#include <stdio.h>
#include <string.h>

#ifdef	COMPAT316
#include "compat.h"	/* MUST come before <minix/sysutil.h> */
#endif
#include <minix/sysutil.h>	/* panic */

PRIVATE struct inode inodes[NUM_INODES];

PRIVATE TAILQ_HEAD(free_head, inode) unused_inodes;
/* inode hashtables */
PRIVATE LIST_HEAD(hashc_lists, inode) hashcluster_inodes[NUM_HASH_SLOTS];
PRIVATE LIST_HEAD(hashr_lists, inode) hashdirref_inodes[NUM_HASH_SLOTS];

FORWARD _PROTOTYPE( void unhash_inode, (struct inode *node) 		);

/*===========================================================================*
 *				init_inodes				     *
 *===========================================================================*/
PUBLIC struct inode *init_inodes(int new_num_inodes)
{
/* Initialize inodes table. Return the root inode. */
  struct inode *rip;
  int index;

  assert(new_num_inodes == NUM_INODES);

  TAILQ_INIT(&unused_inodes);

  /* Initialize (as empty) all the hash queues. */
  for (index = 0; index < NUM_HASH_SLOTS; index++)
	LIST_INIT(&hashcluster_inodes[index]);
  for (index = 0; index < NUM_HASH_SLOTS; index++)
	LIST_INIT(&hashdirref_inodes[index]);

  DBGprintf(("FATfs: %d inodes (0-0%o), %u bytes each, = %u b.\n",
	NUM_INODES, NUM_INODES-1, usizeof(struct inode), usizeof(inodes)));

  /* Mark all inodes except the root inode as free. */
  for (index = 1; index < NUM_INODES; index++) {
	rip = &inodes[index];
	rip->i_parent = NULL;
	LIST_INIT(&rip->i_child);
#if 0
	ino->i_index = index;
	ino->i_gen = (index*index) & 0xff; /* some fancy small number */
#else
	rip->i_index = index;
	rip->i_gen = (unsigned short)-1; /* aesthetics */
#endif
	rip->i_ref = 0;
	rip->i_flags = 0;
	rip->i_num = rip->i_clust = 0;
/* FIXME: dirref... */
	TAILQ_INSERT_TAIL(&unused_inodes, rip, i_free);
  }

  /* Initialize and return the root inode. */
  rip = &inodes[0];
  rip->i_parent = rip;		/* root inode is its own parent */
  LIST_INIT(&rip->i_child);
#if 1
  rip->i_num = ROOT_INODE_NR;
  rip->i_index = 0;
  rip->i_gen = ROOT_GEN_NR;	/* fixed for root node */
#else
  rip->i_num = ROOT_INODE_NR;
  rip->i_index = 0;
  rip->i_gen = 0;		/* unused */
#endif
  rip->i_ref = 1;		/* root inode is hereby in use */
  rip->i_flags = I_DIR | I_ROOTDIR; /* root inode is a directory */
  rip->i_Attributes = ATTR_DIRECTORY;
  memset(rip->i_Name, ' ', 8);	/* root inode has empty name */
  memset(rip->i_Extension, ' ', 3);
/* FIXME: FAT32 is different... attention rehash! */
  rip->i_clust = 1;		/* root inode is given a conventional number*/
/* FIXME: dirref... */

  rehash_inode(rip);

  return(rip);
}

/*===========================================================================*
 *				fetch_inode				     *
 *===========================================================================*/
PUBLIC struct inode *fetch_inode(ino_t ino_nr)
{
/* Return an inode based on its (synthetised) number, as known by VFS.
 * Do not increase its reference count.
 */
  struct inode *rip;
  int index;

  /* Inode 0 is not a valid inode number. */
  index = INODE_INDEX(ino_nr);
  if (ino_nr <= 0 || index < 0 || index >= NUM_INODES) {
	printf("FATfs: VFS passed invalid inode number!\n");
	return NULL;
  }

  assert(index < NUM_INODES);
  rip = &inodes[index];

  /* Make sure the generation number matches. */
  if (INODE_GEN(ino_nr) != rip->i_gen) {
	printf("FATfs: VFS passed outdated inode number!\n");
	return NULL;
  }

  /* The VFS/FS protocol only uses referenced inodes. */
  if (rip->i_ref == 0) {
	printf("FATfs: VFS passed unused inode!\n");
	/* go on anyway... */
  }

  return rip;
}

/*===========================================================================*
 *				find_inode        			     *
 *===========================================================================*/
PUBLIC struct inode *find_inode(
  cluster_t cn			/* cluster number */
)
{
/* Find the inode specified by its first cluster number.
 * Do not increase its reference count.
 */
  struct inode *rip;
  int hashi;

  hashi = (int) (cn % NUM_HASH_SLOTS);

  /* Search inode in the hash table */
  LIST_FOREACH(rip, &hashcluster_inodes[hashi], i_hashclust) {
      if (rip->i_ref > 0 && rip->i_clust == cn) {
          return(rip);
      }
  }
  
  return(NULL);
}

/*===========================================================================*
 *				cluster_to_inode       			     *
 *===========================================================================*/
PUBLIC struct inode * cluster_to_inode(cluster_t clust)
{
/* Find the inode specified by its first cluster number.
 * Do not increase its reference count.
 */
  struct inode *rip;
  int hashc;

  hashc = (int) (clust % NUM_HASH_SLOTS);

  /* Search inode in the hash table */
  LIST_FOREACH(rip, &hashcluster_inodes[hashc], i_hashclust) {
	if (rip->i_clust == clust) {
		return(rip);
	}
  }
  
  return(NULL);
}

/*===========================================================================*
 *				dirref_to_inode        			     *
 *===========================================================================*/
PUBLIC struct inode *dirref_to_inode(
  cluster_t dirclust,		/* cluster number of the parent directory */
  unsigned entrypos)		/* position within the parent directory */
{
/* Find the inode specified by the coordinates of its directory entry,
 * the first cluster number of the directory and the position within it.
 * Do not increase its reference count.
 */
  struct inode *rip;
  int hashr;

  hashr = (int) ((dirclust ^ entrypos) % NUM_HASH_SLOTS);

  /* Search inode in the hash table */
  LIST_FOREACH(rip, &hashdirref_inodes[hashr], i_hashref) {
	if (rip->i_dirref.dr_clust == dirclust
	 && rip->i_dirref.dr_entrypos == entrypos) {
		return(rip);
	}
  }
  
  return(NULL);
}

/*===========================================================================*
 *				(x)enter_inode				     *
 *===========================================================================*/
PUBLIC struct inode *xenter_inode(
  struct fat_direntry * dp,	/* (short name) entry */
  unsigned entrypos)		/* position within the parent directory */
{
/* Enter the inode as specified by its directory entry.

FIXME: need the struct direntryref (*?)
 */
  struct inode *rip;
  cluster_t cn;			/* cluster number */
  int hashi;

#ifndef get_le16
#define get_le16(arr) ((u16_t)( (arr)[0] | ((arr)[1]<<8) ))
#endif
/* FIXME FAT32 */
  cn = get_le16(dp->deStartCluster);
  hashi = (int) (cn % NUM_HASH_SLOTS);

  DBGprintf(("FATfs: enter_inode ['%.8s.%.3s']\n", dp->deName, dp->deExtension));

  /* Search inode in the hash table */
  LIST_FOREACH(rip, &hashcluster_inodes[hashi], i_hashclust) {
	if (rip->i_ref > 0 && rip->i_clust == cn) {
		++rip->i_ref;
		return(rip);
	}
  }

  /* get a new inode with ref. count = 1 */
  rip = get_free_inode();
  rip->i_direntry = *dp;
  rip->i_clust = cn;
  LIST_INSERT_HEAD(&hashcluster_inodes[hashi], rip, i_hashclust);
/* more work needed here: i_mode, i_size */

  DBGprintf(("FATfs: enter_inode create entry for ['%.8s.%.3s']\n",
		rip->i_Name, rip->i_Extension));

  return(rip);
}

/*===========================================================================*
 *				rehash_inode   			     *
 *===========================================================================*/
PUBLIC void rehash_inode(struct inode *rip) 
{
/* Insert into hash tables */
  int hashc, hashr;

  if (rip->i_clust != 0) {
	hashc = (int) (rip->i_clust % NUM_HASH_SLOTS);
	LIST_INSERT_HEAD(&hashcluster_inodes[hashc], rip, i_hashclust);
  }
  if (rip->i_dirref.dr_clust != 0) {
	hashr = (int) ((rip->i_dirref.dr_clust ^ rip->i_dirref.dr_entrypos)
			% NUM_HASH_SLOTS);
	LIST_INSERT_HEAD(&hashdirref_inodes[hashr], rip, i_hashref);
  }
}

/*===========================================================================*
 *				unhash_inode      			     *
 *===========================================================================*/
PRIVATE void unhash_inode(struct inode *rip) 
{
/* Remove from hash tables */

  if (rip->i_clust != 0) {
	LIST_REMOVE(rip, i_hashclust);
	rip->i_clust = 0;
  }
  if (rip->i_dirref.dr_clust != 0) {
	LIST_REMOVE(rip, i_hashref);
	rip->i_dirref.dr_clust = 0;
  }
}

/*===========================================================================*
 *				get_inode				     *
 *===========================================================================*/
PUBLIC void get_inode(struct inode *rip)
{
/* Increase the given inode's reference count. If both reference and link
 * count were zero before, remove the inode from the free list.
 */

  DBGprintf(("FATfs: get_inode(%p):%lo ['%.8s.%.3s']\n", rip,
		INODE_NR(rip), rip->i_Name, rip->i_Extension));

  /* (INUSE, CACHED) -> INUSE */

  /* If this is the first reference, remove the node from the free list. */
  if (rip->i_ref == 0 && !HAS_CHILDREN(rip))
	TAILQ_REMOVE(&unused_inodes, rip, i_free);

  rip->i_ref++;

  if (rip->i_ref == 0)
	panic("inode reference count wrapped");
}

/*===========================================================================*
 *				put_inode				     *
 *===========================================================================*/
PUBLIC void put_inode(struct inode *rip)
{
/* Decrease an inode's reference count. If this count has reached zero, close
 * the inode's file handle, if any. If both reference and link count have
 * reached zero, mark the inode as cached or free.
 */

  assert(rip != NULL);
  DBGprintf(("FATfs: put_inode(%p):%lo ['%.8s.%.3s']\n", rip,
		INODE_NR(rip), rip->i_Name, rip->i_Extension));
  assert(rip->i_ref > 0);

  rip->i_ref--;

  /* If there are still references to this inode, we're done here. */
  if (rip->i_ref > 0)
	return;

/* free_cluster_chain */
/* rw indode if DIRTY + MTIME etc. */
/* doit appeler rw_inode(rip, WRITING), cf. flush */

  /* Close any file handle associated with this inode. */
/*
  put_handle(rip);
 */

  /* Only add the inode to the free list if there are also no links to it. */
  if (HAS_CHILDREN(rip))
	return;

  /* INUSE -> CACHED, DELETED -> FREE */

/* unhash */

  /* Add the inode to the head or tail of the free list, depending on whether
   * it is also deleted (and therefore can never be reused as is).
   */
  if (rip->i_parent == NULL)
	TAILQ_INSERT_HEAD(&unused_inodes, rip, i_free);
  else
	TAILQ_INSERT_TAIL(&unused_inodes, rip, i_free);
}

/*===========================================================================*
 *				link_inode				     *
 *===========================================================================*/
PUBLIC void link_inode(parent, rip)
struct inode *parent;
struct inode *rip;
{
/* Link an inode to a parent. If both reference and link count were zero
 * before, remove the inode from the free list.
 * This function should only be called from add_direntry().
 */

  /* This can never happen, right? */
  if (parent->i_ref == 0 && !HAS_CHILDREN(parent))
	TAILQ_REMOVE(&unused_inodes, parent, i_free);

  LIST_INSERT_HEAD(&parent->i_child, rip, i_next);

  rip->i_parent = parent;
}

/*===========================================================================*
 *				unlink_inode				     *
 *===========================================================================*/
PUBLIC void unlink_inode(struct inode *rip)
{
/* Unlink an inode from its parent. If both reference and link count have
 * reached zero, mark the inode as cached or free.
 * This function should only be used from del_direntry().
 */
  struct inode *parent;

  parent = rip->i_parent;

  LIST_REMOVE(rip, i_next);
  
  if (parent->i_ref == 0 && !HAS_CHILDREN(parent)) {
	if (parent->i_parent == NULL)
		TAILQ_INSERT_HEAD(&unused_inodes, parent, i_free);
	else
		TAILQ_INSERT_TAIL(&unused_inodes, parent, i_free);
  }

  rip->i_parent = NULL;
}

/*===========================================================================*
 *				get_free_inode				     *
 *===========================================================================*/
PUBLIC struct inode *get_free_inode(void)
{
/* Return a free inode object (with reference count 1), if available.
 */
  struct inode *rip;

  /* [CACHED -> FREE,] FREE -> DELETED */

  /* If there are no inodes on the free list, we cannot satisfy the request. */
  if (TAILQ_EMPTY(&unused_inodes)) {
	printf("FATfs: out of inodes!\n");

	return NULL;
  }

  rip = TAILQ_FIRST(&unused_inodes);
  TAILQ_REMOVE(&unused_inodes, rip, i_free);

  assert(rip->i_ref == 0);
  assert(!HAS_CHILDREN(rip));

  /* If this was a cached inode, free it first. */
  unhash_inode(rip);
#if 0
  if (ino->i_parent != NULL)
	del_direntry(ino);
#endif

  assert(rip->i_parent == NULL);

  /* Initialize a subset of its fields */
  rip->i_gen++;
  rip->i_ref = 1;

  return rip;
}

/*===========================================================================*
 *				have_free_inode				     *
 *===========================================================================*/
PUBLIC int have_free_inode(void)
{
/* Check whether there are any free inodes at the moment. Kind of lame, but
 * this allows for easier error recovery in some places.
 */

  return !TAILQ_EMPTY(&unused_inodes);
}

/*===========================================================================*
 *				have_used_inode				     *
 *===========================================================================*/
PUBLIC int have_used_inode(void)
{
/* Check whether any inodes are still in use, that is, any of the inodes have
 * a reference count larger than zero.
 */
  unsigned int index;

  for (index = 0; index < NUM_INODES; index++)
	if (inodes[index].i_ref > 0) {
		DBGprintf(("FATfs: have_used_inode is TRUE "
			"because inode %lo has %d open references...\n",
			INODE_NR(&inodes[index]), inodes[index].i_ref));
		return TRUE;
	}

  return FALSE;
}

/*===========================================================================*
 *				flush_inodes				     *
 *===========================================================================*/
PUBLIC void flush_inodes(void)
{
/* Write all the dirty inodes to the disk. */
  struct inode *rip;

  for(rip = &inodes[0]; rip < &inodes[NUM_INODES]; rip++)
	if(rip->i_ref > 0 && rip->i_flags & I_DIRTY)
#if 0
		rw_inode(rip, WRITING);
#else
		;
#endif
}
