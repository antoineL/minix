/* This file contains the procedures that look up names in the directory
 * system and determine the entry that goes with a given name.
 *
 *  The entry points into this file are
 *   lookup_dir		search for 'filename' and return inode #
 *   find_slot		enter a new entry in a given directory/inode
 *   is_empty_dir	return OK if only . and .. in dir else ENOTEMPTY
 *   do_getdents	perform the GETDENTS file system request
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

/* FIXME: goto const.h */
#ifndef GETDENTS_BUFSIZ
#define GETDENTS_BUFSIZ  (usizeof(struct dirent) + NAME_MAX + usizeof(long))
#endif

/* Private type: */
struct getdents_buf {
  char dents_buf[GETDENTS_BUFSIZ];
  size_t mybuf_off;
  cp_grant_id_t callerbuf_gid;
  size_t callerbuf_size, callerbuf_off;
};

/* Private functions:
 *   enter_as_inode	?
 */
FORWARD _PROTOTYPE( void reset_dents_buf,
	(struct getdents_buf *, cp_grant_id_t gid, size_t size)		);
FORWARD _PROTOTYPE( int flush_dents_buf, (struct getdents_buf *)	);
FORWARD _PROTOTYPE( size_t dents_buf_written, (struct getdents_buf *)	);
FORWARD _PROTOTYPE( int write_dent,
	(struct getdents_buf *, off_t, ino_t, const char * name)	);

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
  unsigned long pos;
  int r;
  unsigned char slot_mark, chksum;
  struct direntryref deref;	/* structure to locate entry later */
  struct fat_direntry direntry;	/* shortFN entry to match */
  struct fat_lfnentry lfnda[LFN_ORD+1]; /* LFN entries read */
  int count;			/* count of entries in lfnda */
  int ord;			/* LFN_ORD of next entry to fetch */

  /* If 'dirp' is not a pointer to a dir inode, error. */
  if ( ! IS_DIR(dirp) ) return(ENOTDIR);

/* FIXME: use some name cache/hash... */

