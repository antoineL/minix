/* This file provides path-to-inode lookup functionality.
 *
 * The entry points into this file are:
 *   do_putnode		perform the PUTNODE file system request
 *   do_mountpoint	perform the MOUNTPOINT file system request
 *   do_lookup		perform the LOOKUP file system request
 *   do_create		perform the CREATE, MKDIR, MKNOD file system requests
 *   do_newnode		perform the NEWNODE file system request
 *   do_unlink		perform the UNLINK, RMDIR file system requests
 *
 * Warning: this code is not reentrant (use static local variables, without mutex)
 *
 * Auteur: Antoine Leca, aout 2010.
 * Updated:
 */

#include "inc.h"

#include <string.h>

#include <sys/stat.h>

#ifdef   COMPAT316
#include "compat.h"	/* MUST come before <minix/sysutil.h> */
#endif
#include <minix/safecopies.h>
#include <minix/syslib.h>	/* sys_safecopies{from,to} */
#include <minix/sysutil.h>	/* panic */

/* Private global variables: */
PRIVATE char user_path[PATH_MAX+1];  /* pathname to be processed */

/* Private functions:
 *   get_mask		?
 */
/* useful only with symlinks... dropped ATM
FORWARD _PROTOTYPE( int ltraverse, (struct inode *rip, char *suffix)	);
 * useful only with permissions... dropped ATM
FORWARD _PROTOTYPE( int get_mask, (vfs_ucred_t *)			);
 */

FORWARD _PROTOTYPE( int parse_path, (ino_t dir, ino_t root,
	struct inode **res_inop, size_t *offsetp)			);

FORWARD _PROTOTYPE( char *get_name, (char *name, char string[NAME_MAX+1]) );
FORWARD _PROTOTYPE( int remove_dir,
	(struct inode *dirp, struct inode *rip, char string[NAME_MAX+1]) );
FORWARD _PROTOTYPE( int unlink_file,
	(struct inode *dirp, struct inode *rip, char string[NAME_MAX+1]) );

/*===========================================================================*
 *				do_putnode				 *
 *===========================================================================*/
PUBLIC int do_putnode(void)
{
/* Decrease an inode's reference count.
 */
  struct inode *ino;
  int count;

  if ((ino = fetch_inode(m_in.REQ_INODE_NR)) == NULL)
	return EINVAL;

  count = m_in.REQ_COUNT;

  if (count <= 0 || count > ino->i_ref) return EINVAL;

  /* Decrease reference counter, but keep one reference; it will be consumed by
   * put_inode(). */ 
  ino->i_ref -= count - 1;

  put_inode(ino);

  return OK;
}

/*===========================================================================*
 *				do_mountpoint				     *
 *===========================================================================*/
PUBLIC int do_mountpoint(void)
{
/* Register a mount point. Nothing really extraordinary for FAT. */
  struct inode *rip;
  int r = OK;
  
  /* Get the inode from the request msg. Do not increase the inode refcount*/
  /*CHECKME: is it needed? is the file open anyway? should we increase ref? */
  if( (rip = fetch_inode((ino_t) m_in.REQ_INODE_NR)) == NULL)
	  return(EINVAL);
  
  if (rip->i_flags & I_MOUNTPOINT) r = EBUSY;
  if (rip->i_ref != 1) {
	DBGprintf(("FATfs: VFS passed otherwise used node as mountpoint!\n"));
	r = EBUSY;
  }

  /* Only allows mouting on directories; not strictly necessary. */
  if ( ! (rip->i_flags & I_DIR) ) r = ENOTDIR;

  if(r == OK) rip->i_flags |= I_MOUNTPOINT;

  /* The flag will be cleared when VFS will issue the PUTNODE request
   * (at umount() time), thus freeing our inode.
   */

  /* put_inode(rip);	we did not increase the refcount, no need to release*/

  return(r);
}

/*===========================================================================*
 *                             do_lookup				 *
 *===========================================================================*/
