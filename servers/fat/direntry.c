/* This file contains the procedures that look up names in the directory
 * system and determine the entry that goes with a given name.
 *
 *  The entry points into this file are
 *   do_getdents	perform the GETDENTS file system request
 *   lookup_dir		search for 'filename' and return inode #
 *   add_direntry	enter a new entry in a given directory/inode
 *   del_direntry	delete a entry from a given directory/inode
 *   is_empty_dir	return OK if only . and .. in dir else ENOTEMPTY
 *
 * Auteur: Antoine Leca, septembre 2010.
 * Updated:
 */
 
#include "inc.h"

#include <stddef.h>
/*#include <stdlib.h>*/
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

/* EOVERFLOW is the official POSIX answer when we find a name too long to be
 * sent back to the user process; but it was only recently put in MINIX.
 */
#ifndef EOVERFLOW
#define EOVERFLOW	ENAMETOOLONG	/* replace by something vaguely close... */
#endif

/* FIXME: goto const.h */
#ifndef GETDENTS_BUFSIZ
#define GETDENTS_BUFSIZ  (usizeof(struct dirent) + NAME_MAX + usizeof(long))
#endif

struct getdents_buf {
  char dents_buf[GETDENTS_BUFSIZ];
  size_t mybuf_off;
  cp_grant_id_t callerbuf_gid;
  size_t callerbuf_size, callerbuf_off;
};

/* Private global variables: */

/* Private functions:
 *   enter_as_inode	?
 */
FORWARD _PROTOTYPE( void reset_dents_buf, (struct getdents_buf *, cp_grant_id_t gid, size_t size)	);
FORWARD _PROTOTYPE( int flush_dents_buf, (struct getdents_buf *)	);
FORWARD _PROTOTYPE( size_t dents_buf_written, (struct getdents_buf *)	);
FORWARD _PROTOTYPE( int write_dent,
	(struct getdents_buf*, off_t, ino_t, const char * name)		);

FORWARD _PROTOTYPE( struct inode *enter_as_inode,
	(struct fat_direntry*, struct inode* dirp, struct direntryref*)	);

/* warning: the following lines are not failsafe macros */
#define	get_le16(arr) ((u16_t)( (arr)[0] | ((arr)[1]<<8) ))
#define	get_le32(arr) ( get_le16(arr) | ((u32_t)get_le16((arr)+2)<<16) )

/*===========================================================================*
 *				enter_as_inode				     *
 *===========================================================================*/
PRIVATE struct inode *enter_as_inode(
  struct fat_direntry * dp,	/* (short name) entry */
  struct inode * dirp,		/* parent directory */
  struct direntryref * dirrefp)	/* coordinates of position within parent dir*/
{
/* Enter the inode as specified by its directory entry.
 * Return the inode with its reference count incremented.

FIXME: need the struct direntryref
->i_clust
FIXME: refcount?
 */
  struct inode *rip;
  cluster_t parent_clust, clust;
  unsigned entrypos;

  clust = get_le16(dp->deStartCluster);
  if (sb.fatmask == FAT32_MASK)
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
	  && dirp->i_flags&I_ROOTDIR ) {
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

  if ( (rip = dirref_to_inode(parent_clust, entrypos)) != NULL ) {
	/* found in inode cache */
	get_inode(rip);
	return(rip);
  }

  /* get a fresh inode with ref. count = 1 */
  rip = get_free_inode();
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
DBGprintf(("FATfs: inode %d is a directory, size up to %u\n", INODE_NR(rip), sb.bpcluster));
		rip->i_size == sb.bpcluster;
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
  memset(&rip->i_fc, '\0', sizeof(rip->i_fc));
/* more work needed here: i_fc */

  DBGprintf(("FATfs: enter_as_inode creates entry for ['%.8s.%.3s'], cluster=%ld gives %lo\n",
		rip->i_Name, rip->i_Extension, rip->i_clust, INODE_NR(rip)));
  
  return(rip);
}

/*===========================================================================*
 *				lookup_dir				     *
 *===========================================================================*/
