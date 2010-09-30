/* This file manages directory entries.
 *
 *  The entry points into this file are
 *   direntry_to_inode	enter an entry as inode
 *   add_direntry	add a new entry in a given directory
 *   del_direntry	delete a entry from a given directory
 *   write_direntry	update to disk changes to a directory entry
 *   update_times	update timestamps to a directory/inode
 *
 * Auteur: Antoine Leca, septembre 2010.
 * Updated:
 */
 
#include "inc.h"

#include <stddef.h>
#include <string.h>
#include <dirent.h>
/*#include <sys/stat.h>*/

#ifdef	COMPAT316
#include "compat.h"	/* MUST come before <minix/sysutil.h> */
#endif
#include <minix/com.h>		/* VFS_BASE */
#include <minix/safecopies.h>
#include <minix/syslib.h>	/* sys_safecopies{from,to} */
#include <minix/sysutil.h>	/* panic */

/* Private global variables: */

/* Private functions:
 *   enter_as_inode	?
 */
/*
FORWARD _PROTOTYPE( struct inode *direntry_to_inode,
	(struct fat_direntry*, struct inode* dirp, struct direntryref*)	);
 */
/* warning: the following lines are not failsafe macros */
#define	get_le16(arr) ((u16_t)( (arr)[0] | ((arr)[1]<<8) ))
#define	get_le32(arr) ( get_le16(arr) | ((u32_t)get_le16((arr)+2)<<16) )

/*===========================================================================*
 *				direntry_to_inode			     *
 *===========================================================================*/
PUBLIC struct inode *direntry_to_inode(
  struct fat_direntry * dp,	/* (short name) entry */
  struct inode * dirp,		/* parent directory */
  struct direntryref * dirrefp)	/* coordinates of position within parent dir*/
{
/* Enter the inode as specified by its directory entry.
 * Return the inode with its reference count incremented.

FIXME: long_filename
FIXME: need the struct direntryref
->i_clust
FIXME: refcount?
 */
  struct inode *rip;
  cluster_t parent_clust, clust;
  unsigned entrypos;

  clust = get_le16(dp->deStartCluster);
  if (sb.fatMask == FAT32_MASK)
	clust |= get_le16(dp->deStartClusterHi) >> 16;

  if (dp->deAttributes & ATTR_DIRECTORY) {
	if ( dirrefp->dr_entrypos == 0
	  && memcmp(dp->deName, NAME_DOT, 8+3) != 0) {
		/* canonical coordinates. */
		parent_clust = dirp->i_clust;
#if 0
/* FIXME: it should work OK with normal case... */
	} else if ( dirrefp->dr_entrypos == DIR_ENTRY_SIZE
	  && memcmp(dp->deName, NAME_DOT_DOT, 8+3) != 0
	  && IS_ROOT(dirp) ) {
		/* FAT quirks: the .. entry of 1st-level subdirectories
		 * (which points to the root directory)
		 * has 0 as starting cluster!
		 */
		rip = fetch_inode(ROOT_INODE_NR);
		assert(rip);
		get_inode(dirp);
		return(dirp);
#endif
	} else {
		/* The entry is the named one in parent, or is .. in sub;
		 * eitherways, the "starting cluster" is what we are after.
		 *
		 * FAT quirks: the .. entry of 1st-level subdirectories (which
		 * points to the root directory) has 0 as starting cluster,
		 * which is exactly what is stored in inodes[0].i_parent_clust
		 */
		parent_clust = clust;
	}
	dirrefp->dr_entrypos = entrypos = 0; /* always 0 for a directory */
  } else {
	parent_clust = dirp->i_clust;
	entrypos = dirrefp->dr_entrypos;
  }

/* FIXME: entrypos=0 clust=root peut etre soit root, soit 1re entree de root */

/*
DBGprintf(("FATfs: enter_as_inode ['%.8s.%.3s'], seek %ld+%d",
		rip->i_Name, rip->i_Extension, parent_clust, entrypos));
 */

  if ( (rip = dirref_to_inode(parent_clust, entrypos)) != NULL ) {
	/* found in inode cache */
	get_inode(rip);
	return(rip);
  }