PUBLIC int do_lookup(void)
{
/* Perform the LOOKUP file system request. Crack the parameters and defer
 * to parse_path which will crunch the path passed.
 */
  cp_grant_id_t gid, gid2;
  int r, flags;
  int path_len;
  size_t offset = 0, path_size, cred_size;
/* FIXME:
 * should be moved to globals? to inode?
 * its use is to be around when used by other ops which might not include
 * all the details... but how can we do the mapping and avoid race conditions
 * particularly if/when VFS became multi-tasked...
 */
  vfs_ucred_t credentials;
  ino_t dir_ino, root_ino;
  struct inode *rip;

  gid		= (cp_grant_id_t) m_in.REQ_GRANT;
  path_size	= (size_t) m_in.REQ_PATH_SIZE; /* Size of the buffer */
  path_len	= (int) m_in.REQ_PATH_LEN; /* including terminating nul */
  dir_ino	= (ino_t) m_in.REQ_DIR_INO;
  root_ino	= (ino_t) m_in.REQ_ROOT_INO;

  /* Check length. */
  if(path_len > sizeof(user_path)) return(E2BIG);	/* too big for buffer */
  if(path_len <= 0) return(EINVAL);			/* too small */

  /* Copy the pathname and set up caller's user and group id */
  r = sys_safecopyfrom(m_in.m_source, gid, (vir_bytes) 0, 
            (vir_bytes) user_path, (size_t) path_len, D);
  if(r != OK) return(r);

  /* Verify this is a null-terminated path. */
  if(user_path[path_len - 1] != '\0') return(EINVAL);

  /* FIXME: drop all that credentials stuff? */
  if(m_in.REQ_FLAGS & PATH_GET_UCRED) {
	/* We have to copy uid/gid credentials */
  	gid2 = (cp_grant_id_t) m_in.REQ_GRANT2;
  	cred_size = (size_t) m_in.REQ_UCRED_SIZE;

  	if (cred_size > sizeof(credentials)) return(EINVAL); /* Too big. */
  	r = sys_safecopyfrom(m_in.m_source, gid2, (vir_bytes) 0,
  			 (vir_bytes) &credentials, cred_size, D);
  	if (r != OK) return(r);
  } else {
  	memset(&credentials, 0, sizeof(credentials));
	credentials.vu_uid = m_in.REQ_UID;
	credentials.vu_gid = m_in.REQ_GID;
	credentials.vu_ngroups = 0;
  }

  DBGprintf(("FATfs: enter lookup dir=%lo, root=%lo, <%.*s>...\n",
	dir_ino, root_ino, path_len, user_path));

  /* Lookup inode */
  rip = NULL;
  r = parse_path(dir_ino, root_ino, &rip, &offset);

  assert(r != ESYMLINK);
  if(r == ELEAVEMOUNT) {
	/* Report offset and the error */
	m_out.RES_OFFSET = offset;
	m_out.RES_SYMLOOP = 0;	/* no symlink in FAT */
	return(r);
  }

  if (r != OK && r != EENTERMOUNT) return(r);

  m_out.RES_INODE_NR = INODE_NR(rip);
  m_out.RES_MODE = rip->i_mode;
  m_out.RES_FILE_SIZE_LO = rip->i_size;
  m_out.RES_FILE_SIZE_HI = 0;
  m_out.RES_UID = use_uid;
  m_out.RES_GID = use_gid;
  m_out.RES_SYMLOOP = 0;
  
  /* This is only valid for block and character specials.
   * But it does not cause any harm to set RES_DEV always.
   */
  m_out.RES_DEV = dev;

  if(r == EENTERMOUNT) {
	m_out.RES_OFFSET = offset;
	put_inode(rip); /* Only return a reference to the final object */
  }

  DBGprintf(("lookup returns ino=%lo size:%ld mode:%.4o\n",
	m_out.RES_INODE_NR, m_out.RES_FILE_SIZE_LO, m_out.RES_MODE));

  return(r);
}

/*===========================================================================*
 *                             parse_path				   *
 *===========================================================================*/