PUBLIC int lookup_dir(
  register struct inode *dirp,	/* ptr to inode for dir to search */
  char string[NAME_MAX],	/* component to search for */
  struct inode **res_inop)	/* pointer to inode if found */
{
/* This function searches the directory whose inode is pointed to by 'dirp',
 * for entry named 'string', and return ptr to inode in (*res_inop).
 *
 * We first build the equivalent directory entry, which is hopefully
 * unique (upper-cased), to speed matches; then we read the whole directory
 * picking entries one by one; for each valid entry, we try to match string.
 */
  register union direntry_u * dp;
  register struct buf *bp = NULL;
  struct fat_direntry direntry;	/* shortFN entry to match */
  struct fat_lfnentry lfnda[LFN_ORD+1]; /* LFN entries read */
  int count_lfn_entries;	/* count of entries in lfnda */
  int expected_lfn_ord;		/* next entry to fetch */
  struct direntryref dirref;	/* structure to locate entry later */
  unsigned long pos;
  unsigned char slot_mark, lfnChksum;

  /* If 'dirp' is not a pointer to a dir inode, error. */
  if ( ! IS_DIR(dirp) ) return(ENOTDIR);

/* FIXME: use some name cache/hash... */

/* FIXME: we currently assume FAT directories are always searchable...
 * A well-behaved way would be to honour use_dir_mask vs. credentials,
 * even more to honour the SYSTEM attribute and use_system_uid etc.
 *
 *   Note: if 'string' is dot1 or dot2, no access permissions are checked.
 */
  
  DBGprintf(("FATfs: lookup in dir=%lo, looking for <%s>...\n",
	INODE_NR(dirp), string));

  memset(&direntry, '\0', sizeof direntry);	/* Avoid leaking any data */
  switch (conv_nameto83(string, &direntry)) {
  case CONV_OK: case CONV_HASLOWER:
	/* we can use the 8.3 name entry for matching */
	break;
  default: /* CONV_NAMETOOLONG, CONV_TRAILINGDOT, CONV_INVAL */
	direntry.deName[0] = '\0'; /* prevent any possible matches */
	break;
  }

