/* This file deals with inode management.
 * The inode content is mostly abstract here; the part which deals with
 * the actual content is in direntry.c.
 *
 * The entry points into this file are:
 *   init_inodes	initialize the inode table, return the root inode
 *   fetch_inode	find an inode based on its VFS inode number
 *   find_inode		find an inode based on its cluster number
 *   enter_inode	allocate a new inode corresponding to an entry
 *   get_inode		find or load an inode, increasing its reference count
 *   put_inode		decrease the reference count of an inode
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

#include "inc.h"

#include <stdio.h>
#include <string.h>

#ifdef	COMPAT316
#include "compat.h"	/* MUST come before <minix/sysutil.h> */
#endif
#include <minix/sysutil.h>	/* panic */

/* FIXME:
 * explain why inode, purpose etc.
 * explain why cannot use cluster numbers
 * explain why cannot use dirref
 * explain that files have only one entry (no hardlink)
 *   but directories may have many links... and we want only one inode!
 * explain that deleted entries should be kept around here _and_ in the
 * FAT chains, until the last open reference in VFS is closed,
 * and /then/ (which means in put_node) can be wiped from the disk copy...
 */

/* FIXME: recover hash linking */

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

/* Number of entries in the cluster hashtable. */
/* FIXME: go to const.h */
#define NUM_HASH_SLOTS   1023

PRIVATE struct inode inodes[NUM_INODES];

PRIVATE TAILQ_HEAD(free_head, inode) free_list;
/* inode hashtable */
PRIVATE LIST_HEAD(hash_lists, inode) hash_inodes[NUM_HASH_SLOTS];

/*===========================================================================*
 *				init_inodes				     *
 *===========================================================================*/
PUBLIC struct inode *init_inodes(int new_num_inodes)
{
/* Initialize inodes table. Return the root inode.
 */
  struct inode *ino;
  int index;

  assert(new_num_inodes == NUM_INODES);

  TAILQ_INIT(&free_list);

  /* Initialize (as empty) all the hash queues. */
  for (index = 0; index < NUM_HASH_SLOTS; index++)
	LIST_INIT(&hash_inodes[index]);

  DBGprintf(("FATfs: %d inodes (0-0%lo), %u bytes each, = %u b.\n",
	NUM_INODES, NUM_INODES-1, sizeof(struct inode), sizeof(inodes)));

  /* Mark all inodes except the root inode as free. */
  for (index = 1; index < NUM_INODES; index++) {
	ino = &inodes[index];
	ino->i_parent = NULL;
	LIST_INIT(&ino->i_child);
#if 0
	ino->i_num = index + 1;
	ino->i_gen = (unsigned short)-1; /* aesthetics */
#else
	ino->i_index = index;
#endif
	ino->i_gen = (index*index) & 0xff; /* some fancy small number */
	ino->i_ref = 0;
	ino->i_flags = 0;
	TAILQ_INSERT_TAIL(&free_list, ino, i_free);
  }

  /* Initialize and return the root inode. */
  ino = &inodes[0];
  ino->i_parent = ino;		/* root inode is its own parent */
  LIST_INIT(&ino->i_child);
#if 0
  ino->i_num = ROOT_INODE_NR;
  ino->i_gen = 0;		/* unused by root node */
#else
  ino->i_index = 0;
  ino->i_gen = ROOT_GEN_NR;	/* fixed for root node */
#endif
  ino->i_ref = 1;		/* root inode is hereby in use */
  ino->i_flags = I_DIR;		/* root inode is a directory */
  ino->i_Attributes = ATTR_DIRECTORY;
  memset(ino->i_Name, ' ', 8);	/* root inode has empty name */
  memset(ino->i_Extension, ' ', 3);
  ino->i_clust = 1;		/* root inode is given a conventional number*/

  return ino;
}

/*===========================================================================*
 *				fetch_inode				     *
 *===========================================================================*/
PUBLIC struct inode *fetch_inode(ino_nr)
ino_t ino_nr;
{
/* Return an inode based on its (perhaps synthetised) number, as known by VFS.
 * Do not increase its reference count.
 */
  struct inode *ino;
  int index;

  /* Inode 0 (= index -1) is not a valid inode number. */

  index = INODE_INDEX(ino_nr);
  if (index < 0) {
	printf("FATfs: VFS passed invalid inode number!\n");

	return NULL;
  }

  assert(index < NUM_INODES);

  ino = &inodes[index];

  /* Make sure the generation number matches. */
  if (INODE_GEN(ino_nr) != ino->i_gen) {
	printf("FATfs: VFS passed outdated inode number!\n");

	return NULL;
  }

  /* The VFS/FS protocol only uses referenced inodes. */
  if (ino->i_ref == 0)
	printf("FATfs: VFS passed unused inode!\n");

  return ino;
}

/*===========================================================================*
 *				find_inode        			     *
 *===========================================================================*/
PUBLIC struct inode *find_inode(
  cluster_t cn			/* cluster number */
)
{
/* Find the inode specified by its coordinates, the first cluster number.
 * Do not increase its reference count.
 */
  struct inode *ino;
  int hashi;

  hashi = (int) (cn % NUM_HASH_SLOTS);

  /* Search inode in the hash table */
  LIST_FOREACH(ino, &hash_inodes[hashi], i_hash) {
      if (ino->i_ref > 0 && ino->i_clust == cn) {
          return(ino);
      }
  }
  
  return(NULL);
}

/*===========================================================================*
 *				enter_inode				     *
 *===========================================================================*/
