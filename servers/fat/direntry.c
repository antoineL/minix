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
 * Warning: this code is not reentrant (use static local variables, without mutex)
 *
 * Auteur: Antoine Leca, septembre 2010.
 * Updated:
 */
 
#include "inc.h"

#include <ctype.h>
#include <stddef.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

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

/* Private global variables: */
  /* working buffer to accumulate to-be-returned direcoty entries */
PRIVATE char getdents_buf[GETDENTS_BUFSIZ];

/* Private functions:
 *   enter_as_inode	?
 *   nameto83		?
 */
FORWARD _PROTOTYPE( struct inode *enter_as_inode,
		(struct fat_direntry*, struct inode* dirp, unsigned)	);
FORWARD _PROTOTYPE( int nameto83,
		(char string[NAME_MAX+1], struct fat_direntry *)	);

/* warning: the following lines are not failsafe macros */
#define	get_le16(arr) ((u16_t)( (arr)[0] | ((arr)[1]<<8) ))
#define	get_le32(arr) ( get_le16(arr) | ((u32_t)get_le16((arr)+2)<<16) )

/*===========================================================================*
 *				do_getdents				     *
 *===========================================================================*/
PUBLIC int do_getdents(void)
{
/* Return as much Directory ENTries as possible in the user buffer. */
  int r, i, o, done;
  struct inode *dirp, *newip;
  struct buf *bp;
  union direntry_u * dp;
  struct fat_direntry *fatdp;
  int len, namelen, extlen, reclen;
  block_t b;
  ino_t ino;
  cp_grant_id_t gid;
  size_t size, mybuf_off, callerbuf_off;
  off_t pos, off, block_pos, new_pos, ent_pos;
  struct dirent *dep;
  char *cp;
  unsigned char slot_mark;

  gid = (gid_t) m_in.REQ_GRANT;
  size = (size_t) m_in.REQ_MEM_SIZE;
  pos = (off_t) m_in.REQ_SEEK_POS_LO;

  DBGprintf(("FATfs: getdents in %lo, off %ld, maxsize:%u\n", m_in.REQ_INODE_NR, pos, size));

/* Check whether the position is properly aligned */
  if( (unsigned int) pos % DIR_ENTRY_SIZE)
	return(ENOENT);

/* Get the values from the request message.
 * We might work for a bit, reading blocks etc., so we will increase
 * the reference count of the inode (Check-me?)
 */
  if( (dirp = fetch_inode((ino_t) m_in.REQ_INODE_NR)) == NULL) 
	return(EINVAL);

  off = pos & sb.brelmask;	/* Offset in block */
  block_pos = pos - off;
  done = FALSE;		/* Stop processing blocks when done is set */
  r = OK;

  mybuf_off = 0;	/* Offset in getdents_buf */
  memset(getdents_buf, '\0', GETDENTS_BUFSIZ);	/* Avoid leaking any data */
  callerbuf_off = 0;	/* Offset in the user's buffer */

#ifdef FAKE_DOT_ON_ROOT
  if (dirp->i_flags & I_ROOTDIR && pos == 0) {
  /* With FAT, the root directory does not have . and ..
   * So we need to fake them...
   */
	/* Compute record length */
	reclen = offsetof(struct dirent, d_name) + 1 + 1;
	o = (reclen % sizeof(long));
	if (o != 0)
		reclen += sizeof(long) - o;
		/* the buffer has been cleared before */

	dep = (struct dirent *) &getdents_buf[mybuf_off];
	dep->d_ino = 1;
	dep->d_off = 0;
	dep->d_reclen = (unsigned short) reclen;
	strcpy(dep->d_name, ".");
	mybuf_off += reclen;

	/* Compute record length */
	reclen = offsetof(struct dirent, d_name) + 2 + 1;
	o = (reclen % sizeof(long));
	if (o != 0)
		reclen += sizeof(long) - o;
		/* the buffer has been cleared before */

	dep = (struct dirent *) &getdents_buf[mybuf_off];
	dep->d_ino = 1;
	dep->d_off = 0;
	dep->d_reclen = (unsigned short) reclen;
	strcpy(dep->d_name, "..");
	mybuf_off += reclen;
  }
#endif /*FAKE_DOT_ON_ROOT*/

/* FIXME: with FAT, dir size are unknown */
/* The default position for the next request is EOF. If the user's buffer
 * fills up before EOF, new_pos will be modified.
 */
/* We do not believe the size registered in the inode, since
 * we are required to perform a full pass through all the entries anyway.
 */
  new_pos = sb.maxFilesize;

/*
  for(; block_pos < dirp->i_size; block_pos += block_size) {
 */
  while (TRUE) {
	b = bmap(dirp, block_pos);	/* get next block number */
	if (b == NO_BLOCK) {	/* no more data... */
/* FIXME: record the EOF? (+ i_flags) */
/* FIXME: update new_pos ? */
		new_pos = block_pos;
		done = TRUE;
		break;
	}
/* FIXME else augmente new_pos ? */
	bp = get_block(dev, b, NORMAL);	/* get the directory block */

	assert(bp != NULL);

	/* Search a directory block. */
#if 0
	if (block_pos < pos)
		dp = &bp->b_dir[off / DIR_ENTRY_SIZE];
	else
		dp = &bp->b_dir[0];

	for (; dp < &bp->b_dir[block_size / DIR_ENTRY_SIZE]; dp++) {
#else
	if (block_pos < pos)
		i = off / DIR_ENTRY_SIZE;
	else
		i = 0;

	for (dp = &bp->b_dir[i]; i < sb.depblk; ++i, ++dp) {
/* FIXME ajoute ent_pos+=DIR_ENTRY_SIZE */
#endif

DBGprintf(("FATfs: seen ['%.8s.%.3s'], #0=\\%.3o\n",
	dp->d_direntry.deName, dp->d_direntry.deExtension, dp->d_direntry.deName[0]));

		/* is the EndOfDirectory mark found? */
		if ( (slot_mark=dp->d_direntry.deName[0]) == SLOT_EMPTY) {
			done = TRUE;
/* FIXME: record the EOF? (+ i_flags) */
			new_pos = block_pos + ((char *) dp - (char *) bp->b_dir);
			break;
		}
		/* is this slot out of use? */
		if (slot_mark == SLOT_DELETED) {
			continue;
		}

		/* Need the position of this entry in the directory */
#if 0	/* FIXME why does not work? */
		ent_pos = block_pos +
			((char *) dp->d_direntry - (char *) bp->/*FIXME? b_dir */ b_data);
#else
		ent_pos = block_pos + ((char *) dp - (char *) bp->b_dir);
#endif

		/* is this entry for long names? */
		if(dp->d_lfnentry.lfnAttributes == ATTR_LFN) {
/* WORK NEEDED! */
			continue;
		}
		if(dp->d_direntry.deAttributes & ATTR_VOLUME) {
		/* skip any entry with volume attribute set */
			continue;
		}

		fatdp = & dp->d_direntry;
		{
			cp = (char*) &fatdp->deName[8];
/*DBGprintf(("cp[-8] = '%.8s' ", cp-8));*/
			while (--cp >= (char*)&fatdp->deName[1])
				if (*cp != ' ') break;
			namelen = cp - (char*)fatdp->deName + 1;
			assert(namelen > 0);
			assert(namelen <= 8);
	
			cp = (char*) &fatdp->deExtension[3];
/*DBGprintf(("cp[-3] = '%.3s' ", cp-3));*/
			while (--cp >= (char*)&fatdp->deExtension[0])
				if (*cp != ' ') break;
			extlen = cp - (char*)fatdp->deExtension + 1;
			assert(extlen >= 0);
			assert(extlen <= 3);
			len = namelen + (extlen ? 1 + extlen : 0);
DBGprintf(("calc.len = %d+%d=%d\n", namelen, extlen, len));
/* FIXME: lcase */
		}
		assert(len > 0);
		if (len > NAME_MAX) {
		/* we discovered a name longer than what MINIX can handle;
		 * it would be a violation of POSIX to return a larger name
		 * and a potential cause of failure in user code;
		 * truncating is not a better option, since it is likely
		 * to cause problems later, like duplicate filenames.
		 * So we end the request with an error.
		 */
/* CHECKME... */
			/* put_inode(dirp); */
			put_block(bp);
			return(EOVERFLOW);
		}
/* FIXME check \0 or / => error */
		
		/* Compute record length */
		reclen = offsetof(struct dirent, d_name) + len + 1;
		o = (reclen % sizeof(long));
		if (o != 0)
			reclen += sizeof(long) - o;
			/* the buffer has been cleared before */

		if (mybuf_off + reclen > GETDENTS_BUFSIZ) {
DBGprintf(("flush: off=%d->%d, reclen=%d, max=%u\n", mybuf_off, callerbuf_off, reclen, GETDENTS_BUFSIZ));
			/* flush my buffer */
			r = sys_safecopyto(VFS_PROC_NR, gid,
					   (vir_bytes) callerbuf_off, 
					   (vir_bytes) getdents_buf,
					   (size_t) mybuf_off, D);
			if (r != OK) {
				/* put_inode(dirp); */
				put_block(bp);
				return(r);
			}

			callerbuf_off += mybuf_off;
			mybuf_off = 0;
			memset(getdents_buf, '\0', GETDENTS_BUFSIZ);
		}
		
		if(callerbuf_off + mybuf_off + reclen > size) {
DBGprintf(("not enough space: caller_off=%d, off=%d, reclen=%d, max=%u\n", callerbuf_off, mybuf_off, reclen, size));
			/* The user has no space for one more record */
			if (callerbuf_off == 0 && mybuf_off == 0) {
			/* The user's buffer is too small for 1 record */
/* CHECKME: can we even directly  return(EINVAL);  ?*/
				r = EINVAL;
			} else {
			/* Record the position of this entry, it is the
			 * starting point of the next request (unless the
			 * position is modified with lseek).
			 */
				new_pos = ent_pos;
			}
			done = TRUE;
			break;
		}

		dep = (struct dirent *) &getdents_buf[mybuf_off];
#if 1
		newip = enter_as_inode(fatdp, dirp, ent_pos);
		if( newip == NULL ) {
/* FIXME: do something more clever... */
			panic("FATfs: getdents cannot create some virtual inode\n");
		}
		dep->d_ino = INODE_NR(newip);
		put_inode(newip);
#elif 0
/* FIXME FAT32 */
		ino = get_le16(fatdp->deStartCluster);
		if (ino == 0) {
/* HACK HACK HACK
 * cannot give the "real" inode given when the file will be opened (lookup)
 * because we have no idea of the generation number...
 * So we enter it anyway in the inode array, hoping it will survive...
 */
			newip = enter_as_inode(fatdp, dirp, ent_pos);
			if( newip == NULL ) {
/* FIXME: do something more clever... */
				panic("FATfs: getdents cannot create some virtual inode\n");
			} else
				ino = INODE_NR(newip);
			put_inode(newip);
		}
		dep->d_ino = ino;
#else
		newip = dirref_to_inode(dirp->i_clust, ent_pos);
		if (newip == NULL) {
			newip = enter_as_inode(fatdp, dirp, ent_pos);
			if( newip == NULL ) {
/* FIXME: do something more clever... */
				panic("FATfs: getdents cannot create some virtual inode\n");
			} else
				get_inode(newip);
		}
		dep->d_ino = INODE_NR(newip);
		put_inode(newip);
#endif
		dep->d_off = ent_pos;
		dep->d_reclen = (unsigned short) reclen;

		{
			cp = &dep->d_name[0];
/* FIXME: lcase */
DBGprintf(("FATfs: copying '%.*s' to %.8p (buf+%d)", namelen, fatdp->deName, cp, cp-getdents_buf));

			memcpy(cp, fatdp->deName, namelen);
			if (slot_mark == SLOT_E5)
				*cp = SLOT_DELETED;	/*DOS was hacked too*/
			cp += namelen;
			if (extlen) {
				*cp++ = '.';
/* FIXME: lcase */
DBGprintf((", '%.*s' to %.8p (buf+%d)", extlen, fatdp->deExtension, cp, cp-getdents_buf));
				memcpy(cp, fatdp->deExtension, extlen);
cp += extlen;
			}
		}
DBGprintf((" till %.8p?=%.8p (buf+%d) - reclen=%d\n", &dep->d_name[len], cp, cp-getdents_buf, reclen));
		dep->d_name[len] = '\0';
		mybuf_off += reclen;
	}

	put_block(bp);
	if(done)
		break;
	block_pos += block_size;
  }

  if(mybuf_off != 0) {
DBGprintf(("flush final: off=%d->%d\n", mybuf_off, callerbuf_off));
	r = sys_safecopyto(VFS_PROC_NR, gid, (vir_bytes) callerbuf_off,
			   (vir_bytes) getdents_buf, (size_t) mybuf_off, D);
	if (r != OK) {
		/* put_inode(dirp); */
		return(r);
	}

	callerbuf_off += mybuf_off;
  }

  if(r == OK) {
	m_out.RES_NBYTES = callerbuf_off;
	m_out.RES_SEEK_POS_LO = new_pos;
	dirp->i_flags |= I_DIRTY | I_ACCESSED;
DBGprintf(("OK result: off=%d, next=%ld\n", callerbuf_off, new_pos));
  }

/* CHECKME... */
  /* put_inode(dirp); */		/* release the inode */
  return(r);
}

/*===========================================================================*
 *				enter_as_inode				     *
 *===========================================================================*/
PRIVATE struct inode *enter_as_inode(
  struct fat_direntry * dp,	/* (short name) entry */
  struct inode * dirp,		/* parent directory */
  unsigned entrypos)		/* position within the parent directory */
{
/* Enter the inode as specified by its directory entry.
 * Return the inode with its reference count incremented.

FIXME: need the struct direntryref
->i_clust
FIXME: refcount?
 */
  struct inode *rip;
  cluster_t clust;

#if 0
  DBGprintf(("FATfs: enter_inode ['%.8s.%.3s']\n", dp->deName, dp->deExtension));
#endif

/* FIXME FAT32 */
  clust = get_le16(dp->deStartCluster);

  if (clust) {
	if ( (rip = cluster_to_inode(clust)) != NULL ) {
	/* found in inode cache */
		get_inode(rip);
		return(rip);
	}
  } else /*clust==0*/ {
	if (dp->deAttributes & ATTR_DIRECTORY && dirp->i_flags&I_ROOTDIR ) {
	/* FAT quirks: the .. entry of 1st-level subdirectories
	 * (which points to the root directory) has 0 as starting cluster!
	 */
		if (memcmp(dp->deName, NAME_DOT_DOT, 8+3) != 0) {
			/* something is wrong... */
  DBGprintf(("FATfs: enter_inode ['%.8s.%.3s'], clust=%d, BUGGY\n", dp->deName, dp->deExtension, clust));
			goto buggy_cluster0;
		}
		get_inode(dirp);
		return(dirp);
	}
	if ( (rip = dirref_to_inode(INODE_NR(dirp), entrypos)) != NULL ) {
	/* found in inode cache (alternative way) */
		get_inode(rip);
		return(rip);
	}
  }

buggy_cluster0:
  /* get a fresh inode with ref. count = 1 */
  rip = get_free_inode();
  rip->i_flags = 0;
  rip->i_direntry = *dp;
  rip->i_clust = clust;
  rip->i_parent_clust = dirp->i_clust;
  rip->i_entrypos = entrypos;
  rip->i_size = get_le32(dp->deFileSize);
  if ( (dp->deAttributes & ATTR_DIRECTORY) == ATTR_DIRECTORY)  {
	rip->i_flags |= I_DIR;	/* before call to get_mode */
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
		rip->i_size == sb.bpcluster;
		rip->i_flags |= I_DIRNOTSIZED;
	}
/* FIXME: Warn+fail is i_clust==0; need to clean the error protocol
 */
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
 *				nameto83				     *
 *===========================================================================*/
PRIVATE int nameto83(
  char string[NAME_MAX+1],	/* some file name (passed by VFS) */
  struct fat_direntry *fatdp)	/* where to put the 8.3 compliant equivalent
				 * with resulting LCase attribute also */
{
/* Transforms a filename (as part of a path) into the form suitable to be
 * used in FAT directory entries, named 8.3 because it could only have up
 * to 8 characters before the (only) dot, and 3 characters after it.
 * This functions returns EINVAL if invalid characters were encountered,
 * and ENAMETOOLONG if it does not fit the 8.3 limits.
 */
  const unsigned char *p = (const unsigned char *)string;
  unsigned char *q;
  int prev, next, haslower, hasupper;

  memset(fatdp->deName, ' ', 8+3);	/* fill with spaces */
  fatdp->deLCase &= ~ (LCASE_NAME|LCASE_EXTENSION);

  switch (next = *p) {
  case '.':
	if (p[1] == '\0') {
		memcpy(fatdp->deName, NAME_DOT, 8+3);
		return(OK);
	} else if (p[1] == '.' && p[2] == '\0') {
		memcpy(fatdp->deName, NAME_DOT_DOT, 8+3);
		return(OK);
	} else
	/* cannot store a filename starting with . */
		return(EINVAL);
  case '\0':
	/* filename staring with NUL ? */
	DBGprintf(("FATfs: passed filename starting with \\0\n"));
	return(EINVAL);
  case ' ':
	/* filename staring with space; legal but slippery */
	DBGprintf(("FATfs: warning: passed filename starting with space\n"));
	break;
  case SLOT_E5:
	DBGprintf(("FATfs: warning: initial character \\05 in a filename, "
		"will be mangled into \\xE5\n"));
  case SLOT_DELETED:
	next = SLOT_E5;
	break;
  }

  haslower = hasupper = prev = 0;
  for (q=&fatdp->deName[0]; q < &fatdp->deName[8]; ++q) {
	if (next == '.' || next == '\0') {
		if (prev == ' ') {
			DBGprintf(("FATfs: warning: final space character "
				"in a name, will be dropped.\n"));
		}
		break;
	} else if (strchr(FAT_FORBIDDEN_CHARS, next) != NULL) {
		/* some characters cannot enter in FAT filenames */
		return(EINVAL);
	} else {
		hasupper |= isupper(next);
		haslower |= islower(next);
		*q = toupper(next);
	}
	prev = next;
	next = *++p;
  }
/* FIXME: move to caller when creating... */
/* check q-d==3 &&
   static const char *dev3[] = {"CON", "AUX", "PRN", "NUL", "   "};
 * check q-d==4 && isdigit(q[-1])
   static const char *dev4[] = {"COM", "LPT" };
 */
  if (haslower && !hasupper)
	fatdp->deLCase |= LCASE_NAME;
/* FIXME: register the case haslower, which asks for LFN to store name... */

  if (*p == '\0') {
	/* no extension */
	return(OK);
  }
  if (*p++ != '.') {
	/* more than 8 chars in name... */
	return(ENAMETOOLONG);
  }
  next = *p;
  if (next == '\0') {
	/* initial filename was "foobar." (trailing dot)
	 * DOS and Windows recognize it the same as "foobar"
	 */
	DBGprintf(("FATfs: filename \"%s\" has a trailing dot...\n",
			string));
	return(OK);	/* (EINVAL); */	/* FIXME: check what to do here */
  }

  haslower = hasupper = prev = 0;
  for (q=&fatdp->deExtension[0]; q < &fatdp->deExtension[3]; ++q) {
	if (next == '\0') {
		break;
	} else if (strchr(FAT_FORBIDDEN_CHARS, next) != NULL) {
		/* some characters cannot enter in FAT filenames */
		return(EINVAL);
	} else {
		hasupper |= isupper(next);
		haslower |= islower(next);
		*q = toupper(next);
	}
	prev = next;
	next = *++p;
  }

  if (next == '\0') {
	if (prev == ' ') {
		DBGprintf(("FATfs: warning: final space character "
			"in an extension, will be dropped.\n"));
	}
	if (haslower && !hasupper)
		fatdp->deLCase |= LCASE_EXTENSION;
	return(OK);
  }
  /* more than 3 chars in extension... */
  return(ENAMETOOLONG);
}

/*===========================================================================*
 *				lookup_dir				     *
 *===========================================================================*/
PUBLIC int lookup_dir(
  register struct inode *dirp,	/* ptr to inode for dir to search */
  char string[NAME_MAX],	/* component to search for */
  struct inode **res_inop)	/* pointer to inode if found */
{
/* This function searches the directory whose inode is pointed to by 'dir',
 * for entry named 'string', and return ptr to inode in (*res_inop).
 */
  register union direntry_u * dp;
  register struct buf *bp = NULL;
  struct fat_direntry direntry;
  int r;
  off_t pos;
  block_t b;
  ino_t ino;
  unsigned char slot_mark;

  /* If 'dirp' is not a pointer to a dir inode, error. */
  if ( (dirp->i_Attributes & ATTR_DIRECTORY) != ATTR_DIRECTORY)  {
	return(ENOTDIR);
  }

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
  if ( (r = nameto83(string, &direntry)) != OK) {
/* WORK NEEDED: LFN... */
	DBGprintf(("FATfs: lookup_dir for %s, "
		"not OK for 8.3, return %d\n", string, r));
	return(r);
  }

  /* Step through the directory one block at a time. */
/*
  for (; pos < dirp->i_size; pos += block_size) {
 */
  pos = 0;
  while (TRUE) {
	b = bmap(dirp, pos);	/* get next block number */
	if (b == NO_BLOCK) {	/* no more data... */
/* FIXME: record the EOF? (+ i_flags) */
		return(ENOENT);
	}
	bp = get_block(dev, b, NORMAL);	/* get the directory block */

	assert(bp != NULL);

	/* Search a directory block. */
	for (dp = &bp->b_dir[0];
			dp < &bp->b_dir[NR_DIR_ENTRIES(block_size)];
			dp++) {

		/* is the EndOfDirectory mark found? */
		if ( (slot_mark=dp->d_direntry.deName[0]) == SLOT_EMPTY) {
			put_block(bp);
			return(ENOENT);
		}
		/* is this slot out of use? */
		if (slot_mark == SLOT_DELETED) {
			continue;
		}

		/* is this entry for long names? */
		if (dp->d_lfnentry.lfnAttributes == ATTR_LFN) {
/* WORK NEEDED! */
			continue;
		}
		if(dp->d_direntry.deAttributes & ATTR_VOLUME) {
		/* skip any entry with volume attribute set */
			continue;
		}

		if (strncmp((char*)dp->d_direntry.deName,
			    (char*)direntry.deName, 8+3) == 0) {
			/* we have a match on short name! */
			r = OK;
			assert(res_inop);
#if 0
/* FIXME uses coordinates... */
/* FIXME FAT32 */
			ino = get_le16(dp->d_direntry.deStartCluster);
			if (ino && (*res_inop = cluster_to_inode(ino)) ) {
				/* found in inode cache */
				get_inode(*res_inop);
			} else if (ino==0
				&& (*res_inop = dirref_to_inode(INODE_NR(dirp),
				    pos + ((char*)dp - (char*)&bp->b_dir[0]) )) ) {
				/* found in inode cache (alternative way) */
				get_inode(*res_inop);
			} else {
/* WORK NEEDED! */
				*res_inop = enter_as_inode(&dp->d_direntry, dirp,
					pos + ((char*)dp - (char*)&bp->b_dir[0]) );
				if( *res_inop == NULL ) {
/* FIXME: do something clever... */
					panic("FATfs: lookup cannot create inode\n");
				}
			}
#else
			*res_inop = enter_as_inode(&dp->d_direntry, dirp,
					pos + ((char*)dp - (char*)&bp->b_dir[0]) );
			if( *res_inop == NULL ) {
/* FIXME: do something clever... */
				panic("FATfs: lookup cannot create inode\n");
			}
#endif
			/* inode have its reference count incremented. */
			put_block(bp);
			return(r);
		}
	}

	/* The whole block has been searched. */
	put_block(bp);
	pos += block_size;	/* continue searching dir */
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
/* This function searches the directory whose inode is pointed to by 'ldip':
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
  if ( (dirp->i_mode & I_TYPE) != I_DIRECTORY)  {
	return(ENOTDIR);
   }
  
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
/* This function searches the directory whose inode is pointed to by 'ldip',
 * and delete the entry from the directory.
 */
  register union direntry_u * dp;
  register struct buf *bp = NULL;
  struct fat_direntry direntry;
  int r;
  off_t pos;
  block_t b;

  /* If 'dirp' is not a pointer to a dir inode, error. */
  if ( (dirp->i_Attributes & ATTR_DIRECTORY) != ATTR_DIRECTORY)  {
	return(ENOTDIR);
  }
/* we should use the dir_ref to check we search the correct parent,
 * and that the entries (the searched and what is on disk) matche.
 */
}

/*===========================================================================*
 *				is_empty_dir				     *
 *===========================================================================*/
PUBLIC int is_empty_dir(
  register struct inode *dirp) /* ptr to inode for dir to search */
{
/* This function searches the directory whose inode is pointed to by 'ldip',
 * and return OK if only . and .. in dir, else ENOTEMPTY;
 */
  register union direntry_u * dp;
  register struct buf *bp = NULL;
  struct fat_direntry direntry;
  int r;
  off_t pos;
  block_t b;

  /* If 'dirp' is not a pointer to a dir inode, error. */
  if ( (dirp->i_Attributes & ATTR_DIRECTORY) != ATTR_DIRECTORY)  {
	return(ENOTDIR);
  }

  memset(&direntry, '\0', sizeof direntry);	/* Avoid leaking any data */
#if 0
  if ( (r = nameto83(string, &direntry)) != OK)
/* WORK NEEDED: LFN... */
	return(r);
#endif

  /* Step through the directory one block at a time. */
/*
  for (; pos < dirp->i_size; pos += block_size) {
 */
  pos = 0;
  while (TRUE) {
	b = bmap(dirp, pos);	/* get next block number */
	if (b == NO_BLOCK) {	/* no more data... */
/* FIXME: record the EOF? (+ i_flags) */
		return(OK);	/* it was empty! */
	}
	bp = get_block(dev, b, NORMAL);	/* get the directory block */

	assert(bp != NULL);

	/* Search a directory block. */
	for (dp = &bp->b_dir[0];
			dp < &bp->b_dir[NR_DIR_ENTRIES(block_size)];
			dp++) {

		/* is the EndOfDirectory mark found? */
		if (dp->d_direntry.deName[0] == SLOT_EMPTY) {
			put_block(bp);
			return(OK);	/* it was empty! */
		}
		/* is this slot out of use? */
		if (dp->d_direntry.deName[0] == SLOT_DELETED) {
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
		return(ENOTEMPTY);
	}

	/* The whole block has been searched. */
	put_block(bp);
	pos += block_size;	/* continue searching dir */
  }

/* Cannot happen!
 * Either we encounter the 0 (SLOT_EMPTY) marker,
 * or we exhausted the cluster chain.
 */
  panic("FATfs: broke out of is_empty_dir() loop\n");
}
