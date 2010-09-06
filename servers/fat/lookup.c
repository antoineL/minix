/* This file provides path-to-inode lookup functionality.
 *
 * The entry points into this file are:
 *   do_lookup		perform the LOOKUP file system request
 *   do_putnode		perform the PUTNODE file system call
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

#if 0
#include "inode.h"
#endif

PRIVATE char user_path[PATH_MAX+1];  /* pathname to be processed */

FORWARD _PROTOTYPE( int ltraverse, (struct inode *rip, char *suffix)	);
FORWARD _PROTOTYPE( int parse_path, (ino_t dir, ino_t root, int flags,
	struct inode **res_inop, size_t *offsetp, int *symlinkp)	);


FORWARD _PROTOTYPE( char *get_name, (char *name, char string[NAME_MAX+1]) );

#if 0

FORWARD _PROTOTYPE( int get_mask, (vfs_ucred_t *ucred)			);
FORWARD _PROTOTYPE( int access_as_dir, (struct inode *ino,
			struct hgfs_attr *attr, int uid, int mask)	);
FORWARD _PROTOTYPE( int next_name, (char **ptr, char **start,
			char name[NAME_MAX+1])				);
FORWARD _PROTOTYPE( int go_up, (char path[PATH_MAX], struct inode *ino,
			struct inode **res_ino, struct hgfs_attr *attr)	);
FORWARD _PROTOTYPE( int go_down, (char path[PATH_MAX],
			struct inode *ino, char *name,
			struct inode **res_ino, struct hgfs_attr *attr)	);
#endif

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
 *				get_mask				 *
 *===========================================================================*/
PRIVATE int get_mask(
  vfs_ucred_t *ucred)		/* credentials of the caller */
{
  /* Given the caller's credentials, precompute a search access mask to test
   * against directory modes.
   */
  int i;

  if (ucred->vu_uid == use_uid) return S_IXUSR;

  if (ucred->vu_gid == use_gid) return S_IXGRP;

  for (i = 0; i < ucred->vu_ngroups; i++)
	if (ucred->vu_sgroups[i] == use_gid) return S_IXGRP;

  return S_IXOTH;
}

/*===========================================================================*
 *                             parse_path				   *
 *===========================================================================*/