  /* Step through the directory one block at a time. */
/*
  for (; pos < dirp->i_size; pos += bcc.bpblock) {
 *
 * FIXME: if sb.rootEntries*sb.depblk != sb.rootSiz (for fixed root dir),
 * we should NOT rely on the same logic (enlarging if needed),
 * and TRUST sb.rootEntries!
 */
  pos = 0;
  expected_lfn_ord = count_lfn_entries = 0; /* reset LFN processing */
  while (TRUE) {
	dirref.dr_absbn = bmap(dirp, pos); /* get next block number */
	if (dirref.dr_absbn == NO_BLOCK) { /* no more data... */
/* FIXME: record the EOF? (+ i_flags) */
		return(ENOENT);
	}
	bp = get_block(dev, dirref.dr_absbn, NORMAL); /* get directory block*/

	assert(bp != NULL);

	/* Search a directory block. */
	for (dp = &bp->b_dir[0];
	     dp < &bp->b_dir[bcc.depblk];
	     dp++, pos+=DIR_ENTRY_SIZE) {
		slot_mark=dp->d_direntry.deName[0];

		/* is the EndOfDirectory mark found? */
		if ( slot_mark == SLOT_EMPTY) {
			put_block(bp);
/* FIXME: record the EOF? (+ i_flags) */
			return(ENOENT);
		}

		assert(slot_mark == dp->d_lfnentry.lfnOrd);
		/* is this entry the next part of long name? */
		if ( slot_mark == expected_lfn_ord
		  && dp->d_lfnentry.lfnAttributes == ATTR_LFN
		  && dp->d_lfnentry.lfnChksum == lfnChksum) {
			/* Next (really previous) part of LFN filename;
			 * accumulate all the entries in the array
			 */
			lfnda[--expected_lfn_ord] = dp->d_lfnentry;
			continue;
		}

		/* is this slot out of use? */
		if (slot_mark == SLOT_DELETED) {
			expected_lfn_ord= count_lfn_entries= 0; /* reset LFN*/
			continue;
		}

		/* is this entry for long names? */
		if(dp->d_lfnentry.lfnAttributes == ATTR_LFN
		  && dp->d_lfnentry.lfnOrd & LFN_LAST
		  && ! (dp->d_lfnentry.lfnOrd & LFN_DELETED) ) {
			/* We found the last part of a long filename.
			 * Since long filenames can lay accross
			 * many entries, possibily crossing blocks,
			 * we will accumulate all the entries in
			 * an array (lfnda), and will process it later.
			 * expected_lfn_ord is the value of the next
			 * entry to be fetched.
			 */
			count_lfn_entries = dp->d_lfnentry.lfnOrd & LFN_ORD;
			expected_lfn_ord = count_lfn_entries;
			lfnda[--expected_lfn_ord] = dp->d_lfnentry;
			lfnChksum = dp->d_lfnentry.lfnChksum;
			continue;
		}

		if(dp->d_direntry.deAttributes & ATTR_VOLUME) {
		/* skip any entry with volume attribute set */
			expected_lfn_ord= count_lfn_entries= 0; /* reset LFN*/
			continue;
		}

		if (count_lfn_entries) {
			/* we have LFN entries before*/
			if (expected_lfn_ord != 0
			 || lfnChksum != lfn_chksum(& dp->d_direntry)) {
			/* not all the entries were there
			 * or the checksums do not match
			 */
				count_lfn_entries = 0;	/* invalid LFN */
			} else {
/* WORK NEEDED: match mixed case lfnda / string

 */
				count_lfn_entries = 0;	/* invalid LFN */
			}
		}

		if (count_lfn_entries == 0) {
		/* Either we do not have LFN entries before,
	???	 * or the file name was somewhat invalid;
		 * let try to match the "short" 8.3 name
		 */
			if (memcmp(dp->d_direntry.deName,
				   direntry.deName, 8+3) != 0) {
			/* short name do not match */
				expected_lfn_ord = 0; /* reset LFN*/
				continue;
			}
		}

		/* we have a match! */
DBGprintf(("match on ['%.8s.%.3s'], LFN=%d, pos=%ld\n",
	dp->d_direntry.deName, dp->d_direntry.deExtension, count_lfn_entries, pos));
		dirref.dr_entrypos = pos;
		dirref.dr_lfnpos = pos - count_lfn_entries*DIR_ENTRY_SIZE;
		assert(res_inop);
		*res_inop = enter_as_inode(&dp->d_direntry, dirp, &dirref);
		if( *res_inop == NULL ) {
/* FIXME: do something clever... */
			panic("FATfs: lookup cannot create inode\n");
		}
		/* inode had its reference count incremented. */
		put_block(bp);
		return(OK);
	}

	/* The whole block has been searched. */
	assert( (pos & bcc.brelmask) == 0); /* block completed */
	put_block(bp);
	/* continue searching dir... */
  }

/* Cannot happen!
 * We should have exited when we encounter the 0 (SLOT_EMPTY) marker,
 * or when we exhausted the cluster chain.
 */
  panic("FATfs: broke out of direntry!lookup loop\n");
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
  struct inode *ent_ptr)	/* pointer to inode if found */
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
}

/*===========================================================================*
 *				is_empty_dir				     *
 *===========================================================================*/