/* FIXME: we currently assume FAT directories are always searchable...
 * A well-behaved way would be to honour use_dir_mask vs. credentials,
 * even more to honour the SYSTEM attribute and use_system_uid etc.
 */
  
  DBGprintf(("FATfs: lookup in dir=%lo, looking for <%s>...\n",
	INODE_NR(dirp), string));

  if (IS_ROOT(dirp)) {
	/* The root directory in a FAT file system does not have the
	 * '.'  and '..' entries. If we are asked after them, we
	 * need to give the correct answer.
	 * We take the granted opportunity to detect the ELEAVEMOUNT case.
	 */
	if (strcmp(string, ".") == 0 || strcmp(string, "..") == 0) {
		assert(res_inop);
		*res_inop = dirp;
		return string[1] == '.' ? ELEAVEMOUNT : OK;
	}
  }

  /* Build the equivalent (short name) directory entry */
  memset(&direntry, '\0', sizeof direntry);	/* Avoid leaking any data */
  switch (conv_nameto83(string, &direntry)) {
  case CONV_OK: case CONV_HASLOWER:
	/* we can use the 8.3 name entry for matching */
	break;
  default: /* CONV_NAMETOOLONG, CONV_TRAILINGDOT, CONV_INVAL */
	direntry.deName[0] = '\0'; /* prevent any possible matches */
	break;
  }

  /* Step through the directory one entry at a time.
   * It is done this way because in the case of a fixed root directory,
   * we must obey the sb.rootEntries as found in the first sector, and
   * we are not sure this number is a integer divisor of the number of
   * directory entries per block (bcc.depBlk).
   *
   * Also remember that except the particular case of the fixed root,
   * FAT directories do not have explicit sizes: end of directory lookup
   * is because there are no more clusters allocated to it, or because
   * we found the SLOT_EMPTY (which is a marker to not look further.)
   */
  ord = count = 0; /* reset LFN processing */
  for(pos = 0;
      dirp->i_flags & I_DIRNOTSIZED || pos<dirp->i_size;
      pos+=DIR_ENTRY_SIZE, dp++) {

	if ( (pos & bcc.bMask) == 0) {
		/* start of a new block */
		if (bp) put_block(bp);
		deref.dr_absbn = bmap(dirp, pos); /* next block number */
		if (deref.dr_absbn == NO_BLOCK) { /* no more data... */
/* FIXME: can be EIO... */
			break;
		}
		/* get directory block */
		bp = get_block(dev, deref.dr_absbn, NORMAL);
/* FIXME: can raise EIO... */
		dp = &bp->b_dir[0];
	}
	assert(bp != NULL);

	slot_mark=dp->d_direntry.deName[0];

	if ( slot_mark == SLOT_EMPTY)
		/* the EndOfDirectory mark is found */
		break;

	assert(slot_mark == dp->d_lfnentry.lfnOrd);
	/* is this entry the next part of long name? */
	if ( slot_mark == ord
	  && dp->d_lfnentry.lfnAttributes == ATTR_LFN
	  && dp->d_lfnentry.lfnChksum == chksum) {
		/* Next (really previous) part of LFN filename;
		 * accumulate all the entries in the array
		 */
		lfnda[--ord] = dp->d_lfnentry;
		continue;
	}

	if (slot_mark == SLOT_DELETED) {
		/* this slot is out of use */
		ord = count = 0; /* reset LFN*/
		continue;
	}

	/* is this entry for long names? */
	if ( dp->d_lfnentry.lfnAttributes == ATTR_LFN
	  && dp->d_lfnentry.lfnOrd & LFN_LAST
	  && ! (dp->d_lfnentry.lfnOrd & LFN_DELETED) ) {
		/* We found the last part of a long filename.
		 * Since long filenames can lay accross many entries,
		 * possibily crossing blocks, we will accumulate all the
		 * entries in an array (lfnda), and will process it later.
		 *
		 * ord is the value of the next entry to be fetched (1-N);
		 * it is also used as index into lfnda (0 to N-1).
		 */
		count = dp->d_lfnentry.lfnOrd & LFN_ORD;
		chksum = dp->d_lfnentry.lfnChksum;
		ord = count;
		lfnda[--ord] = dp->d_lfnentry;
		continue;
	}

	if(dp->d_direntry.deAttributes & ATTR_VOLUME) {
		/* skip any entry with volume attribute set */
		ord = count = 0; /* reset LFN*/
		continue;
	}

	/* so this entry is a regular one! */

	if ( count == 0		/* we do not have LFN entries before */
	  || ord != 0		/* or not all the LFN entries were there */
	  || chksum != lfn_chksum(& dp->d_direntry) ) {/*or chksums no match*/
		ord = count = 0; /* reset LFN*/
		/* let try to match the "short" 8.3 name */
		r = memcmp(dp->d_direntry.deName, direntry.deName, 8+3);
	} else
		r = comp_name_lfn(string, count, lfnda);
			
	if (r != 0) {
		/* names do not match */
		ord = count = 0; /* reset LFN*/
		continue;
	}

	/* we have a match! */
	deref.dr_entrypos = pos;
	deref.dr_lfnpos = pos - count*DIR_ENTRY_SIZE;
	assert(res_inop);
	*res_inop = direntry_to_inode(&dp->d_direntry, dirp, &deref);
	if( *res_inop == NULL ) {
/* FIXME: do something clever... */
		panic("FATfs: lookup_dir cannot create inode\n");
	}
	/* inode had its reference count incremented. */
	put_block(bp);
	return(OK);		/* work is done */
  }

  /* We broke out of the loop, or we reached the end of the directory.
   * In any case, the search is unsuccessful.
   */
  if (bp) put_block(bp);
  if (dirp->i_flags & I_DIRNOTSIZED) {
	dirp->i_size = pos;
	dirp->i_flags &= ~I_DIRNOTSIZED;
  }
  return(ENOENT);
}

/*===========================================================================*
 *				find_slots				     *
 *===========================================================================*/