PRIVATE int parse_path(dir_ino, root_ino, flags, res_inop, offsetp, symlinkp)
ino_t dir_ino;
ino_t root_ino;
int flags;
struct inode **res_inop;
size_t *offsetp;
int *symlinkp;
{
/* Parse the path in user_path, starting at dir_ino. If the path is the empty
 * string, just return dir_ino. It is upto the caller to treat an empty
 * path in a special way. Otherwise, if the path consists of just one or
 * more slash ('/') characters, the path is replaced with ".". Otherwise,
 * just look up the first (or only) component in path after skipping any
 * leading slashes. 
 */
  int r, leaving_mount;
  struct inode *rip, *dir_ip;
  char *cp, *next_cp; /* component and next component */
  char component[NAME_MAX+1];

  /* Start parsing path at the first component in user_path */
  cp = user_path;  

  /* No symlinks encountered yet */
  *symlinkp = 0;

  /* Find starting inode inode according to the request message */
  if((rip = find_inode( /*fs_dev,*/ dir_ino)) == NULL) 
	return(ENOENT);

  /* If dir has been removed return ENOENT. */
/*CHEKCME
  if (rip->i_nlinks == NO_LINK) return(ENOENT);
 */
 
  get_inode(rip);

  /* If the given start inode is a mountpoint, we must be here because the file
   * system mounted on top returned an ELEAVEMOUNT error. In this case, we must
   * only accept ".." as the first path component.
   */
  leaving_mount = rip->i_mountpoint; /* True iff rip is a mountpoint */

  /* Scan the path component by component. */
  while (TRUE) {
	if(cp[0] == '\0') {
		/* We're done; either the path was empty or we've parsed all 
		 components of the path */
		
		*res_inop = rip;
		*offsetp += cp - user_path;

		/* Return EENTERMOUNT if we are at a mount point */
		if (rip->i_mountpoint) return(EENTERMOUNT);
		
		return(OK);
	}

	while(cp[0] == '/') cp++;
	next_cp = get_name(cp, component);

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
		if (rip->i_index == root_ino) {
			cp = next_cp;
			continue;	/* Ignore the '..' at a process' root 
					 and move on to the next component */
		}

		if (IS_ROOT(rip)) {
			/* comment: we cannot be the root FS! */
			/* Climbing up to parent FS */

			put_inode(rip);
			*offsetp += cp - user_path; 
			return(ELEAVEMOUNT);
		}
	}

	/* Only check for a mount point if we are not coming from one. */
	if (!leaving_mount && rip->i_mountpoint) {
		/* Going to enter a child FS */

		*res_inop = rip;
		*offsetp += cp - user_path;
		return(EENTERMOUNT);
	}

	/* There is more path.  Keep parsing.
	 * If we're leaving a mountpoint, skip directory permission checks.
	 */
	dir_ip = rip;
	r = advance(dir_ip, &rip, leaving_mount ? dot2 : component, CHK_PERM);
	if(r == ELEAVEMOUNT || r == EENTERMOUNT)
		r = OK;

	if (r != OK) {
		put_inode(dir_ip);
		return(r);
	}
	leaving_mount = 0;

	/* The call to advance() succeeded.  Fetch next component. */
	if (S_ISLNK(rip->i_mode)) {

		if (next_cp[0] == '\0' && (flags & PATH_RET_SYMLINK)) {
			put_inode(dir_ip);
			*res_inop = rip;
			*offsetp += next_cp - user_path;
			return(OK);
		}

		/* Extract path name from the symlink file */
		r = ltraverse(rip, next_cp);
		next_cp = user_path;
		*offsetp = 0;

		/* Symloop limit reached? */
		if (++(*symlinkp) > SYMLOOP_MAX)
			r = ELOOP;

		if (r != OK) {
			put_inode(dir_ip);
			put_inode(rip);
			return(r);
		}

		if (next_cp[0] == '/') {
                        put_inode(dir_ip);
                        put_inode(rip);
                        return(ESYMLINK);
		}
	
		put_inode(rip);
		get_inode(dir_ip);
		rip = dir_ip;
	} 

	put_inode(dir_ip);
	cp = next_cp; /* Process subsequent component in next round */
  }
}

/*===========================================================================*
 *                             ltraverse				   *
 *===========================================================================*/
PRIVATE int ltraverse(rip, suffix)
register struct inode *rip;	/* symbolic link */
char *suffix;			/* current remaining path. Has to point in the
				 * user_path buffer
				 */
{
/* Traverse a symbolic link. Copy the link text from the inode and insert
 * the text into the path. Return error code or report success. Base 
 * directory has to be determined according to the first character of the
 * new pathname.
 */
  
  block_t blink;	/* block containing link text */
  size_t llen;		/* length of link */
  size_t slen;		/* length of suffix */
  struct buf *bp;	/* buffer containing link text */
  char *sp;		/* start of link text */

  if ((blink = bmap(rip, (off_t) 0)) == NO_BLOCK)
	return(EIO);

  bp = get_block(dev, blink, NORMAL);
  llen = (size_t) rip->i_size;
/* CHEW ME (!) The link is in Unicode!!! */
  sp = (char *) bp->dp;
  slen = strlen(suffix);

  /* The path we're parsing looks like this:
   * /already/processed/path/<link> or
   * /already/processed/path/<link>/not/yet/processed/path
   * After expanding the <link>, the path will look like
   * <expandedlink> or
   * <expandedlink>/not/yet/processed
   * In both cases user_path must have enough room to hold <expandedlink>.
   * However, in the latter case we have to move /not/yet/processed to the
   * right place first, before we expand <link>. When strlen(<expandedlink>) is
   * smaller than strlen(/already/processes/path), we move the suffix to the
   * left. Is strlen(<expandedlink>) greater then we move it to the right. Else
   * we do nothing.
   */ 

  if (slen > 0) { /* Do we have path after the link? */
	/* For simplicity we require that suffix starts with a slash */
	if (suffix[0] != '/') {
		panic("ltraverse: suffix does not start with a slash");
	}

	/* To be able to expand the <link>, we have to move the 'suffix'
	 * to the right place.
	 */
	if (slen + llen + 1 > sizeof(user_path))
		return(ENAMETOOLONG);/* <expandedlink>+suffix+\0 does not fit*/
	if ((unsigned) (suffix-user_path) != llen) { 
		/* Move suffix left or right */
		memmove(&user_path[llen], suffix, slen+1);
	}
  } else {
  	if (llen + 1 > sizeof(user_path))
  		return(ENAMETOOLONG); /* <expandedlink> + \0 does not fix */
  		
	/* Set terminating nul */
	user_path[llen]= '\0';
  }

  /* Everything is set, now copy the expanded link to user_path */
  memmove(user_path, sp, llen);

  put_block(bp);
  return(OK);
}