PUBLIC int is_empty_dir(struct inode *dirp) /* ptr to dir inode to search */
{
/* This function searches the directory whose inode is pointed to by 'dirp',
 * and return OK if only . and .. in dir, else ENOTEMPTY;
 */
  register union direntry_u * dp;
  register struct buf *bp = NULL;
  int r;
  off_t pos;
  block_t b;
  unsigned char slot_mark;

  /* If 'dirp' is not a pointer to a dir inode, error. */
  if ( ! IS_DIR(dirp) ) return(ENOTDIR);

  /* Do not even think about removing the root directory... */
  if ( dirp->i_flags & I_ROOTDIR ) return(EPERM);

  /* Step through the directory one block at a time. */
  pos = 0;
  dp = &bp->b_dir[0];
/*
 if ! root dir && pos = 0 && dp = [0] && dp->Name=".      " 
	continue;
 if ! root dir && pos = 0 && dp = [1] && dp->Name="..     " 
	continue;
 */
  while (TRUE) {
	b = bmap(dirp, pos);	/* get next block number */
	if (b == NO_BLOCK) {	/* no more data... */
/* FIXME: record the EOF? (+ i_flags) */
		return(OK);	/* it was empty! */
	}
	bp = get_block(dev, b, NORMAL);	/* get the directory block */
	assert(bp != NULL);

	/* Search a directory block. */
	for ( ; dp < &bp->b_dir[bcc.depblk]; dp++) {
		slot_mark = dp->d_direntry.deName[0];

		/* is the EndOfDirectory mark found? */
		if (slot_mark == SLOT_EMPTY) {
/* FIXME: record the EOF? (+ i_flags) */
			put_block(bp);
			return(OK);	/* it was empty! */
		}

		/* is this slot out of use? */
		if (slot_mark == SLOT_DELETED) {
			continue;
		}

/*
 if ! root dir && pos = 0 && dp = [0] && dp->Name=".      " 
	continue;
 if ! root dir && pos = 0 && dp = [1] && dp->Name="..     " 
	continue;
 */

		if(dp->d_direntry.deAttributes & ATTR_VOLUME) {
		/* skip any entry with volume attribute set;
		 * this includes any LFN entries, which we are
		 * not interested about at the moment
		 */
			continue;
		}

		/* everything else means we have an entry */
		put_block(bp);
		return(ENOTEMPTY); /* we are done */
	}

	/* The whole block has been searched. */
	put_block(bp);
	pos += bcc.bpblock;	/* continue searching dir */
	dp = &bp->b_dir[0];	/* from very start of block */
  }

/* Cannot happen!
 * Either we encounter the 0 (SLOT_EMPTY) marker,
 * or we exhausted the cluster chain.
 */
  panic("FATfs: broke out of is_empty_dir() loop\n");
}

/*===========================================================================*
 *				do_getdents				     *
 *===========================================================================*/