PRIVATE int parse_path(
  ino_t dir_ino,
  ino_t root_ino,
  struct inode **res_inop,
  size_t *offsetp)
{
/* Parse the path in user_path, starting at dir_ino. If the path is the empty
 * string, just return dir_ino. It is upto the caller to treat an empty
 * path in a special way. Otherwise, if the path consists of just one or
 * more slash ('/') characters, the path is replaced with ".". Otherwise,
 * just look up the first (or only) component in path after skipping any
 * leading slashes. 
 */
  int r, leaving_mount;
  struct inode *dirp, *rip;
  char *cp, *next_cp; /* component and next component */
  char component[NAME_MAX+1];

  /* Start parsing path at the first component in user_path */
  cp = user_path;  

  /* Find starting inode inode according to the request message */
  if((rip = fetch_inode(dir_ino)) == NULL) {
	DBGprintf(("FATfs: parse_path cannot locate dir inode %lo...\n", dir_ino));
	return(ENOENT);
  }

  /* If dir has been removed return ENOENT. */
  if (rip->i_flags & I_ORPHAN) return(ENOENT);
 
  /* Increase the inode refcount. */
  get_inode(rip);

  /* If the given start inode is a mountpoint, we must be here because the file
   * system mounted on top returned an ELEAVEMOUNT error. In this case, we must
   * only accept ".." as the first path component.
   */
  leaving_mount = rip->i_flags & I_MOUNTPOINT; /* True iff rip is mountpoint*/

  r = OK;		/* assume success without comments */

  /* Scan the path component by component. */
  while (cp[0] != '\0') {
	next_cp = get_name(cp, component);

#if 0
	/* Special code for '..'. A process is not allowed to leave a chrooted
	 * environment. A lookup of '..' at the root of a mounted filesystem
	 * has to return ELEAVEMOUNT. In both cases, the caller needs search
	 * permission for the current inode, as it is used as directory.
	 */
	if(strcmp(component, "..") == 0) {
		/* 'rip' is now accessed as directory */
/*
		if ((r = forbidden(rip, X_BIT)) != OK) {
			put_inode(rip);
			return(r);
		}
 */
/* CHECK INDEX? *? */
		if (INODE_NR(rip) == root_ino) {
			cp = next_cp;
			continue;	/* Ignore the '..' at a process' root 
					 and move on to the next component */
		}

		if (IS_ROOT(rip)) {
/* NOTE: in FAT filesystems the root directory does NOT have the . and .. entries */
			/* comment: we cannot be the root FS! */
			/* Climbing up to parent FS */

			put_inode(rip);
			*offsetp = cp - user_path; 
			return(ELEAVEMOUNT);
		}
	}
#else
	/* Special case: a process is not allowed to leave a chrooted environment.
	 * A lookup of '..' when pointing at the user's root directory should
	 * not be allowed to go upper.
	 */
	if ( (INODE_NR(rip) == root_ino) && (strcmp(component, "..") == 0) ) {
	/* Ignore the '..' at a process' root and move on to the next comp. */
		cp = next_cp;
		continue;	
				
	}
#endif


	/* Only check for a mount point if we are not coming from one. */
	if (!leaving_mount && rip->i_flags & I_MOUNTPOINT) {
		/* Going to enter a child FS */
#if 0
		*res_inop = rip;
		*offsetp = cp - user_path;
		return(EENTERMOUNT);
#else
		break;
#endif
	}

	/* There is more path.  Keep parsing. */
	dirp = rip;
#if 0
	r = advance(dirp, &rip, leaving_mount ? dot2 : component, CHK_PERM);
#elif 0
	r = advance(dirp, &rip, leaving_mount ? ".." : component, CHK_PERM);
/* Given a directory and a component of a path, look up the component in
 * the directory, find the inode, open it, and return a pointer to its inode
 * slot.
 */
#else
	assert(component[0] != '\0');
	r = lookup_dir(dirp, component, &rip);
#endif

	if (r != OK) break;
	leaving_mount = 0;

	/* The call to lookup_dir() succeeded.  Fetch next component. */
	put_inode(dirp);	/* release the current inode */
	cp = next_cp;
  }

  /* We are done; either the path was empty, or we have parsed all 
   * components of the path, or we are changing to another FS.
   */

  if (r == OK || r == ELEAVEMOUNT || r == EENTERMOUNT) {
	assert(res_inop);
	*res_inop = rip;
	*offsetp = cp - user_path;
DBGprintf(("FATfs: parse path returned inode %lo\n", INODE_NR(rip)));

	/* Return EENTERMOUNT if we are at a mount point */
	if (rip->i_flags & I_MOUNTPOINT) r = EENTERMOUNT;
  }	
  return(r);
}