PUBLIC struct inode *enter_inode(
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
  LIST_FOREACH(rip, &hash_inodes[hashi], i_hash) {
	if (rip->i_ref > 0 && rip->i_clust == cn) {
		++rip->i_ref;
		return(rip);
	}
  }

  /* get a new inode with ref. count = 1 */
  rip = get_free_inode();
  rip->i_direntry = *dp;
  rip->i_clust = cn;
  LIST_INSERT_HEAD(&hash_inodes[hashi], rip, i_hash);
/* more work needed here: i_mode, i_size */

  DBGprintf(("FATfs: enter_inode create entry for ['%.8s.%.3s']\n",
		rip->i_Name, rip->i_Extension));
  
  return(rip);
}

/*===========================================================================*
 *				get_inode				     *
 *===========================================================================*/
PUBLIC void get_inode(ino)
struct inode *ino;
{
/* Increase the given inode's reference count. If both reference and link
 * count were zero before, remove the inode from the free list.
 */

  DBGprintf(("FATfs: get_inode(%p):%lo ['%.8s.%.3s']\n", ino,
		INODE_NR(ino), ino->i_Name, ino->i_Extension));

  /* (INUSE, CACHED) -> INUSE */

  /* If this is the first reference, remove the node from the free list. */
  if (ino->i_ref == 0 && !HAS_CHILDREN(ino))
	TAILQ_REMOVE(&free_list, ino, i_free);

  ino->i_ref++;

  if (ino->i_ref == 0)
	panic("inode reference count wrapped");
}

/*===========================================================================*
 *				put_inode				     *
 *===========================================================================*/
PUBLIC void put_inode(ino)
struct inode *ino;
{
/* Decrease an inode's reference count. If this count has reached zero, close
 * the inode's file handle, if any. If both reference and link count have
 * reached zero, mark the inode as cached or free.
 */

  assert(ino != NULL);
  DBGprintf(("FATfs: put_inode(%p):%lo ['%.8s.%.3s']\n", ino,
		INODE_NR(ino), ino->i_Name, ino->i_Extension));
  assert(ino->i_ref > 0);

  ino->i_ref--;

  /* If there are still references to this inode, we're done here. */
  if (ino->i_ref > 0)
	return;

/* free_cluster_chain */
/* rw indode if DIRTY + MTIME etc. */
/* doit appeler rw_inode(rip, WRITING), cf. flush */

  /* Close any file handle associated with this inode. */
/*
  put_handle(ino);
 */

  /* Only add the inode to the free list if there are also no links to it. */
  if (HAS_CHILDREN(ino))
	return;

  /* INUSE -> CACHED, DELETED -> FREE */

/* unhash */

  /* Add the inode to the head or tail of the free list, depending on whether
   * it is also deleted (and therefore can never be reused as is).
   */
  if (ino->i_parent == NULL)
	TAILQ_INSERT_HEAD(&free_list, ino, i_free);
  else
	TAILQ_INSERT_TAIL(&free_list, ino, i_free);
}

/*===========================================================================*
 *				link_inode				     *
 *===========================================================================*/
PUBLIC void link_inode(parent, ino)
struct inode *parent;
struct inode *ino;
{
/* Link an inode to a parent. If both reference and link count were zero
 * before, remove the inode from the free list.
 * This function should only be called from add_direntry().
 */

  /* This can never happen, right? */
  if (parent->i_ref == 0 && !HAS_CHILDREN(parent))
	TAILQ_REMOVE(&free_list, parent, i_free);

  LIST_INSERT_HEAD(&parent->i_child, ino, i_next);

  ino->i_parent = parent;
}

/*===========================================================================*
 *				unlink_inode				     *
 *===========================================================================*/
PUBLIC void unlink_inode(ino)
struct inode *ino;
{
/* Unlink an inode from its parent. If both reference and link count have
 * reached zero, mark the inode as cached or free.
 * This function should only be used from del_direntry().
 */
  struct inode *parent;

  parent = ino->i_parent;

  LIST_REMOVE(ino, i_next);
  
  if (parent->i_ref == 0 && !HAS_CHILDREN(parent)) {
	if (parent->i_parent == NULL)
		TAILQ_INSERT_HEAD(&free_list, parent, i_free);
	else
		TAILQ_INSERT_TAIL(&free_list, parent, i_free);
  }

  ino->i_parent = NULL;
}

/*===========================================================================*
 *				get_free_inode				     *
 *===========================================================================*/
PUBLIC struct inode *get_free_inode()
{
/* Return a free inode object (with reference count 1), if available.
 */
  struct inode *ino;

  /* [CACHED -> FREE,] FREE -> DELETED */

  /* If there are no inodes on the free list, we cannot satisfy the request. */
  if (TAILQ_EMPTY(&free_list)) {
	printf("FATfs: out of inodes!\n");

	return NULL;
  }

  ino = TAILQ_FIRST(&free_list);
  TAILQ_REMOVE(&free_list, ino, i_free);

  assert(ino->i_ref == 0);
  assert(!HAS_CHILDREN(ino));

#if 0
  /* If this was a cached inode, free it first. */
  if (ino->i_parent != NULL)
	del_direntry(ino);
#endif

  assert(ino->i_parent == NULL);

  /* Initialize a subset of its fields */
  ino->i_gen++;
  ino->i_ref = 1;

  return ino;
}

/*===========================================================================*
 *				have_free_inode				     *
 *===========================================================================*/
PUBLIC int have_free_inode(void)
{
/* Check whether there are any free inodes at the moment. Kind of lame, but
 * this allows for easier error recovery in some places.
 */

  return !TAILQ_EMPTY(&free_list);
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