PUBLIC int do_getdents(void)
{
/* Return as much Directory ENTries as possible in the user buffer. */
  int r, i, done, len;
  struct inode *dirp, *newip;
  struct getdents_buf gdbuf;
  struct buf *bp;
  union direntry_u * dp;
  struct fat_direntry *fatdp;
  char filename[LFN_NAME_MAX + 1];
  struct fat_lfnentry lfnda[LFN_ORD+1];
  int count_lfn_entries;	/* count of entries in lfnda */
  int expected_lfn_ord;		/* next entry to fetch */
  struct direntryref dirref;	/* structure to locate entry later */
  off_t pos, off, block_pos, new_pos;
  unsigned char slot_mark, lfnChksum;

  /* Get the values from the request message. */
  reset_dents_buf(&gdbuf, (gid_t) m_in.REQ_GRANT, (size_t) m_in.REQ_MEM_SIZE);
  pos = (off_t) m_in.REQ_SEEK_POS_LO;

  DBGprintf(("FATfs: getdents in %lo, off %ld, maxsize:%u\n", m_in.REQ_INODE_NR, pos, (unsigned)m_in.REQ_MEM_SIZE));

  if( (dirp = fetch_inode((ino_t) m_in.REQ_INODE_NR)) == NULL) 
	return(EINVAL);
  /* We might work for a bit, reading blocks etc., so we will increase
   * the reference count of the inode (Check-me?)
   * 	  get_inode(rip);
   */

  done = FALSE;			/* Stop processing blocks when done is set */

  /* Check whether the position is properly aligned */
  if( (unsigned int) pos % DIR_ENTRY_SIZE)
	return(ENOENT);
  if (m_in.REQ_SEEK_POS_HI || pos<0 || pos>sb.maxFilesize) {
	done = TRUE;		/* position is too high */
  }

  off = pos & bcc.brelmask;	/* Offset in block */
  block_pos = pos - off;
  i = off / DIR_ENTRY_SIZE;
  expected_lfn_ord = count_lfn_entries = 0; /* reset LFN processing */

  r = OK;

#ifdef FAKE_DOT_ON_ROOT
  if (dirp->i_flags & I_ROOTDIR && pos == 0) {
  /* With FAT, the root directory does not have . and ..
   * So we need to fake them...
   */
	if ( (r=write_dent(&gdbuf, 0, ROOT_INODE_NR, ".")) != OK
	  || (r=write_dent(&gdbuf, 0, ROOT_INODE_NR, "..")) != OK)
		return(r);
  }
#endif /*FAKE_DOT_ON_ROOT*/

/* FIXME: with FAT, dir size are unknown */
/* The default position for the next request is EOF. If the user's buffer
 * fills up before EOF, new_pos will be modified.
 *
 * We may not believe the size registered in the inode, since
 * we are required to perform a full pass through all the entries anyway.
 *
 * FIXME: if sb.rootEntries*sb.depblk != sb.rootSiz (for fixed root dir),
 * we should NOT rely on the same logic (looking up anything),
 * and TRUST sb.rootEntries!
 */
/* CHECKME: this value is always erased... */
  new_pos = sb.maxFilesize;

  while (! done) {
	dirref.dr_absbn = bmap(dirp, pos); /* get next block number */
	if (dirref.dr_absbn == NO_BLOCK) { /* no more data... */
/* FIXME: record the EOF? (+ i_flags) */
/* FIXME: note that pos might be bigger;
 * and in fact, we should read the FAT cache to learn the allocated size...
 */
		new_pos = block_pos;
		done = TRUE;
		break;
	}
/* FIXME else augmente new_pos/i_size ? see above = maxfilesize... */
	bp = get_block(dev, dirref.dr_absbn, NORMAL); /* get directory block*/
	assert(bp != NULL);

	/* Search a directory block. */
	for (dp = &bp->b_dir[i];
	     i < bcc.depblk;
	     ++i, ++dp, pos += DIR_ENTRY_SIZE) {
		assert((void*) dp == (void*) &bp->b_data[pos & bcc.brelmask]);
		slot_mark=dp->d_direntry.deName[0];

DBGprintf(("FATfs: seen ['%.8s.%.3s'], #0=\\%.3o\n",
	dp->d_direntry.deName, dp->d_direntry.deExtension, slot_mark));

		/* is the EndOfDirectory mark found? */
		if (slot_mark == SLOT_EMPTY) {
			done = TRUE;
/* FIXME: record the EOF? (+ i_flags) */
			new_pos = pos;
			break;
		}

		assert(slot_mark == dp->d_lfnentry.lfnOrd);
		/* is this entry the next part of long name? */
		if ( slot_mark == expected_lfn_ord
		  && dp->d_lfnentry.lfnAttributes == ATTR_LFN
		  && dp->d_lfnentry.lfnChksum == lfnChksum) {
			/* Next (really previous) part of LFN filename;
			 * accumulate all the entries in the array
			 */
			lfnda[--expected_lfn_ord] = dp->d_lfnentry;
			continue;
		}

		/* is this slot out of use? */
		if (slot_mark == SLOT_DELETED) {
			expected_lfn_ord= count_lfn_entries= 0; /* reset LFN*/
			continue;
		}

		/* is this entry for long names? */
		if(dp->d_lfnentry.lfnAttributes == ATTR_LFN
		  && dp->d_lfnentry.lfnOrd & LFN_LAST
		  && ! (dp->d_lfnentry.lfnOrd & LFN_DELETED) ) {
			/* We found the last part of a long filename.
			 * Since long filenames can lay accross
			 * many entries, possibily crossing blocks,
			 * we will accumulate all the entries in
			 * an array (lfnda), and will process it later.
			 * expected_lfn_ord is the value of the next
			 * entry to be fetched.
			 */
			count_lfn_entries = dp->d_lfnentry.lfnOrd & LFN_ORD;
			expected_lfn_ord = count_lfn_entries;
			lfnda[--expected_lfn_ord] = dp->d_lfnentry;
			lfnChksum = dp->d_lfnentry.lfnChksum;
			continue;
		}

		if(dp->d_direntry.deAttributes & ATTR_VOLUME) {
		/* skip any entry with volume attribute set */
			expected_lfn_ord= count_lfn_entries= 0; /* reset LFN*/
			continue;
		}

		fatdp = & dp->d_direntry;
		if (count_lfn_entries) {
			/* we have LFN entries before*/
			if (expected_lfn_ord != 0
			 || lfnChksum != lfn_chksum(fatdp)) {
			/* not all the entries were there
			 * or the checksums do not match
			 */
				count_lfn_entries = 0;	/* invalid LFN */
			} else {
/* WORK NEEDED: convert lfnda -> filename
				len = conv_lfntoname(lfnda,
					filename, sizeof(filename) - 1);
 */
				count_lfn_entries = 0;	/* invalid LFN */
			}
		}

		if (count_lfn_entries == 0) {
		/* Either we do not have LFN entries before,
		 * or the file name was somewhat invalid;
		 * let try to parse the "short" 8.3 name
		 */
			assert(sizeof(filename) > NAME_MAX+1);
			if (conv_83toname(fatdp, filename) != CONV_OK) {
			/* Unable to convert 8.3 name into a name like
			 * the ones MINIX accepts (probably a / embedded).
			 * Skip the entry.
			 */
				DBGprintf(("FATfs: getdents unable to "
					"convert FAT entry <%.8s.%.3s>, skip\n",
					fatdp->deName, fatdp->deExtension));
				expected_lfn_ord = 0; /* reset LFN */
				continue;
			}
			len = strlen(filename);
		}

		assert(len > 0);
		if (len > NAME_MAX) {
		/* we discovered a name longer than what MINIX can handle;
		 * it would be a violation of POSIX to return a larger name
		 * and a potential cause of failure in user code;
		 * truncating is not a better option, since it is likely
		 * to cause problems later, like duplicate filenames.
		 * So we silently skip the entry.
		 */
#if 0
/* CHECKME... ? */
		 * So we end the request with an error.
		 */

			/* put_inode(dirp); */
			put_block(bp);
			return(EOVERFLOW);
#else
			DBGprintf(("FATfs: met directory entry with too "
					"large file name (%d), skip\n", len));
			expected_lfn_ord= count_lfn_entries= 0; /* reset LFN*/
			continue;
#endif
		}
		
		dirref.dr_entrypos = pos;
		dirref.dr_lfnpos = pos - count_lfn_entries * DIR_ENTRY_SIZE;
		newip = enter_as_inode(fatdp, dirp, &dirref);
		if( newip == NULL ) {
			DBGprintf(("FATfs: getdents cannot create some "
				"virtual inode, skip %.40s\n", filename));
			expected_lfn_ord= count_lfn_entries= 0; /* reset LFN*/
			continue;
		}
		r = write_dent(&gdbuf, pos, INODE_NR(newip), filename);
		put_inode(newip);
		if (r == -ENAMETOOLONG) {
		/* Record the position of this entry, it is the starting
		 * point of the next request (unless modified with lseek).
		 */
			new_pos = pos;
			done = TRUE;
			break;
		} else if (r != OK) {
			/* put_inode(dirp); */
			put_block(bp);
			return(r);
		}

		expected_lfn_ord = count_lfn_entries = 0; /* reset LFN */
	}

	put_block(bp);
	if(done)
		break;
	block_pos += bcc.bpblock;
	assert(block_pos == pos);
	i = 0;			/* on next loop, start at beginning of block*/
  }
			
  r = flush_dents_buf(&gdbuf);
  if (r == OK) {
	m_out.RES_NBYTES = dents_buf_written(&gdbuf);
	m_out.RES_SEEK_POS_LO = new_pos;
	m_out.RES_SEEK_POS_HI = m_in.REQ_SEEK_POS_HI;
	dirp->i_flags |= I_DIRTY | I_ACCESSED;
	dirp->i_atime = TIME_UPDATED; 
  }

/* CHECKME... */
  /* put_inode(dirp); */		/* release the inode */
  return(r);
}