  /* get a fresh inode with ref. count = 1 */
  rip = get_free_inode();
  if (rip == NULL) return NULL;
  rip->i_flags = 0;
  rip->i_direntry = *dp;
  rip->i_dirref = *dirrefp;
/* FIXME: not init'd !!! */
  rip->i_clust = clust;
  rip->i_parent_clust = parent_clust;
  rip->i_entrypos = entrypos;
  rip->i_size = get_le32(dp->deFileSize);
  if ( (dp->deAttributes & ATTR_DIRECTORY) == ATTR_DIRECTORY)  {
	rip->i_flags |= I_DIR;	/* before call to get_mode */
	if (rip->i_clust == CLUST_NONE) {
/* FIXME: Warn+fail is i_clust==CLUST_NONE(0); need to clean the error protocol
 * Note that
		if (IS_ROOT(dirp))
 * should NOT be a possible case, it should have been caught by hash...
 */
		put_inode(rip);
		return(NULL);
	}
	if (rip->i_size == 0) {
	/* in the FAT file system, since directory entries are
	 * replicated in many places around, the deFileSize field
	 * is always stored as 0.
	 * VFS and programs rely on the other hand on a non-zero
	 * value, and will not even consider a 0-sized entry...
	 *
	 * We could enumerate the FAT chain to learn how long
	 * the directory really is; right now we do not do that,
	 * and just place a faked value of one cluster.
	 */
DBGprintf(("FATfs: inode %d is a directory, size up to %u\n", INODE_NR(rip), sb.bpCluster));
		rip->i_size == sb.bpCluster;
		rip->i_flags |= I_DIRNOTSIZED;
	}
  }
/* FIXME: else should check clust==0
 * (or we have potential problems down the road...)
 */
  rip->i_mode = get_mode(rip);

  rehash_inode(rip);
#if 1
  link_inode(dirp, rip);
#endif

  /* initialize the caches */
  memset(&rip->i_fc, '\0', sizeof(rip->i_fc));
  fc_purge(rip, 0);
  rip->i_btime = rip->i_mtime = rip->i_atime = rip->i_ctime = TIME_NOT_CACHED;

  DBGprintf(("FATfs: enter_as_inode creates entry for ['%.8s.%.3s'], cluster=%ld gives %lo\n",
		rip->i_Name, rip->i_Extension, rip->i_clust, INODE_NR(rip)));
  
  return(rip);
}

/*===========================================================================*
 *				add_direntry				     *
 *===========================================================================*/
PUBLIC int add_direntry(
  register struct inode *dirp, /* ptr to inode for dir to search */
  char string[NAME_MAX],	/* component to search for */
  struct inode **res_inop)	/* pointer to inode if added */
{
/* This function searches the directory whose inode is pointed to by 'dirp':
 * and enter 'string' in the directory with inode # '*numb'
 */
/*
  register struct direct *dp = NULL;
 */
  register struct buf *bp = NULL;
  int i, r, e_hit, t, match;
  off_t pos;
  unsigned new_slots, old_slots;
  block_t b;
/*
  struct super_block *sp;
 */
  int extended = 0;

#if 0
  /* If 'dirp' is not a pointer to a dir inode, error. */
  if ( ! IS_DIR(dirp) ) return(ENOTDIR);
  
  r = OK;

  if (flag != IS_EMPTY) {
#if 0
  mode_t bits;
	bits = (flag == LOOK_UP ? X_BIT : W_BIT | X_BIT);
#endif

	if (string == dot1 || string == dot2) {
		if (flag != LOOK_UP && read_only) r = EROFS;
				   /* only a writable device is required. */
#if 0
        } else if(check_permissions) {
		r = forbidden(ldirp, bits); /* check access permissions */
#endif
	}
  }
  if (r != OK) return(r);
  
