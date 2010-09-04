/* This file contains the procedures that look up names in the directory
 * system and determine the entry that goes with a given name.
 *
 *  The entry points into this file are
 *   do_getdents	perform the GETDENTS file system request
 *   search_dir: search a directory for a string and return its inode number
 *
 */
 
#include "inc.h"

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

#include "fat.h"
#include "super.h"
#include "inode.h"

/*===========================================================================*
 *				search_dir				     *
 *===========================================================================*/
PUBLIC int search_dir(
  register struct inode *ldir_ptr, /* ptr to inode for dir to search */
  char string[NAME_MAX],	/* component to search for */
  ino_t *numb,			/* pointer to inode number */
  enum search_dir_arg_e flag,	/* LOOK_UP, ENTER, DELETE or IS_EMPTY */
  int check_permissions)	/* check permissions when flag is !IS_EMPTY */
{
/* This function searches the directory whose inode is pointed to by 'ldip':
 * if (flag == ENTER)  enter 'string' in the directory with inode # '*numb';
 * if (flag == DELETE) delete 'string' from the directory;
 * if (flag == LOOK_UP) search for 'string' and return inode # in 'numb';
 * if (flag == IS_EMPTY) return OK if only . and .. in dir else ENOTEMPTY;
 *
 *    if 'string' is dot1 or dot2, no access permissions are checked.
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

  /* If 'ldir_ptr' is not a pointer to a dir inode, error. */
  if ( (ldir_ptr->i_mode & I_TYPE) != I_DIRECTORY)  {
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
#if 0
 bmap
	b = read_map(ldir_ptr, pos);	/* get block number */
#endif

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

#ifndef GETDENTS_BUFSIZ
#define GETDENTS_BUFSIZ  257
#endif
PRIVATE char getdents_buf[GETDENTS_BUFSIZ];


/*===========================================================================*
 *				do_getdents				     *
 *===========================================================================*/
PUBLIC int do_getdents(void)
{
  register struct inode *rip;
  int o, r, done;
  unsigned int block_size, len, reclen;
  ino_t ino;
  block_t b;
  cp_grant_id_t gid;
  size_t size, tmpbuf_off, userbuf_off;
  off_t pos, off, block_pos, new_pos, ent_pos;
  struct buf *bp;
  struct fat_direntry *dp;
  struct dirent *dep;
  char *cp;

  ino = (ino_t) m_in.REQ_INODE_NR;
  gid = (gid_t) m_in.REQ_GRANT;
  size = (size_t) m_in.REQ_MEM_SIZE;
  pos = (off_t) m_in.REQ_SEEK_POS_LO;

  /* Check whether the position is properly aligned */
  if( (unsigned int) pos % DIR_ENTRY_SIZE)
	  return(ENOENT);
  
  if( (rip = fetch_inode(ino)) == NULL) 
	  return(EINVAL);

  block_size = /*FIXME rip->i_sp->s_block_size */ 512;
  off = (pos % block_size);		/* Offset in block */
  block_pos = pos - off;
  done = FALSE;		/* Stop processing directory blocks when done is set */

  tmpbuf_off = 0;	/* Offset in getdents_buf */
  memset(getdents_buf, '\0', GETDENTS_BUFSIZ);	/* Avoid leaking any data */
  userbuf_off = 0;	/* Offset in the user's buffer */

  /* The default position for the next request is EOF. If the user's buffer
   * fills up before EOF, new_pos will be modified. */
  new_pos = rip->i_size;

  for(; block_pos < rip->i_size; block_pos += block_size) {
#if 0
	b = read_map(rip, block_pos);	/* get block number */
#endif
	  
	/* Since directories don't have holes, 'b' cannot be NO_BLOCK. */
	bp = get_block(dev, b, NORMAL);	/* get a dir block */

	assert(bp != NULL);

	  /* Search a directory block. */
	  if (block_pos < pos)
#if 0
		  dp = &bp->b_dir[off / DIR_ENTRY_SIZE];
#elif 0
		  dp = &bp->b_dir[off / DIR_ENTRY_SIZE].d_direntry;
#else
		  dp = (struct fat_direntry *) &bp->b_dir[off / DIR_ENTRY_SIZE].d_direntry;
#endif
	  else
		  dp = (struct fat_direntry *) &bp->b_dir[0];
#if 0
	  for (; dp < &bp->b_dir[NR_DIR_ENTRIES(block_size)]; dp++) {
#else
	  for (; dp < (struct fat_direntry *) &bp->b_dir[16]; dp++) {
#endif

#if 0
		  if (dp->d_ino == 0) 
			  continue;	/* Entry is not in use */

		  /* Compute the length of the name */
		  cp = memchr(dp->d_name, '\0', NAME_MAX);
		  if (cp == NULL)
			  len = NAME_MAX;
		  else
			  len = cp - (dp->d_name);
#endif
		
		  /* Compute record length */
		  reclen = offsetof(struct dirent, d_name) + len + 1;
		  o = (reclen % sizeof(long));
		  if (o != 0)
			  reclen += sizeof(long) - o;

		  /* Need the position of this entry in the directory */
/* FIXME: should rather be (char *)bp->b_dir, methinks */
		  ent_pos = block_pos + ((char *) dp - (bp->b_data));

		  if(tmpbuf_off + reclen > GETDENTS_BUFSIZ) {
			  r = sys_safecopyto(VFS_PROC_NR, gid,
			  		     (vir_bytes) userbuf_off, 
					     (vir_bytes) getdents_buf,
					     (size_t) tmpbuf_off, D);
			  if (r != OK) {
			  	put_inode(rip);
			  	return(r);
			  }

			  userbuf_off += tmpbuf_off;
			  tmpbuf_off = 0;
		  }
		  
		  if(userbuf_off + tmpbuf_off + reclen > size) {
			  /* The user has no space for one more record */
			  done = TRUE;
			  
			  /* Record the position of this entry, it is the
			   * starting point of the next request (unless the
			   * postion is modified with lseek).
			   */
			  new_pos = ent_pos;
			  break;
		  }

		  dep = (struct dirent *) &getdents_buf[tmpbuf_off];
#if 0
/* HACK HACK HACK
 * cannot give the "real" inode given when the file will be opened (lookup)
 * because we have no idea of the generation number...
 */
		  dep->d_ino = dp->d_ino;
#endif
		  dep->d_off = ent_pos;
		  dep->d_reclen = (unsigned short) reclen;
#if 0
		  memcpy(dep->d_name, dp->d_name, len);
#endif
		  dep->d_name[len] = '\0';
		  tmpbuf_off += reclen;
	  }

	  put_block(bp /*, DIRECTORY_BLOCK*/ );
	  if(done)
		  break;
  }

  if(tmpbuf_off != 0) {
	  r = sys_safecopyto(VFS_PROC_NR, gid, (vir_bytes) userbuf_off,
	  		     (vir_bytes) getdents_buf, (size_t) tmpbuf_off, D);
	  if (r != OK) {
	  	put_inode(rip);
	  	return(r);
	  }

	  userbuf_off += tmpbuf_off;
  }

  if(done && userbuf_off == 0)
	  r = EINVAL;		/* The user's buffer is too small */
  else {
	  m_out.RES_NBYTES = userbuf_off;
	  m_out.RES_SEEK_POS_LO = new_pos;
/*
	  rip->i_update |= ATIME;
	  rip->i_dirt = DIRTY;
 */
	  r = OK;
  }

  put_inode(rip);		/* release the inode */
  return(r);
}