/*===========================================================================*
 *				reset_dents_buf				     *
 *===========================================================================*/
PRIVATE void reset_dents_buf(struct getdents_buf * gdbufp, cp_grant_id_t gid, size_t size)
{
/* ... */

  gdbufp->callerbuf_gid = gid;
  gdbufp->callerbuf_size = size;
  gdbufp->callerbuf_off = 0;	/* Offset in the user's buffer */
  gdbufp->mybuf_off = 0;	/* Offset in getdents_buf */
  memset(gdbufp->dents_buf, '\0',GETDENTS_BUFSIZ); /* Avoid leaking any data*/
}

/*===========================================================================*
 *				flush_dents_buf				     *
 *===========================================================================*/
PRIVATE int flush_dents_buf(struct getdents_buf * gdbufp)
{
/* ... */
  int r;

  if(gdbufp->mybuf_off != 0) {
DBGprintf(("flush: off=%d->%d\n", gdbufp->mybuf_off, gdbufp->callerbuf_off));
	r = sys_safecopyto(VFS_PROC_NR, gdbufp->callerbuf_gid,
			   (vir_bytes) gdbufp->callerbuf_off,
			   (vir_bytes) gdbufp->dents_buf,
			   (size_t) gdbufp->mybuf_off, D);
	if (r != OK) return(r);

	gdbufp->callerbuf_off += gdbufp->mybuf_off;
	gdbufp->mybuf_off = 0;
	memset(gdbufp->dents_buf, '\0', GETDENTS_BUFSIZ);
  }
  return(OK);
}