/*===========================================================================*
 *				advance					   *
 *===========================================================================*/
PUBLIC int advance(
  struct inode *dirp,		/* inode for directory to be searched */
  struct inode **res_inop,
  char string[NAME_MAX],	/* component name to look for */
  int chk_perm)			/* check permissions when string is looked up*/
{
/* Given a directory and a component of a path, look up the component in
 * the directory, find the inode, open it, and return a pointer to its inode
 * slot.
 */
  int r;
  ino_t numb;
  struct inode *rip;

  DBGprintf(("FATfs: advance in dir=%lo, looking for <%s>...\n",
	INODE_NR(dirp), string));

  /* If 'string' is empty, return an error. */
  if (string[0] == '\0') {
	return(ENOENT);
  }

#if 0
FIXME: changed interface...
  /* Check for NULL. */
  if (dirp == NULL) return(*res_inop = NULL);
#else
  assert(dirp != NULL);
#endif

  /* If 'string' is not present in the directory, signal error. */
  if ( (r = lookup_dir(dirp, string, &rip)) != OK) {
	return(r);
  }

  /* The component has been found in the directory.  Get inode. */
  /* get_inode(rip); */	/* already done before. */

  r = OK;		/* assume success without comments */

  /* The following test is for "mountpoint/.." where mountpoint is a
   * mountpoint. ".." will refer to the root of the mounted filesystem,
   * but has to become a reference to the parent of the 'mountpoint'
   * directory.
   *
   * This case is recognized by the looked up name pointing to a
   * root inode, and the directory in which it is held being a
   * root inode, _and_ the name[1] being '.'. (This is a test for '..'
   * and excludes '.'.)
   */
  if (rip->i_index == /*ROOT_INODE*/0) {
	if (dirp->i_index == /*ROOT_INODE*/0) {
		if (string[1] == '.') {
			/* comment: we cannot be the root FS! */
			/* Climbing up mountpoint */
			r = ELEAVEMOUNT;
		}
	}
  }

/* REVISE COMMENT! */
  /* See if the inode is mounted on.  If so, switch to root directory of the
   * mounted file system.  The super_block provides the linkage between the
   * inode mounted on and the root directory of the mounted file system.
   */
  if (rip->i_flags & I_MOUNTPOINT) {
	/* Mountpoint encountered, report it */
	r = EENTERMOUNT;
  }
  *res_inop = rip;
  return(r);
}

/*===========================================================================*
 *				get_name				   *
 *===========================================================================*/
PRIVATE char *get_name(
  char *path_name,		/* path name to parse */
  char string[NAME_MAX+1])	/* component extracted from 'old_name' */
{
/* Given a pointer to a path name, 'path_name', copy the first component
 * to 'string' (truncated if necessary, always nul terminated).
 * A pointer to the string after the first component of the name as yet
 * unparsed is returned.  Roughly speaking,
 * 'get_name' = 'path_name' - 'string'.
 *
 * This routine follows the standard convention that /usr/ast, /usr//ast,
 * //usr///ast and /usr/ast/ are all equivalent.
 */
  size_t len;
  char *cp, *ep;

  cp = path_name;

  /* Skip leading slashes */
  while (cp[0] == '/') cp++;

  /* Find the end of the first component */
#if 0
  ep = cp;
  while(ep[0] != '\0' && ep[0] != '/')
	ep++;
#else
  ep = strchr(cp, '/');
  if (ep == NULL) {
	ep = cp + strlen(cp);
	assert(*ep == '\0');
  }
#endif

  len = (size_t) (ep - cp);

/* FIXME: no: we should return ENAMETOOLONG instead! */
  /* Truncate the amount to be copied if it exceeds NAME_MAX */
  if (len > NAME_MAX) len = NAME_MAX;

  /* Special case of the string at cp is empty */
  if (len == 0) 
	strcpy(string, ".");  /* Return "." */
  else {
	memcpy(string, cp, len);
	string[len]= '\0';
  }

  return(ep);
}