  /* Step through the directory one block at a time. */
  old_slots = (unsigned) (ldirp->i_size/ DIR_ENTRY_SIZE );
  new_slots = 0;
  e_hit = FALSE;
  match = 0;			/* set when a string match occurs */

/* FIXME: if sb.rootEntries*sb.depblk != sb.rootSiz (for fixed root dir),
 * we should NOT rely on the logic "enlarging if needed",
 * and TRUST sb.rootEntries!
 */
  for (pos = 0; pos < ldirp->i_size; pos += /*FIXME ldirp->i_sp->s_block_size*/ 512 ) {
	b = bmap(ldirp, pos);	/* get next block number */

	/* Since directories don't have holes, 'b' cannot be NO_BLOCK. */
	bp = get_block(dev, b, NORMAL);	/* get the directory block */

	assert(bp != NULL);

#if 0
	/* Search a directory block. */
	for (dp = &bp->b_dir[0];
		dp < &bp->b_dir[NR_DIR_ENTRIES(ldirp->i_sp->s_block_size)];
		dp++) {
		if (++new_slots > old_slots) { /* not found, but room left */
			if (flag == ENTER) e_hit = TRUE;
			break;
		}

		/* Match occurs if string found. */
		if (flag != ENTER && dp->d_ino != NO_ENTRY) {
			if (flag == IS_EMPTY) {
				/* If this test succeeds, dir is not empty. */
				if (strcmp(dp->d_name, "." ) != 0 &&
				  strcmp(dp->d_name, "..") != 0) match = 1;
			} else {
				if (strncmp(dp->d_name, string, NAME_MAX) == 0){
					match = 1;
				}
			}
		}

		if (match) {
			/* LOOK_UP or DELETE found what it wanted. */
			r = OK;
			if (flag == IS_EMPTY) r = ENOTEMPTY;
			else if (flag == DELETE) {
				/* Save d_ino for recovery. */
				t = NAME_MAX - sizeof(ino_t);
				*((ino_t *) &dp->d_name[t]) = dp->d_ino;
				dp->d_ino = NO_ENTRY;	/* erase entry */
				bp->b_dirt = DIRTY;
				ldirp->i_update |= CTIME | MTIME;
				ldirp->i_dirt = DIRTY;
			} else {
				sp = ldirp->i_sp;	/* 'flag' is LOOK_UP */
				*numb = (ino_t) conv4(sp->s_native,
						    (int) dp->d_ino);
			}
			put_block(bp, DIRECTORY_BLOCK);
			return(r);
		}

		/* Check for free slot for the benefit of ENTER. */
		if (flag == ENTER && dp->d_ino == 0) {
			e_hit = TRUE;	/* we found a free slot */
			break;
		}
	}
#endif

	/* The whole block has been searched or ENTER has a free slot. */
	if (e_hit) break;	/* e_hit set if ENTER can be performed now */
	put_block(bp);		/* otherwise, continue searching dir */
  }

  /* The whole directory has now been searched. */
  if (flag != ENTER) {
  	return(flag == IS_EMPTY ? OK : ENOENT);
  }
#endif

  /* This call is for ENTER.  If no free slot has been found so far, try to
   * extend directory.
   */
  if (e_hit == FALSE) { /* directory is full and no room left in last block */
	new_slots++;		/* increase directory size by 1 entry */
	if (new_slots == 0) return(EFBIG); /* dir size limited by slot count */
#if 0
	if ( (bp = new_block(ldirp, ldirp->i_size)) == NULL)
		return(err_code);
	dp = &bp->b_dir[0];
#endif
	extended = 1;
  }

#if 0
  /* 'bp' now points to a directory block with space. 'dp' points to slot. */
  (void) memset(dp->d_name, 0, (size_t) NAME_MAX); /* clear entry */
  for (i = 0; i < NAME_MAX && string[i]; i++) dp->d_name[i] = string[i];
  sp = ldirp->i_sp; 
  dp->d_ino = conv4(sp->s_native, (int) *numb);
  bp->b_dirt = DIRTY;
  put_block(bp, DIRECTORY_BLOCK);
  ldirp->i_update |= CTIME | MTIME;	/* mark mtime for update later */
  ldirp->i_dirt = DIRTY;
  if (new_slots > old_slots) {
	ldirp->i_size = (off_t) new_slots * DIR_ENTRY_SIZE;
	/* Send the change to disk if the directory is extended. */
	if (extended) rw_inode(ldirp, WRITING);
  }
#endif
  return(OK);
}

