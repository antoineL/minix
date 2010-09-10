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

#if 0
#include "fat.h"
#include "super.h"
#include "inode.h"
#endif

FORWARD _PROTOTYPE( int nameto83,
		 (char string[NAME_MAX+1], struct fat_direntry *)	);

/* warning: the following lines are not failsafe macros */
#define	get_le16(arr) ((u16_t)( (arr)[0] | ((arr)[1]<<8) ))
#define	get_le32(arr) ( get_le16(arr) | ((u32_t)get_le16((arr)+2)<<16) )

#ifndef GETDENTS_BUFSIZ
#define GETDENTS_BUFSIZ  (259+sizeof(struct dirent))
#endif
PRIVATE char getdents_buf[GETDENTS_BUFSIZ];

/*===========================================================================*
 *				do_getdents				   *
 *===========================================================================*/
PUBLIC int do_getdents(void)
{
/* ...
 */
  int o, r, done;
  struct inode *rip, *newip;
  struct buf *bp;
  union direntry_u * dp;
  struct fat_direntry *fatdp;
  int len, namelen, extlen, reclen;
  block_t b;
  cp_grant_id_t gid;
  size_t size, mybuf_off, callerbuf_off;
  off_t pos, off, block_pos, new_pos, ent_pos;
  struct dirent *dep;
  char *cp;

  gid = (gid_t) m_in.REQ_GRANT;
  size = (size_t) m_in.REQ_MEM_SIZE;
  pos = (off_t) m_in.REQ_SEEK_POS_LO;

  DBGprintf(("FATfs: getdents in %lo, off %ld\n", m_in.REQ_INODE_NR, pos));

  /* Check whether the position is properly aligned */
  if( (unsigned int) pos % DIR_ENTRY_SIZE)
	return(ENOENT);

  if( (rip = fetch_inode((ino_t) m_in.REQ_INODE_NR)) == NULL) 
	return(EINVAL);
  off = pos & (block_size-1);	/* Offset in block */
  block_pos = pos - off;
  done = FALSE;		/* Stop processing blocks when done is set */

  mybuf_off = 0;	/* Offset in getdents_buf */
  memset(getdents_buf, '\0', GETDENTS_BUFSIZ);	/* Avoid leaking any data */
  callerbuf_off = 0;	/* Offset in the user's buffer */

/* CHECKME: with FAT, root dir does not have . and ..  Do we need to fake them? */

/* WORK NEEDED: with FAT, dir size are unknown */
  /* The default position for the next request is EOF. If the user's buffer
   * fills up before EOF, new_pos will be modified. */
  new_pos = sb.maxFilesize;

/*
  for(; block_pos < rip->i_size; block_pos += block_size) {
 */
  while (TRUE) {
	b = bmap(rip, block_pos);	/* get block number */
	if (b == NO_BLOCK) {	/* no more data... */
/* FIXME: record the EOF? (+ i_flags) */
/* FIXME: update new_pos ? */
		done = TRUE;
		break;
	}
	bp = get_block(dev, b, NORMAL);	/* get a dir block */

	assert(bp != NULL);

	/* Search a directory block. */
	if (block_pos < pos)
#if 1
		dp = &bp->b_dir[off / DIR_ENTRY_SIZE];
#elif 0
		fatdp = &bp->b_dir[off / DIR_ENTRY_SIZE].d_direntry;
#else
		fatdp = (struct fat_direntry *) &bp->b_dir[off / DIR_ENTRY_SIZE].d_direntry;
#endif
	else
		dp = &bp->b_dir[0];

#if 1
	for (; dp < &bp->b_dir[block_size / DIR_ENTRY_SIZE]; dp++) {
#elif 0
	for (; fatdp < &bp->b_dir[NR_DIR_ENTRIES(block_size)]; fatdp++) {
#else
	for (; fatdp < (struct fat_direntry *) &bp->b_dir[16]; fatdp++) {
#endif

DBGprintf(("FATfs: seen ['%.8s.%.3s'], #0=\\%.3o\n",
	dp->d_direntry.deName, dp->d_direntry.deExtension, dp->d_direntry.deName[0]));

		/* is the EndOfDirectory mark found? */
		if (dp->d_direntry.deName[0] == SLOT_EMPTY) {
			done = TRUE;
/* FIXME: record the EOF? (+ i_flags) */
/* FIXME: update new_pos ? */
			break;
		}
		/* is this slot out of use? */
		if (dp->d_direntry.deName[0] == SLOT_DELETED) {
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
		fatdp = & dp->d_direntry;
		{
			cp = (char*) &fatdp->deName[8];
DBGprintf(("cp[-8] = '%.8s' ", cp-8));
			while (--cp > (char*)&fatdp->deName[1])
				if (*cp != ' ') break;
			namelen = cp - (char*)fatdp->deName + 1;
			assert(namelen > 0);
			assert(namelen <= 8);
	
			cp = (char*) &fatdp->deExtension[3];
DBGprintf(("cp[-3] = '%.3s' ", cp-3));
			while (--cp > (char*)&fatdp->deExtension[0])
				if (*cp != ' ') break;
			extlen = cp - (char*)fatdp->deExtension + 1;
			assert(extlen >= 0);
			assert(extlen <= 3);
			len = namelen + (extlen ? 1 + extlen : 0);
DBGprintf(("calc.len = %d\n", len));
/* FIXME: lcase */
		}
		assert(len > 0);
		
		/* Compute record length */
		reclen = offsetof(struct dirent, d_name) + len + 1;
		o = (reclen % sizeof(long));
		if (o != 0)
			reclen += sizeof(long) - o;
			/* the buffer has been cleared before */

		if (mybuf_off + reclen > GETDENTS_BUFSIZ) {
DBGprintf(("flush: off=%d, reclen=%d, max=%u\n", mybuf_off, reclen, GETDENTS_BUFSIZ));
			/* flush my buffer */
			r = sys_safecopyto(VFS_PROC_NR, gid,
					   (vir_bytes) callerbuf_off, 
					   (vir_bytes) getdents_buf,
					   (size_t) mybuf_off, D);
			if (r != OK) {
				put_inode(rip);
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
			done = TRUE;
			
			/* Record the position of this entry, it is the
			 * starting point of the next request (unless the
			 * postion is modified with lseek).
			 */
			new_pos = ent_pos;
			break;
		}

		dep = (struct dirent *) &getdents_buf[mybuf_off];
/* HACK HACK HACK
 * cannot give the "real" inode given when the file will be opened (lookup)
 * because we have no idea of the generation number...
 * So we enter it anyway in the inode array, hoping it will survive...
 */
		newip = enter_inode(fatdp);
		if( newip == NULL ) {
/* FIXME: do something clever... */
			panic("FATfs: getdents cannot create some virtual inode\n");
		} else
			dep->d_ino = INODE_NR(newip);
		/* put_inode(newip); */
		dep->d_off = ent_pos;
		dep->d_reclen = (unsigned short) reclen;
		{
			cp = &dep->d_name[0];
/* FIXME: lcase */
DBGprintf(("FATfs: copying '%.*' to %.8p (buf+%d)", namelen, fatdp->deName, cp, cp-getdents_buf));

			memcpy(cp, fatdp->deName, namelen);
			if (fatdp->deName[0] == SLOT_E5)
				*cp = SLOT_DELETED;	/*DOS was hacked too*/
			cp += namelen;
			if (extlen) {
				*cp++ = '.';
/* FIXME: lcase */
DBGprintf((", '%.*' to %.8p (buf+%d)", extlen, fatdp->deExtension, cp, cp-getdents_buf));
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
	r = sys_safecopyto(VFS_PROC_NR, gid, (vir_bytes) callerbuf_off,
			   (vir_bytes) getdents_buf, (size_t) mybuf_off, D);
	if (r != OK) {
		put_inode(rip);
		return(r);
	}

	callerbuf_off += mybuf_off;
  }

  if(done && callerbuf_off == 0)
	r = EINVAL;		/* The user's buffer is too small */
  else {
	m_out.RES_NBYTES = callerbuf_off;
	m_out.RES_SEEK_POS_LO = new_pos;
	rip->i_flags |= I_DIRTY | I_ACCESS;
	r = OK;
  }

  put_inode(rip);		/* release the inode */
  return(r);
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
 * used in FAT directory entries, named 8.3 because it could only count
 * 8 characters before the only dot, and 3 characters after it.
 * This functions returns EINVAL if invalid characters were encountered,
 * and ENAMETOOLONG if it does not fit the 8.3 limits.
 */
  const unsigned char *p = (const unsigned char *)string;
  unsigned char *q;
  int prev, next, havelower, haveupper;

  memset(fatdp->deName, ' ', 8+3);	/* fill with spaces */
  fatdp->deLCase &= ~ (LCASE_NAME|LCASE_EXTENSION);

  switch (next = *p) {
  case '.':
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

  havelower = haveupper = prev = 0;
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
		haveupper |= isupper(next);
		havelower |= islower(next);
		*q = toupper(next);
	}
	prev = next;
	next = *p++;
  }
/* check q-d==3 &&
   static const char *dev3[] = {"CON", "AUX", "PRN", "NUL", "   "};
 * check q-d==4 && isdigit(q[-1])
   static const char *dev4[] = {"COM", "LPT" };
 */
  if (havelower && !haveupper)
	fatdp->deLCase |= LCASE_NAME;

  if (*p == '\0') {
	/* no extension */
	return(OK);
  }
  if (*p++ != '.') {
	/* more than 8 chars in name... */
	return(ENAMETOOLONG);
  }
  if (*p == '\0') {
	/* initial filename was "foobar." (trailing dot)
	 * DOS and Windows recognize it the same as "foobar"
	 */
	DBGprintf(("FATfs: filename \"%s\" has a trailing dot...\n",
			string));
	return(OK);	/* (EINVAL); */
  }

  havelower = haveupper = prev = 0;
  for (q=&fatdp->deExtension[0]; q < &fatdp->deExtension[3]; ++q) {
	if (next == '\0') {
		if (prev == ' ') {
			DBGprintf(("FATfs: warning: final space character "
				"in an extension, will be dropped.\n"));
		}
		if (havelower && !haveupper)
			fatdp->deLCase |= LCASE_EXTENSION;
		return(OK);
	} else if (strchr(FAT_FORBIDDEN_CHARS, next) != NULL) {
		/* some characters cannot enter in FAT filenames */
		return(EINVAL);
	} else {
		haveupper |= isupper(next);
		havelower |= islower(next);
		*q = toupper(next);
	}
	prev = next;
	next = *p++;
  }
  /* more than 3 chars in extension... */
  return(ENAMETOOLONG);
}

/*===========================================================================*
 *				lookup_dir				     *
 *===========================================================================*/
PUBLIC int lookup_dir(
  register struct inode *dir_ptr, /* ptr to inode for dir to search */
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

  /* If 'dir_ptr' is not a pointer to a dir inode, error. */
  if ( (dir_ptr->i_Attributes & ATTR_DIRECTORY) != ATTR_DIRECTORY)  {
	return(ENOTDIR);
  }

/* FIXME: we currently assume FAT directories are always searchable...
 * A well-behaved way would be to honour use_dir_mask vs. credentials,
 * even more to honour the SYSTEM attribute and use_system_uid etc.
 *
 *   Note: if 'string' is dot1 or dot2, no access permissions are checked.
 */
  
  memset(&direntry, '\0', sizeof direntry);	/* Avoid leaking any data */
  if ( (r = nameto83(string, &direntry)) != OK)
/* WORK NEEDED: LFN... */
	return(r);

  /* Step through the directory one block at a time. */
/*
  for (; pos < dir_ptr->i_size; pos += block_size) {
 */
  pos = 0;
  while (TRUE) {
	b = bmap(dir_ptr, pos);	/* get block number */
	if (b == NO_BLOCK) {	/* no more data... */
/* FIXME: record the EOF? (+ i_flags) */
		return(ENOENT);
	}
	bp = get_block(dev, b, NORMAL);	/* get a dir block */

	assert(bp != NULL);

	/* Search a directory block. */
	for (dp = &bp->b_dir[0];
			dp < &bp->b_dir[NR_DIR_ENTRIES(block_size)];
			dp++) {

		/* is the EndOfDirectory mark found? */
		if (dp->d_direntry.deName[0] == SLOT_EMPTY) {
			put_block(bp);
			return(ENOENT);
		}
		/* is this slot out of use? */
		if (dp->d_direntry.deName[0] == SLOT_DELETED) {
			continue;
		}

		/* is this entry for long names? */
		if (dp->d_lfnentry.lfnAttributes == ATTR_LFN) {
/* WORK NEEDED! */
			continue;
		}

		if (strncmp((char*)dp->d_direntry.deName,
			    (char*)direntry.deName, 8+3) == 0) {
			/* we have a match on short name! */
			r = OK;
			assert(res_inop);
			*res_inop = enter_inode(&dp->d_direntry);
			if( *res_inop == NULL ) {
/* FIXME: do something clever... */
				panic("FATfs: lookup cannot create inode\n");
			}
			/* get_inode(*res_inop); */
			put_block(bp);
			return(r);
		}
	}

	/* The whole block has been searched. */
	put_block(bp);
	pos += block_size;	/* continue searching dir */
  }

/* Cannot happen!
 * Either we encounter the 0 (SLOT_EMPTY) marker,
 * or we exhausted the cluster chain.
 */
  panic("FATfs: broke out of dir.lookup loop\n");
}

/*===========================================================================*
 *				add_direntry				     *
 *===========================================================================*/
PUBLIC int add_direntry(
  register struct inode *dir_ptr, /* ptr to inode for dir to search */
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
  /* If 'dir_ptr' is not a pointer to a dir inode, error. */
  if ( (dir_ptr->i_mode & I_TYPE) != I_DIRECTORY)  {
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
		r = forbidden(ldir_ptr, bits); /* check access permissions */
#endif
	}
  }
  if (r != OK) return(r);
  
  /* Step through the directory one block at a time. */
  old_slots = (unsigned) (ldir_ptr->i_size/ DIR_ENTRY_SIZE );
  new_slots = 0;
  e_hit = FALSE;
  match = 0;			/* set when a string match occurs */

  for (pos = 0; pos < ldir_ptr->i_size; pos += /*FIXME ldir_ptr->i_sp->s_block_size*/ 512 ) {
	b = bmap(ldir_ptr, pos);	/* get block number */

	/* Since directories don't have holes, 'b' cannot be NO_BLOCK. */
	bp = get_block(dev, b, NORMAL);	/* get a dir block */

	assert(bp != NULL);

#if 0
	/* Search a directory block. */
	for (dp = &bp->b_dir[0];
		dp < &bp->b_dir[NR_DIR_ENTRIES(ldir_ptr->i_sp->s_block_size)];
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
				ldir_ptr->i_update |= CTIME | MTIME;
				ldir_ptr->i_dirt = DIRTY;
			} else {
				sp = ldir_ptr->i_sp;	/* 'flag' is LOOK_UP */
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
	if ( (bp = new_block(ldir_ptr, ldir_ptr->i_size)) == NULL)
		return(err_code);
	dp = &bp->b_dir[0];
#endif
	extended = 1;
  }

#if 0
  /* 'bp' now points to a directory block with space. 'dp' points to slot. */
  (void) memset(dp->d_name, 0, (size_t) NAME_MAX); /* clear entry */
  for (i = 0; i < NAME_MAX && string[i]; i++) dp->d_name[i] = string[i];
  sp = ldir_ptr->i_sp; 
  dp->d_ino = conv4(sp->s_native, (int) *numb);
  bp->b_dirt = DIRTY;
  put_block(bp, DIRECTORY_BLOCK);
  ldir_ptr->i_update |= CTIME | MTIME;	/* mark mtime for update later */
  ldir_ptr->i_dirt = DIRTY;
  if (new_slots > old_slots) {
	ldir_ptr->i_size = (off_t) new_slots * DIR_ENTRY_SIZE;
	/* Send the change to disk if the directory is extended. */
	if (extended) rw_inode(ldir_ptr, WRITING);
  }
#endif
  return(OK);
}

/*===========================================================================*
 *				del_direntry				     *
 *===========================================================================*/
PUBLIC int del_direntry(
  register struct inode *dir_ptr, /* ptr to inode for dir to search */
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

  /* If 'dir_ptr' is not a pointer to a dir inode, error. */
  if ( (dir_ptr->i_Attributes & ATTR_DIRECTORY) != ATTR_DIRECTORY)  {
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
  register struct inode *dir_ptr) /* ptr to inode for dir to search */
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

  /* If 'dir_ptr' is not a pointer to a dir inode, error. */
  if ( (dir_ptr->i_Attributes & ATTR_DIRECTORY) != ATTR_DIRECTORY)  {
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
  for (; pos < dir_ptr->i_size; pos += block_size) {
 */
  pos = 0;
  while (TRUE) {
	b = bmap(dir_ptr, pos);	/* get block number */
	if (b == NO_BLOCK) {	/* no more data... */
/* FIXME: record the EOF? (+ i_flags) */
		return(OK);	/* it was empty! */
	}
	bp = get_block(dev, b, NORMAL);	/* get a dir block */

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
		/* is this entry for long names? */
		if (dp->d_lfnentry.lfnAttributes == ATTR_LFN) {
/* WORK NEEDED! */
			continue;
		}

		/* we have a valid short name! */
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