PUBLIC int find_slots(struct inode *dirp, /* ptr to inode for dir to search */
  int slots,			/* number of needed slots (1+LFN entries) */
  struct direntryref *derefp)	/* pointer to structure to locate entry */
{
/* This function searches into the directory whose inode is pointed to by
 * 'dirp' for enough space to enter a new entry.
 * It will extend the directory if needed.
 */
  register union direntry_u * dp;
  register struct buf *bp = NULL;
  unsigned long pos, newsize;
  int count;			/* count of free entries found */
  unsigned char slot_mark;
  int r;
  block_t b;

  unsigned char chksum;
  struct direntryref deref;	/* structure to locate entry later */
  struct fat_direntry direntry;	/* shortFN entry to match */
  struct fat_lfnentry lfnda[LFN_ORD+1]; /* LFN entries read */
  int ord;			/* LFN_ORD of next entry to fetch */

  /* If 'dirp' is not a pointer to a dir inode, error. */
  if ( ! IS_DIR(dirp) ) return(ENOTDIR);

  assert(slots > 0);
  count = 0;

  /* Step through the directory one entry at a time. See notes above. */
  for(pos = 0;
      dirp->i_flags & I_DIRNOTSIZED || pos<dirp->i_size;
      pos+=DIR_ENTRY_SIZE, dp++) {

	if ( (pos & bcc.bMask) == 0) {
		/* start of a new block */
		if (bp) put_block(bp);
		deref.dr_absbn = bmap(dirp, pos); /* next block number */
		if (deref.dr_absbn == NO_BLOCK) { /* no more data... */
/* FIXME: can be EIO... */
		}
		/* get directory block */
		bp = get_block(dev, deref.dr_absbn, NORMAL);
/* FIXME: can raise EIO... */
		dp = &bp->b_dir[0];
	}
	assert(bp != NULL);

	slot_mark = dp->d_direntry.deName[0];

	if ( slot_mark == SLOT_EMPTY)
		/* the EndOfDirectory mark is found */
		break;

	if ( slot_mark == SLOT_DELETED
	  || ( dp->d_lfnentry.lfnAttributes == ATTR_LFN
	    && dp->d_lfnentry.lfnOrd & LFN_DELETED ) ) {
		/* this slot is free to use */
		if (++count >= slots)
			break;	/* found enough space */
		continue;
	}

	/* Note: a borderline case have not been considered: orphaned
	 * LFN entries without LFN_DELETED set will be considered in use
	 * while really they could be re-used; detection involves the
	 * 'ord' and 'count' variables' logic as used in lookup_dir().
	 *
	 * TODO: check if some important OS is leaving such orphans hanging.
	 */

	/* anything else is used in some way */
	count = 0;
  }

  if (bp) put_block(bp);

  if (count < slots) {
	/* Not enough contiguous free slots has been found so far.
	 * So we will append the new entry at the end.
	 * We might need to extend the directory's allocation.
	 */
	if ( (pos/DIR_ENTRY_SIZE) - count + slots
	     >= (dirp->i_clust==CLUST_ROOT ? sb.rootEntries : 1L<<16) )
		/* Do not go beyond the fixed size of FAT12/16 root directory.
		 * Also some FAT drivers have a 16-bit limit on the number of
		 * directory entries they can deal with. Do not martyrize them
		 */
		return(ENOSPC);

	newsize = pos + (slots - count) * DIR_ENTRY_SIZE;
	pos = newsize - DIR_ENTRY_SIZE;
	deref.dr_absbn = bmap(dirp, pos); /* block number for fat_direntry */
	if (deref.dr_absbn == NO_BLOCK) {
/* FIXME: can be EIO... */
		/* We need to try to extend directory on disk
		 * Note that the appended area is zeroed, so any lookup
		 * while creating the entries will not return garbage.
		 */
		assert(dirp->i_clust != CLUST_ROOT);
		if ( (r = extendfileclear(dirp, newsize, NULL, NULL)) != OK)
			return(r);
		deref.dr_absbn = bmap(dirp, pos);
		if (deref.dr_absbn == NO_BLOCK) {
/* FIXME: can be EIO... */
/* FIXME: do something clever... */
			panic("FATfs: find_slots cannot extend file flawlessly\n");
		}
	}
	if ( (b = bmap(dirp, newsize)) != NO_BLOCK) {
		/* There is more space in the allocated space after our new
		 * entry; so we need to write the new EndOfDirectory mark.
		 */
		bp = get_block(dev, b, NORMAL);
		if (bp==NULL) {
/* FIXME: can raise EIO... */
/* FIXME: do something clever... */
			panic("FATfs: find_slots cannot get block\n");
		}
#if 0 /* overkill */
		memset(&bp->b_dir[newsize & bcc.bMask], 0, 
			bcc.depBlock - (newsize & bcc.bMask));
#else
		bp->b_dir[newsize & bcc.bMask].d_direntry.deName[0] = SLOT_EMPTY;
#endif
		bp->b_dirt = DIRTY;
		if (bp) put_block(bp);
	}
  }