/*===========================================================================*
 *				do_create				     *
 *===========================================================================*/
PUBLIC int do_create(void)
{
/* ... */
/* CREATE, MKNOD, MKDIR; NEW_NODE?; idem SYMLINK */
  int r;
  struct inode *dirp;
  struct inode *rip;
  char string[LFN_NAME_MAX + 1];
  mode_t newnode_mode;
  size_t len;

  if (read_only) return(EROFS);	/* paranoia */

  /* Read request message */
  newnode_mode = (mode_t) m_in.REQ_MODE;

  /* Copy the last component (i.e., file name) */
  len = m_in.REQ_PATH_LEN;
  if (len > sizeof(string) || len > NAME_MAX+1) {
	return(ENAMETOOLONG);
  }
  memset(string, 0, sizeof(string));
  r = sys_safecopyfrom(VFS_PROC_NR, (cp_grant_id_t) m_in.REQ_GRANT,
		    (vir_bytes) 0, (vir_bytes) string, len, D);
  if (r != OK) return r;
  /* Check protocol sanity */
  if( len<=1 || string[len-1]!='\0' || strlen(string)+1!=len )
	return(EINVAL);

  /* Get last directory inode (i.e., directory that will hold the new inode) */
  /* Get the inode from the request msg. Do not increase the inode refcount*/
  if( (dirp = fetch_inode((ino_t) m_in.REQ_INODE_NR)) == NULL)
	  return(EINVAL);
  /*CHECKME: is it needed? is the dir open anyway? should we increase ref? */
  /* get_inode(dirp); */

  assert(string[0] != '\0');
  r = lookup_dir(dirp, string, &rip);

  if ( rip == NULL && r == ENOENT) {
	/* Things look good so far... */
	/* Last path component does not exist.  Make new directory entry. */
  /* Are we creating a directory, a regular file,
   * or something else (which will fail)?
   */
  if (S_ISDIR(newnode_mode)) {
	/* Directories need allocation of space now (even if using MKNOD)
	 * for the server to work, since we rely on the fact
	 * directories have a non-zero start cluster number.
	 */
	/* make_fat_dir(dirp, string, &cn); */
#if 0  
MKDIR
  /* Get the inode numbers for . and .. to enter in the directory. */
  dotdot = ldirp->i_num;	/* parent's inode number */
  dot = rip->i_num;		/* inode number of the new dir itself */

  /* Now make dir entries for . and .. unless the disk is completely full. */
  /* Use dot1 and dot2, so the mode of the directory isn't important. */
  rip->i_mode = (mode_t) m_in.REQ_MODE;	/* set mode */
  r1 = search_dir(rip, dot1, &dot, ENTER, IGN_PERM);/* enter . in the new dir*/
  r2 = search_dir(rip, dot2, &dotdot, ENTER, IGN_PERM); /* enter .. in the new
							 dir */

  /* If both . and .. were successfully entered, increment the link counts. */
  if (r1 == OK && r2 == OK) {
	  /* Normal case.  It was possible to enter . and .. in the new dir. */
	  rip->i_nlinks++;	/* this accounts for . */
	  ldirp->i_nlinks++;	/* this accounts for .. */
	  ldirp->i_dirt = DIRTY;	/* mark parent's inode as dirty */
  } else {
	  /* It was not possible to enter . or .. probably disk was full -
	   * links counts haven't been touched. */
	  if(search_dir(ldirp, string, NULL, DELETE, IGN_PERM) != OK)
		  panic("Dir disappeared: %ul", rip->i_num);
	  rip->i_nlinks--;	/* undo the increment done in new_node() */
  }
  rip->i_dirt = DIRTY;		/* either way, i_nlinks has changed */
#endif
  } else if (S_ISREG(newnode_mode)) {
	/* cn = 0; */
  } else
	return(EINVAL);

  /* Try to make the file. */
  r = add_direntry(dirp, string, &rip /*, cn */ );

  /* FIXME: chmod READONLY */

  } else if (r == EENTERMOUNT || r == ELEAVEMOUNT) {
  	r = EEXIST;
  } else { 
	/* Either last component exists, or there is some problem. */
	if (rip != NULL)
		r = EEXIST;
  }

  /* If an error occurred, release inode. */
/* MKDIR also execs if rip==NULL */

  /* Reply message (solo CREATE) */
  if (m_in.m_type == REQ_CREATE) {
	m_out.RES_INODE_NR = INODE_NR(rip);
	m_out.RES_FILE_SIZE_LO = rip->i_size;
	m_out.RES_FILE_SIZE_HI = 0;
	
	/* These values are needed for VFS working: */
	m_out.RES_MODE = rip->i_mode;
	m_out.RES_UID = use_uid;
	m_out.RES_GID = use_gid;
  } else
	/* only CREATE keeps the node referenced */
	put_inode(rip);		/* drop the inode of the newly made entry */

  /* put_inode(dirp); */	/* no need to drop the inode of the parent dir */
  return(OK);
}