/*===========================================================================*
 *				advance					   *
 *===========================================================================*/
PUBLIC int advance(
struct inode *dirp,		/* inode for directory to be searched */
struct inode **res_inop,
char string[NAME_MAX],		/* component name to look for */
int chk_perm)			/* check permissions when string is looked up*/
{
/* Given a directory and a component of a path, look up the component in
 * the directory, find the inode, open it, and return a pointer to its inode
 * slot.
 */
  int r;
  ino_t numb;
  struct inode *rip;

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

#if 0 /*obsolete code */
  /* If 'string' is not present in the directory, signal error. */
  if ( (r = search_dir(dirp, string, &numb, LOOK_UP, chk_perm)) != OK) {
	return(r);
  }

  /* The component has been found in the directory.  Get inode. */
  if ( (rip = get_inode(dirp->i_dev, (int) numb)) == NULL)  {
	return(NULL);
  }
#endif

  /* If 'string' is not present in the directory, signal error. */
  if ( (r = lookup_dir(dirp, string, &rip)) != OK) {
	return(r);
  }

  /* The component has been found in the directory.  Get inode. */
  get_inode(rip);

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
  if (rip->i_mountpoint) {
	/* Mountpoint encountered, report it */
	r = EENTERMOUNT;
  }
  *res_inop = rip;
  return(r);
}

/*===========================================================================*
 *				get_name				   *
 *===========================================================================*/