  /* All is in place. Gives all the details to caller. */
  deref.dr_parent = dirp->i_clust;
  deref.dr_entrypos = pos;
  deref.dr_lfnpos = pos - (count-1)*DIR_ENTRY_SIZE;
  if (derefp) *derefp = deref;

  return(OK);
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
  if (IS_ROOT(dirp)) return(EPERM);

  /* Step through the directory one block at a time.
   * Note we avoided the case of the root directory above.
   */
  pos = 0;
  while (TRUE) {
	b = bmap(dirp, pos);	/* get next block number */
	if (b == NO_BLOCK) {	/* no more data... */
		return(OK);	/* it was empty indeed! */
	}
	bp = get_block(dev, b, NORMAL);	/* get the directory block */
	assert(bp != NULL);

	dp = &bp->b_dir[0];	/* start from beginning of block */

	if (pos==0) {
		/* Skip entries '.' and '..' at start */
		if ( memcmp(dp->d_direntry.deName, NAME_DOT, 8+3) == 0
		  && ! (dp->d_direntry.deAttributes & ATTR_DIRECTORY) )
			++dp;
		if ( memcmp(dp->d_direntry.deName, NAME_DOT_DOT, 8+3) == 0
		  && ! (dp->d_direntry.deAttributes & ATTR_DIRECTORY) )
			++dp;
	}

	/* Search a directory block. */
	for ( ; dp < &bp->b_dir[bcc.depBlock]; dp++) {
		slot_mark = dp->d_direntry.deName[0];

		/* is the EndOfDirectory mark found? */
		if (slot_mark == SLOT_EMPTY) {
			put_block(bp);
			return(OK);	/* it was empty! */
		}

		/* is this slot out of use? */
		if (slot_mark == SLOT_DELETED) {
			continue;
		}

		if (dp->d_direntry.deAttributes & ATTR_VOLUME) {
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
	pos += bcc.bpBlock;	/* continue searching dir */
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
  union direntry_u * dp;
  struct buf *bp = NULL;
  unsigned long pos, off;
  int r, iX;
  enum {BEGIN, LOOPING, BUFFULL, END} state;
  unsigned char slot_mark, chksum;
  struct inode *dirp, *newip;
  struct getdents_buf gdbuf;
  char filename[LFN_NAME_MAX + 1];
  size_t len;
  struct direntryref deref;	/* structure to locate entry later */
  struct fat_lfnentry lfnda[LFN_ORD+1]; /* LFN entries read */
  int count;			/* count of entries in lfnda */
  int ord;			/* LFN_ORD of next entry to fetch */

  struct fat_direntry direntry;	/* shortFN entry to match */

  /* Get the values from the request message. */
  reset_dents_buf(&gdbuf, (gid_t) m_in.REQ_GRANT, (size_t) m_in.REQ_MEM_SIZE);
  pos = (unsigned long) m_in.REQ_SEEK_POS_LO;

  DBGprintf(("FATfs: getdents in %lo, off %ld, maxsize:%u\n", m_in.REQ_INODE_NR, pos, (unsigned)m_in.REQ_MEM_SIZE));

  if( (dirp = fetch_inode((ino_t) m_in.REQ_INODE_NR)) == NULL) 
	return(EINVAL);
  /* We might work for a bit, reading blocks etc., so we will increase
   * the reference count of the inode (Check-me?)
   * 	  get_inode(rip);
   */

  state = BEGIN;		/* Initialize satte machine */

  /* Check whether the position is properly aligned */
  if( (unsigned int) pos % DIR_ENTRY_SIZE)
	return(ENOENT);
  if (m_in.REQ_SEEK_POS_HI || pos<0 || pos>sb.maxFilesize) {
	state = END;		/* position is too high */
  }
#if 0
  off = pos & bcc.bMask;	/* Offset in block */
  i = (pos & bcc.bMask) / DIR_ENTRY_SIZE;
#endif
  r = OK;

#ifdef FAKE_DOT_ON_ROOT
  if (IS_ROOT(dirp) && pos == 0) {
  /* With FAT, the root directory does not have . and ..
   * So we need to fake them...
   */
	if ( (r=write_dent(&gdbuf, 0, ROOT_INODE_NR, ".")) != OK
	  || (r=write_dent(&gdbuf, 0, ROOT_INODE_NR, "..")) != OK)
		return(r);
  }
#endif /*FAKE_DOT_ON_ROOT*/

/* FIXME: we currently assume FAT directories are always readable...
 * A well-behaved way would be to honour use_dir_mask vs. credentials,
 * even more to honour the SYSTEM attribute and use_system_uid etc.
 */
  
  /* Step through the directory one entry at a time. */
  ord = count = 0; /* reset LFN processing */
  for(;
      state<=LOOPING && (dirp->i_flags & I_DIRNOTSIZED || pos<dirp->i_size);
      pos+=DIR_ENTRY_SIZE, dp++) {

	if ( state == BEGIN || (pos & bcc.bMask) == 0) {
		/* start of a new block */
		if (bp) put_block(bp);
		deref.dr_absbn = bmap(dirp, pos); /* next block number */
		if (deref.dr_absbn == NO_BLOCK) { /* no more data... */
/* FIXME: can be EIO... */
			state = END;
			break;
		}
		/* get directory block */
		bp = get_block(dev, deref.dr_absbn, NORMAL);
/* FIXME: can raise EIO... */
		dp = &bp->b_dir[(pos & bcc.bMask) / DIR_ENTRY_SIZE];
		state = LOOPING;
	}
	assert(bp != NULL);

	slot_mark=dp->d_direntry.deName[0];

	if ( slot_mark == SLOT_EMPTY) {
		/* the EndOfDirectory mark is found */
		state = END;
		break;
	}

	assert(slot_mark == dp->d_lfnentry.lfnOrd);
	/* is this entry the next part of long name? */
	if ( slot_mark == ord
	  && dp->d_lfnentry.lfnAttributes == ATTR_LFN
	  && dp->d_lfnentry.lfnChksum == chksum) {
		/* Next (really previous) part of LFN filename;
		 * accumulate all the entries in the array
		 */
		lfnda[--ord] = dp->d_lfnentry;
		continue;
	}

	if (slot_mark == SLOT_DELETED) {
		/* this slot is out of use */
		ord = count = 0; /* reset LFN*/
		continue;
	}

	/* is this entry for long names? */
	if ( dp->d_lfnentry.lfnAttributes == ATTR_LFN
	  && dp->d_lfnentry.lfnOrd & LFN_LAST
	  && ! (dp->d_lfnentry.lfnOrd & LFN_DELETED) ) {
		/* We found the last part of a long filename.
		 * Since long filenames can lay accross many entries,
		 * possibily crossing blocks, we will accumulate all the
		 * entries in an array (lfnda), and will process it later.
		 *
		 * ord is the value of the next entry to be fetched (1-N);
		 * it is also used as index into lfnda (0 to N-1).
		 */
		count = dp->d_lfnentry.lfnOrd & LFN_ORD;
		chksum = dp->d_lfnentry.lfnChksum;
		ord = count;
		lfnda[--ord] = dp->d_lfnentry;
		continue;
	}

	if(dp->d_direntry.deAttributes & ATTR_VOLUME) {
		/* skip any entry with volume attribute set */
		ord = count = 0; /* reset LFN*/
		continue;
	}

	/* so this entry is a regular one! */

	if ( count == 0		/* we do not have LFN entries before */
	  || ord != 0		/* or not all the LFN entries were there */
	  || chksum != lfn_chksum(& dp->d_direntry) ) {/*or chksums no match*/
		ord = count = 0; /* reset LFN*/
		assert(sizeof(filename) > NAME_MAX+1);
		if (conv_83toname(&dp->d_direntry, filename) != CONV_OK) {
			/* Unable to convert 8.3 name into a name like
			 * the ones MINIX accepts (probably a / embedded).
			 * Skip the entry.
			 */
			DBGprintf(("FATfs: getdents unable to convert "
				"FAT entry <%.8s.%.3s>, skip\n",
				dp->d_direntry.deName,
				dp->d_direntry.deExtension));
			continue;
		}
		len = strlen(filename);
	} else {
		len = sizeof(filename);
		if (conv_lfntoname(count, lfnda, filename, &len) != CONV_OK) {
			/* Unable to convert long file name into a name like
			 * the ones MINIX accepts (probably a / embedded).
			 * Very strange. Skip the entry.
			 */
			DBGprintf(("FATfs: getdents unable to "
				"convert LFN entry, skip\n"));
			continue;
		}
	}

	assert(len > 0);
	if (len > NAME_MAX) {
		/* we discovered a name longer than what MINIX can handle;
		 * it would be a violation of POSIX to return a larger name
		 * and a potential cause of failure in user code;
		 * truncating is not a better option, since it is likely
		 * to cause problems later, like duplicate filenames;
		 * returning an error like EOVERFLOW, as Posix is suggesting,
		 * is not really better, since it will prevent enumerating all
		 * the valid entries after this one.
		 * So we silently skip the entry.
		 */
		DBGprintf(("FATfs: met directory entry with too "
				"large file name (%d), skip\n", len));
		ord= count= 0; /* reset LFN*/
		continue;
	}
	
	deref.dr_entrypos = pos;
	deref.dr_lfnpos = pos - count*DIR_ENTRY_SIZE;
	newip = direntry_to_inode(&dp->d_direntry, dirp, &deref);
	if( newip == NULL ) {
		DBGprintf(("FATfs: getdents cannot create some "
			"virtual inode, skip %.40s\n", filename));
		ord= count= 0; /* reset LFN*/
		continue;
	}
	r = write_dent(&gdbuf, pos, INODE_NR(newip), filename);
	put_inode(newip);
	if (r == -ENAMETOOLONG) {
	/* Record the position of this entry, it is the starting
	 * point of the next request (unless modified with lseek).
	 */
		state = BUFFULL;
		break;
	} else if (r != OK) {
	/* some error occurred while writing in buffer; inform VFS.*/
		/* put_inode(dirp); */
		put_block(bp);
		return(r);
	}

	ord = count = 0; /* reset LFN for next iteration */
  }

  /* We broke out of the loop, or we reached the end of the directory,
   * or the user buffer filled up. In any case, the work is done.
   */
  if (bp) put_block(bp);

  if (state==END && (dirp->i_flags & I_DIRNOTSIZED) ) {
	/* we reached the end of the directory, remember it */
	dirp->i_size = pos;
	dirp->i_flags &= ~I_DIRNOTSIZED;
  }

  r = flush_dents_buf(&gdbuf);
  if (r == OK) {
	m_out.RES_NBYTES = dents_buf_written(&gdbuf);
	m_out.RES_SEEK_POS_LO = pos;
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
	r = flush_dents_buf(gdbufp);
	if (r != OK) return(r);
  }

  if(gdbufp->callerbuf_off + gdbufp->mybuf_off + reclen > gdbufp->callerbuf_size) {
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