/*===========================================================================*
 *				do_newnode				     *
 *===========================================================================*/
PUBLIC int do_newnode(void)
{
/* ... */
/* NEW_NODE?; idem SYMLINK */
  int r;
  struct inode *rip;
  mode_t newnode_mode;

  if (read_only) return(EROFS);	/* paranoia */

  /* Read request message */
  newnode_mode = (mode_t) m_in.REQ_MODE;
/*
  newnode_uid = (uid_t) m_in.REQ_UID;
  newnode_gid = (gid_t) m_in.REQ_GID;
  */
  /* Are we creating a directory, a regular file,
   * or something else (which will fail)?
   */
  if (S_ISDIR(newnode_mode)) {
	/* Directories need allocation of space now (even if using MKNOD)
	 * for the server to work, since we rely on the fact
	 * directories have a non-zero start cluster number.
	 */
  } else if (S_ISREG(newnode_mode)) {
  } else
	return(EINVAL);

/* Create a new inode by calling new_node(). 
 * New_node() is called by do_open(), do_mknod(), and do_mkdir().  
 * In all cases it allocates a new inode, makes a directory entry for it in
 * the dirp directory with string name, and initializes it.  
 * It returns a pointer to the inode if it can do this; 
 * otherwise it returns NULL.  It always sets 'err_code'
 * to an appropriate value (OK or an error code).
 * 
 * The parsed path rest is returned in 'parsed' if parsed is nonzero. It
 * has to hold at least NAME_MAX bytes.
 */

#if 0
	if ( (rip = alloc_inode((ldirp)->i_dev, bits)) == NULL) {
		/* Cannot creat new inode: out of inodes. */
		return(NULL);
	}

	/* Force inode to the disk before making directory entry to make
	 * the system more robust in the face of a crash: an inode with
	 * no directory entry is much better than the opposite.
	 */
pour FAT, pour MKDIR, l´ordre est le suivant:
 -creer inode vide (get_free_inode)
 -allouer cluster, remplir avec . et ..
 -add_entry
#if 0  
MKDIR
  /* Get the inode numbers for . and .. to enter in the directory. */
  dotdot = ldirp->i_num;	/* parent's inode number */
  dot = rip->i_num;		/* inode number of the new dir itself */

  /* Now make dir entries for . and .. unless the disk is completely full. */
  /* Use dot1 and dot2, so the mode of the directory isn't important. */
  rip->i_mode = (mode_t) m_in.REQ_MODE;	/* set mode */
  r1 = search_dir(rip, dot1, &dot, ENTER, IGN_PERM);/* enter . in the new dir*/
  r2 = search_dir(rip, dot2, &dotdot, ENTER, IGN_PERM); /* enter .. in the new
							 dir */

  /* If both . and .. were successfully entered, increment the link counts. */
  if (r1 == OK && r2 == OK) {
	  /* Normal case.  It was possible to enter . and .. in the new dir. */
	  rip->i_nlinks++;	/* this accounts for . */
	  ldirp->i_nlinks++;	/* this accounts for .. */
	  ldirp->i_dirt = DIRTY;	/* mark parent's inode as dirty */
  } else {
	  /* It was not possible to enter . or .. probably disk was full -
	   * links counts haven't been touched. */
	  if(search_dir(ldirp, string, NULL, DELETE, IGN_PERM) != OK)
		  panic("Dir disappeared: %ul", rip->i_num);
	  rip->i_nlinks--;	/* undo the increment done in new_node() */
  }
  rip->i_dirt = DIRTY;		/* either way, i_nlinks has changed */
#endif
 
	rip->i_nlinks++;
/* MKNOD:
(zone_t) m_in.REQ_DEV
 */
	rip->i_zone[0] = z0;		/* major/minor device numbers */
	rw_inode(rip, WRITING);		/* force inode to disk now */

#endif

  /* The created inode does not have associated directory entry */
  rip->i_flags |= I_ORPHAN;

  if (r == OK) {
	m_out.RES_INODE_NR = INODE_NR(rip);
	m_out.RES_FILE_SIZE_LO = rip->i_size;
	m_out.RES_FILE_SIZE_HI = 0;
	
	/* These values are needed for VFS working: */
	m_out.RES_MODE = rip->i_mode;
	m_out.RES_UID = use_uid;
	m_out.RES_GID = use_gid;
  } else {
  /* If an error occurred, release inode. */
	if (rip) put_inode(rip);
  }
  return(r);
}