PRIVATE char *get_name(path_name, string)
char *path_name;		/* path name to parse */
char string[NAME_MAX+1];	/* component extracted from 'old_name' */
{
/* Given a pointer to a path name in fs space, 'path_name', copy the first
 * component to 'string' (truncated if necessary, always nul terminated).
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
  ep = cp;
  while(ep[0] != '\0' && ep[0] != '/')
	ep++;

  len = (size_t) (ep - cp);

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



#if 0
/*===========================================================================*
 *				access_as_dir				 *
 *===========================================================================*/
PRIVATE int access_as_dir(ino, attr, uid, mask)
struct inode *ino;		/* the inode to test */
struct hgfs_attr *attr;		/* attributes of the inode */
int uid;			/* UID of the caller */
int mask;			/* search access mask of the caller */
{
/* Check whether the given inode may be accessed as directory.
 * Return OK or an appropriate error code.
 */
  mode_t mode;

  assert(attr->a_mask & HGFS_ATTR_MODE);

  /* The inode must be a directory to begin with. */
  if (!IS_DIR(ino)) return ENOTDIR;

  /* The caller must have search access to the directory. Root always does. */
  if (uid == 0) return OK;

  mode = get_mode(ino, attr->a_mode);

  return (mode & mask) ? OK : EACCES;
}

/*===========================================================================*
 *				next_name				 *
 *===========================================================================*/
PRIVATE int next_name(ptr, start, name)
char **ptr;			/* cursor pointer into path (in, out) */
char **start;			/* place to store start of name */
char name[NAME_MAX+1];		/* place to store name */
{
/* Get the next path component from a path.
 */
  char *p;
  int i;

  for (p = *ptr; *p == '/'; p++);

  *start = p;

  if (*p) {
	for (i = 0; *p && *p != '/' && i <= NAME_MAX; p++, i++)
		name[i] = *p;

	if (i > NAME_MAX)
		return ENAMETOOLONG;

	name[i] = 0;
  } else {
	strcpy(name, ".");
  }

  *ptr = p;
  return OK;
}

/*===========================================================================*
 *				go_up					 *
 *===========================================================================*/
PRIVATE int go_up(path, ino, res_ino, attr)
char path[PATH_MAX];		/* path to take the last part from */
struct inode *ino;		/* inode of the current directory */
struct inode **res_ino;		/* place to store resulting inode */
struct hgfs_attr *attr;		/* place to store inode attributes */
{
/* Given an inode, progress into the parent directory.
 */
  struct inode *parent;
  int r;

  pop_path(path);

  parent = ino->i_parent;
  assert(parent != NULL);

  if ((r = verify_path(path, parent, attr, NULL)) != OK)
	return r;

  get_inode(parent);

  *res_ino = parent;

  return r;
}

/*===========================================================================*
 *				go_down					 *
 *===========================================================================*/
PRIVATE int go_down(path, parent, name, res_ino, attr)
char path[PATH_MAX];		/* path to add the name to */
struct inode *parent;		/* inode of the current directory */
char *name;			/* name of the directory entry */
struct inode **res_ino;		/* place to store resulting inode */
struct hgfs_attr *attr;		/* place to store inode attributes */
{
/* Given a directory inode and a name, progress into a directory entry.
 */
  struct inode *ino;
  int r, stale = 0;

  if ((r = push_path(path, name)) != OK)
	return r;

  dprintf(("HGFS: go_down: name '%s', path now '%s'\n", name, path));

  ino = lookup_dentry(parent, name);

  dprintf(("HGFS: lookup_dentry('%s') returned %p\n", name, ino));

  if (ino != NULL)
	r = verify_path(path, ino, attr, &stale);
  else
	r = hgfs_getattr(path, attr);

  dprintf(("HGFS: path query returned %d\n", r));

  if (r != OK) {
	if (ino != NULL) {
		put_inode(ino);

		ino = NULL;
	}

	if (!stale)
		return r;
  }

  dprintf(("HGFS: name '%s'\n", name));

  if (ino == NULL) {
	if ((ino = get_free_inode()) == NULL)
		return ENFILE;

	dprintf(("HGFS: inode %p ref %d\n", ino, ino->i_ref));

	ino->i_flags = MODE_TO_DIRFLAG(attr->a_mode);

	add_dentry(parent, name, ino);
  }

  *res_ino = ino;
  return OK;
}

/*===========================================================================*
 *			hgfs_lookup				 *
 *===========================================================================*/
PUBLIC int hgfs_lookup()
{
/* Resolve a path string to an inode.
 */
  ino_t dir_ino_nr, root_ino_nr;
  struct inode *cur_ino, *root_ino;
  struct inode *next_ino = NULL;
  struct hgfs_attr attr;
  char buf[PATH_MAX], path[PATH_MAX];
  char name[NAME_MAX+1];
  char *ptr, *last;
  vfs_ucred_t ucred;
  mode_t mask;
  size_t len;
  int r;

  dir_ino_nr = m_in.REQ_DIR_INO;
  root_ino_nr = m_in.REQ_ROOT_INO;
  len = m_in.REQ_PATH_LEN;

  /* Fetch the path name. */
  if (len < 1 || len > PATH_MAX)
	return EINVAL;

  r = sys_safecopyfrom(m_in.m_source, m_in.REQ_GRANT, 0,
	(vir_bytes) buf, len, D);

  if (r != OK)
	return r;

  if (buf[len-1] != 0) {
	printf("HGFS: VFS did not zero-terminate path!\n");

	return EINVAL;
  }

  /* Fetch the credentials, and generate a search access mask to test against
   * directory modes.
   */
  if (m_in.REQ_FLAGS & PATH_GET_UCRED) {
	if (m_in.REQ_UCRED_SIZE != sizeof(ucred)) {
		printf("HGFS: bad credential structure size\n");

		return EINVAL;
	}

	r = sys_safecopyfrom(m_in.m_source, m_in.REQ_GRANT2, 0,
		(vir_bytes) &ucred, m_in.REQ_UCRED_SIZE, D);

	if (r != OK)
		return r;
  }
  else {
	ucred.vu_uid = m_in.REQ_UID;
	ucred.vu_gid = m_in.REQ_GID;
	ucred.vu_ngroups = 0;
  }

  mask = get_mask(&ucred);

  /* Start the actual lookup. */
  dprintf(("HGFS: lookup: got query '%s'\n", buf));

  if ((cur_ino = find_inode(dir_ino_nr)) == NULL)
	return EINVAL;

  attr.a_mask = HGFS_ATTR_MODE | HGFS_ATTR_SIZE;

  if ((r = verify_inode(cur_ino, path, &attr)) != OK)
	return r;

  get_inode(cur_ino);

  if (root_ino_nr > 0)
	root_ino = find_inode(root_ino_nr);
  else
	root_ino = NULL;

  /* One possible optimization would be to check a path only right before the
   * first ".." in a row, and at the very end (if still necessary). This would
   * have consequences for inode validation, though.
   */
  for (ptr = last = buf; *ptr != 0; ) {
	if ((r = access_as_dir(cur_ino, &attr, ucred.vu_uid, mask)) != OK)
		break;

	if ((r = next_name(&ptr, &last, name)) != OK)
		break;

	dprintf(("HGFS: lookup: next name '%s'\n", name));

	if (!strcmp(name, ".") ||
			(cur_ino == root_ino && !strcmp(name, "..")))
		continue;

	if (!strcmp(name, "..")) {
		if (IS_ROOT(cur_ino))
			r = ELEAVEMOUNT;
		else
			r = go_up(path, cur_ino, &next_ino, &attr);
	} else {
		r = go_down(path, cur_ino, name, &next_ino, &attr);
	}

	if (r != OK)
		break;

	assert(next_ino != NULL);

	put_inode(cur_ino);

	cur_ino = next_ino;
  }

  dprintf(("HGFS: lookup: result %d\n", r));

  if (r != OK) {
	put_inode(cur_ino);

	/* We'd need support for these here. We don't have such support. */
	assert(r != EENTERMOUNT && r != ESYMLINK);

	if (r == ELEAVEMOUNT) {
		m_out.RES_OFFSET = (int) (last - buf);
		m_out.RES_SYMLOOP = 0;
	}

	return r;
  }

  m_out.RES_INODE_NR = INODE_NR(cur_ino);
  m_out.RES_MODE = get_mode(cur_ino, attr.a_mode);
  m_out.RES_FILE_SIZE_HI = ex64hi(attr.a_size);
  m_out.RES_FILE_SIZE_LO = ex64lo(attr.a_size);
  m_out.RES_UID = opt.uid;
  m_out.RES_GID = opt.gid;
  m_out.RES_DEV = NO_DEV;

  return OK;
}
#endif

/*===========================================================================*
 *                             do_lookup				 *
 *===========================================================================*/
PUBLIC int do_lookup(void)
{
  cp_grant_id_t grant, grant2;
  int r, r1, flags, symlinks;
  unsigned int len;
  size_t offset = 0, path_size, cred_size;
  vfs_ucred_t credentials;
  mode_t mask;
  ino_t dir_ino, root_ino;
  struct inode *rip;

  grant		= (cp_grant_id_t) m_in.REQ_GRANT;
  path_size	= (size_t) m_in.REQ_PATH_SIZE; /* Size of the buffer */
  len		= (int) m_in.REQ_PATH_LEN; /* including terminating nul */
  dir_ino	= (ino_t) m_in.REQ_DIR_INO;
  root_ino	= (ino_t) m_in.REQ_ROOT_INO;
  flags		= (int) m_in.REQ_FLAGS;

  /* Check length. */
  if(len > sizeof(user_path)) return(E2BIG);	/* too big for buffer */
  if(len == 0) return(EINVAL);			/* too small */

  /* Copy the pathname and set up caller's user and group id */
  r = sys_safecopyfrom(m_in.m_source, grant, /*offset*/ (vir_bytes) 0, 
            (vir_bytes) user_path, (size_t) len, D);
  if(r != OK) return(r);

  /* Verify this is a null-terminated path. */
  if(user_path[len - 1] != '\0') return(EINVAL);

  if(flags & PATH_GET_UCRED) { /* Do we have to copy uid/gid credentials? */
  	grant2 = (cp_grant_id_t) m_in.REQ_GRANT2;
  	cred_size = (size_t) m_in.REQ_UCRED_SIZE;

  	if (cred_size > sizeof(credentials)) return(EINVAL); /* Too big. */
  	r = sys_safecopyfrom(m_in.m_source, grant2, (vir_bytes) 0,
  			 (vir_bytes) &credentials, cred_size, D);
  	if (r != OK) return(r);
/*
  	caller_uid = credentials.vu_uid;
  	caller_gid = credentials.vu_gid;
 */
  } else {
  	memset(&credentials, 0, sizeof(credentials));
	credentials.vu_uid = m_in.REQ_UID;
	credentials.vu_gid = m_in.REQ_GID;
	credentials.vu_ngroups = 0;
/*
	caller_uid	= (uid_t) m_in.REQ_UID;
	caller_gid	= (gid_t) m_in.REQ_GID;
 */
  }
  mask = get_mask(&credentials);

  /* Lookup inode */
  rip = NULL;
  r = parse_path(dir_ino, root_ino, flags, &rip, &offset, &symlinks);

  if(symlinks != 0 && (r == ELEAVEMOUNT || r == EENTERMOUNT || r == ESYMLINK)){
	len = strlen(user_path)+1;
	if(len > path_size) return(ENAMETOOLONG);

	r1 = sys_safecopyto(m_in.m_source, grant, (vir_bytes) 0,
			(vir_bytes) user_path, (size_t) len, D);
	if(r1 != OK) return(r1);
  }

  if(r == ELEAVEMOUNT || r == ESYMLINK) {
	/* Report offset and the error */
	m_out.RES_OFFSET = offset;
	m_out.RES_SYMLOOP = symlinks;

	return(r);
  }

  if (r != OK && r != EENTERMOUNT) return(r);

  m_out.RES_INODE_NR = INODE_NR(rip);
  m_out.RES_MODE		= /*rip->i_mode*/ 0;
  m_out.RES_FILE_SIZE_LO = rip->i_size;
  m_out.RES_FILE_SIZE_HI = 0;
  m_out.RES_SYMLOOP		= symlinks;
  m_out.RES_UID = use_uid;
  m_out.RES_GID = use_gid;
  
  /* This is only valid for block and character specials. But it doesn't
   * cause any harm to set RES_DEV always.
  m_out.RES_DEV		= (dev_t) rip->i_zone[0];
 */
  m_out.RES_DEV = NO_DEV;

  if(r == EENTERMOUNT) {
	m_out.RES_OFFSET	= offset;
	put_inode(rip); /* Only return a reference to the final object */
  }

  return(r);
}