/*===========================================================================*
 *				dents_buf_written			     *
 *===========================================================================*/
PRIVATE size_t dents_buf_written(struct getdents_buf * gdbufp)
{
/* Number of bytes written so far. */
  return(gdbufp->callerbuf_off);
}

/*===========================================================================*
 *				write_dent				     *
 *===========================================================================*/
PRIVATE int write_dent(
  struct getdents_buf * gdbufp,
  off_t entry_pos,
  ino_t ino,
  const char * name)
{
/* ... */
  struct dirent *dep;
  int r, o;
  int reclen;

  /* Compute record length */
  reclen = offsetof(struct dirent, d_name) + strlen(name) + 1;
  o = (reclen % sizeof(long));
  if (o != 0)
	reclen += sizeof(long) - o;
	/* the buffer has been cleared before */

  if (gdbufp->mybuf_off + reclen > GETDENTS_BUFSIZ) {
DBGprintf(("flush: off=%d->%d, reclen=%d, max=%u\n", gdbufp->mybuf_off, gdbufp->callerbuf_off, reclen, GETDENTS_BUFSIZ));
	r = flush_dents_buf(gdbufp);
	if (r != OK) return(r);
  }

  if(gdbufp->callerbuf_off + gdbufp->mybuf_off + reclen > gdbufp->callerbuf_size) {
DBGprintf(("not enough space: caller_off=%d, off=%d, reclen=%d, max=%u\n", gdbufp->callerbuf_off, gdbufp->mybuf_off, reclen, 
gdbufp->callerbuf_size));
	/* The user has no space for one more record */
	if (gdbufp->callerbuf_off == 0 && gdbufp->mybuf_off == 0)
	/* The user's buffer is too small for 1 record */
		return(EINVAL);
	else
		return(-ENAMETOOLONG);
  }

  dep = (struct dirent *) & gdbufp->dents_buf[gdbufp->mybuf_off];
  dep->d_ino = ino;
  dep->d_off = entry_pos;
  dep->d_reclen = (unsigned short) reclen;
  strlcpy(dep->d_name, name, GETDENTS_BUFSIZ-gdbufp->mybuf_off);
  gdbufp->mybuf_off += reclen;
  return(OK);
}