/*===========================================================================*
 *				do_unlink				     *
 *===========================================================================*/
PUBLIC int do_unlink()
{
/* Perform the UNLINK or RMDIR request. The code for these two is almost
 * the same. They differ only in some condition testing.
 *
 * unlink() may be used by the superuser to do dangerous things; rmdir() may
 * not. However these distinctions are supposedly handled at VFS level.
 * So we intend to perform any possible operation.
 */
  struct inode *rip;
  struct inode *dirp;
  int r;
  char string[NAME_MAX];
  size_t len;
  
  if (read_only) return(EROFS);	/* paranoia */

  /* Copy the last component */
#if 0
  len = min( (unsigned) m_in.REQ_PATH_LEN, sizeof(string));
#else
  len = m_in.REQ_PATH_LEN;
  if (len > sizeof(string) || len > NAME_MAX+1) {
	return(ENAMETOOLONG);
  }
#endif
  r = sys_safecopyfrom(VFS_PROC_NR, (cp_grant_id_t) m_in.REQ_GRANT,
		    (vir_bytes) 0, (vir_bytes) string, len, D);
  if (r != OK) return r;
#if 0
  NUL(string, len, sizeof(string));
  memset();
#endif

  /* Temporarily open the dir. */
  /* Get the inode from the request msg. Do not increase the inode refcount*/
  if( (dirp = fetch_inode((ino_t) m_in.REQ_INODE_NR)) == NULL)
	  return(EINVAL);
  /*CHECKME: is it needed? is the dir open anyway? should we increase ref? */
  /* get_inode(dirp); */

  /* The last directory exists.  Does the file also exist? */
  r = advance(dirp, &rip, string, IGN_PERM);

  /* If error, return inode. */
  if(r != OK) {
	  /* Mount point? */
  	if (r == EENTERMOUNT || r == ELEAVEMOUNT) {
  	  	put_inode(rip);
  		r = EBUSY;
  	}
	/* put_inode(dirp); */
	return(r);
  }
  
  /* Now test if the call is allowed, separately for unlink() and rmdir(). */
  if(m_in.m_type == REQ_RMDIR) {
	r = remove_dir(dirp, rip, string);
  } else { /* request is UNLINK */
	/* Only the superuser may unlink directories, but it can unlink
	 * any dir. However in current versions of VFS, the check is not
	 * performed; so we choose the safe way, and prevent any unlink(DIR)
	 *
	 * A possible way is to record if the last LOOKUP (on that directory)
	 * was done with root credentials or not, and use that information
	 * to decide if the operation can be authorized; however it would
	 * remove the stateless property of the current protocol, and
	 * allow possible races and subsequent security holes if/when
	 * VFS becomes multi-tasked.
	 * Also note that POSIX allows file systems to arbitrary denegate
	 * directories unlinking.
	 */
	if (IS_DIR(rip)) r = EPERM;

	/* Actually try to unlink the file; fails if parent is mode 0 etc. */
	if (r == OK) r = unlink_file(dirp, rip, string);
  }

  /* If unlink was possible, it has been done, otherwise it has not. */
  put_inode(rip);
  /* put_inode(dirp); */
  return(r);
}