/*===========================================================================*
 *				del_direntry				     *
 *===========================================================================*/
PUBLIC int del_direntry(
  register struct inode *dirp, /* ptr to inode for dir to search */
  struct inode *rip)		/* pointer to inode to delete */
{
/* This function searches the directory whose inode is pointed to by 'dirp',
 * and delete the entry from the directory.
 */
  register union direntry_u * dp;
  register struct buf *bp = NULL;
  struct fat_direntry direntry;
  int r;
  off_t pos;
  block_t b;

  /* If 'dirp' is not a pointer to a dir inode, error. */
  if ( ! IS_DIR(dirp) ) return(ENOTDIR);

/* we should use the dir_ref to check we search the correct parent,
 * and that the entries (the searched and what is on disk) match.
 */

  rip->i_flags |= I_ORPHAN;

}

/*===========================================================================*
 *				update_direntry				     *
 *===========================================================================*/
PUBLIC int update_direntry(struct inode *rip)
{
}

/*===========================================================================*
 *				update_startclust			     *
 *===========================================================================*/
PUBLIC int update_startclust(struct inode *rip, cluster_t cn)
{
  struct fat_direntry * dp = & rip->i_direntry;

  dp->deStartCluster[0] =  cn & 0xFF;
  dp->deStartCluster[1] = (cn <<  8) & 0xFF;
  if (sb.fatMask == FAT32_MASK) {
	dp->deStartClusterHi[0] = (cn << 16) & 0xFF;
	dp->deStartClusterHi[1] = (cn << 24) & 0xFF;
  }
  rip->i_flags |= I_DIRTY;	/* inode is thus now dirty */
}

/*===========================================================================*
 *				update_times				     *
 *===========================================================================*/
PUBLIC int update_times(struct inode *rip,	/* update this directory/inode */
  time_t mtime, time_t atime, time_t ctime)	/* with these timestamps */
{
/* Update both the cached values in inode and the FAT direntry values with
 * the indicated timestamps;
 * some values are conventional:
define	TIME_UNDETERM	(time_t)0	* the value is undeterminate *
define	TIME_NOT_CACHED	(time_t)1	* compute from on-disk value *
define	TIME_UPDATED	(time_t)2	* inode was updated and is dirty;
					 * value to be retrieved from clock
					 *
define	TIME_UNKNOWN	(time_t)3	* nothing known about the value *
 * if the function is passed 0 as argument, it looks at the cached value
 */
  struct fat_direntry *dp;
  int flags = 0;

  dp = & rip->i_direntry;

  if (rip->i_btime < TIME_UNKNOWN) {
/* FIXME: if TIME_UPDATED should use now() and update the deBDate/deBTime field... */
/* FIXME: need to deal with deBHundredth; require struct timespec */
	rip->i_btime = dos2unixtime(dp->deBDate, dp->deBTime);
	if (rip->i_btime > TIME_UNKNOWN && dp->deBHundredth >= 100)
		++rip->i_btime;
  }
  if (mtime) {
	rip->i_mtime = mtime;
	flags |= I_DIRTY;
  }
  if (rip->i_mtime < TIME_UNKNOWN) {
/* FIXME: if TIME_UPDATED should use now() and update the deADate field...
	if (rip->i_atime == TIME_UPDATED)
		unix2dostime( ??? , dp->deADate, NULL);
 */
	rip->i_atime = dos2unixtime( dp->deADate, NULL) ;
  }
  if (rip->i_atime < TIME_UNKNOWN) {
/* FIXME: if TIME_UPDATED should use now() and update the deMDate/deMTime field...
	if (rip->i_mtime == TIME_UPDATED)
		unix2dostime( ??? , dp->deMDate, dp->deMTime);
 */
	rip->i_mtime = dos2unixtime(dp->deMDate, dp->deMTime) ;
  }
  if (rip->i_ctime < TIME_UNKNOWN) {
/* FIXME: if TIME_UPDATED should use now()... */
	rip->i_ctime = rip->i_mtime; /* no better idea with FAT */
  }

  rip->i_flags |= I_DIRTY;	/* inode is thus now dirty */
}