/*===========================================================================*
 *				remove_dir				     *
 *===========================================================================*/
PRIVATE int remove_dir(
  struct inode *dirp,		 	/* parent directory */
  struct inode *rip,			/* directory to be removed */
  char dir_name[NAME_MAX])		/* name of directory to be removed */
{
/* A directory has to be removed. Five conditions have to met:
 *	- The file must be a directory
 *	- The directory must be empty (except for . and ..)
 *	- The final component of the path must not be . or ..
 *	- The directory must not be the root of a mounted file system (VFS)
 *	- The directory must not be anybody's root/working directory (VFS)
 */
  int r;

  /* is_empty_dir() checks that rip is a directory too. */
  if ((r = is_empty_dir(rip)) != OK)
  	return(r);

  if (strcmp(dir_name, ".") == 0 || strcmp(dir_name, "..") == 0)
	return(EINVAL);
  if ( rip->i_flags & (I_ROOTDIR|I_MOUNTPOINT) ) {
	/* cannot remove 'root' or a mounpoint */
	return(EBUSY);
  }

  /* Actually try to unlink the file; fails if parent is mode 0 etc. */
  if ((r = unlink_file(dirp, rip, dir_name)) != OK)
	return r;

  /* Unlink . and .. from the dir. The super user can link and unlink any dir,
   * so don't make too many assumptions about them.
   */
/* FIXME: is it really necessary for FAT? */
  (void) unlink_file(rip, NULL, /*dot1*/ ".");
  (void) unlink_file(rip, NULL, /*dot2*/ "..");
/* truncate(0); free clusters... */
  return(OK);
}

/*===========================================================================*
 *				unlink_file				     *
 *===========================================================================*/
PRIVATE int unlink_file(
  struct inode *dirp,		/* parent directory of file */
  struct inode *rip,		/* inode of file, may be NULL too. */
  char file_name[NAME_MAX])	/* name of file to be removed */
{
/* Unlink 'file_name'; rip must be the inode of 'file_name' or NULL. */

  ino_t numb;			/* inode number */
  int	r;

#if 0
/* FIXME: only used for . and .. above; or perhaps RENAME? */
  /* If rip is not NULL, it is used to get faster access to the inode. */
  if (rip == NULL) {
  	/* Search for file in directory and try to get its inode. */
	err_code = search_dir(dirp, file_name, &numb, LOOK_UP, IGN_PERM);
	if (err_code == OK) rip = get_inode(dirp->i_dev, (int) numb);
	if (err_code != OK || rip == NULL) return(err_code);
  } else {
	dup_inode(rip);		/* inode will be returned with put_inode */
  }
#endif

  r = del_direntry(dirp, rip);

  if (r == OK) {
#if 0
	rip->i_nlinks--;	/* entry deleted from parent's dir */
	rip->i_update |= CTIME;
#endif
	rip->i_ctime = TIME_UPDATED;
	rip->i_flags |= I_DIRTY;
  }

  /* did not get_inode/dup_inode above, so no need to
   * put_inode(rip); */
  return(r);
}
