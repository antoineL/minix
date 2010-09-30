/* This file contains file and directory reading file system call handlers.
 *
 * The entry points into this file are:
 *   do_read		perform the READ file system request
 *   do_inhibread	perform the INHIBREAD file system request
 *   do_bread		perform the BREAD file system request
 *   do_write		perform the WRITE file system request
 *   do_bwrite		perform the BWRITE file system request
 *   do_readwrite	perform the READ, WRITE, BREAD and BWRITE requests
 *   read_ahead		try to launch the read ahead of some more blocks
 *
 * Warning: this code is not reentrant (use static local variables, without mutex)
 *
 * Auteur: Antoine Leca, aout 2010.
 * Updated:
 */

#include "inc.h"

#include <stdlib.h>
#include <dirent.h>

#ifdef	COMPAT316
#include "compat.h"	/* MUST come before <minix/sysutil.h> */
#endif
#include <minix/u64.h>
#include <minix/com.h>		/* VFS_BASE */
#include <minix/safecopies.h>
#include <minix/syslib.h>	/* sys_safecopies{from,to} */
#include <minix/sysutil.h>	/* panic */

/* Private global variables: */
  /* position to read ahead */
PRIVATE unsigned long rdahedpos;
  /* pointer to inode to read ahead */
PRIVATE struct inode *rdahed_inode;

/* Private functions:
 *   rahead		?
 */
FORWARD _PROTOTYPE( struct buf *rahead, (struct inode *rip, block_t baseblock,
			u64_t position, unsigned bytes_ahead)		);
FORWARD _PROTOTYPE( int set_newsize,
			(struct inode *, unsigned long newsize)		);
FORWARD _PROTOTYPE( int clear_area, (struct inode *rip,
			unsigned long start, unsigned long end)		);

/*===========================================================================*
 *				do_read					   *
 *===========================================================================*/
PUBLIC int do_read(void)
{
/* Read from a file.
 */
  int r, rw_flag, n;
  cp_grant_id_t gid;
  unsigned long position, file_size, bytes_left;
  size_t rem_bytes, chunk;
  vir_bytes cum_bytes;
  unsigned int off;
  block_t b;
  struct inode *rip;
  struct buf *bp;
  
  r = OK;
  
  /* Get the values from the request msg. Do not increase the inode refcount*/
  if ((rip = fetch_inode((ino_t) m_in.REQ_INODE_NR)) == NULL)
	return(EINVAL);
  rem_bytes = (size_t) m_in.REQ_NBYTES;
  position = (unsigned long) m_in.REQ_SEEK_POS_LO;
  if (m_in.REQ_SEEK_POS_HI || position>sb.maxFilesize) {
	rem_bytes = 0;		/* indicates EOF, do not read */
  }
  gid = (cp_grant_id_t) m_in.REQ_GRANT;
  file_size = rip->i_size;

  DBGprintf(("FATfs: read in %lo, %u bytes from %lu...\n", INODE_NR(rip), rem_bytes, position));

#if 0
  rdwt_err = OK;		/* set to EIO if disk error occurs */
#endif

  cum_bytes = 0;
  /* Split the transfer into chunks that don't span two blocks. */
  while (rem_bytes > 0) {
	off = position & bcc.bMask; /* offset in blk*/
	chunk = rem_bytes < (bcc.bpBlock-off) ? rem_bytes : bcc.bpBlock - off;
DBGprintf(("chunk is %u bytes, at %lx (BS=%d, EOF=%x)\n", chunk, position, bcc.bpBlock, file_size));

	bytes_left = file_size - position;
	if (position >= file_size) break;	/* we are beyond EOF */
	if (chunk > bytes_left) chunk = bytes_left;

	/* Read or write 'chunk' bytes. */
	b = bmap(rip, position);
	
	if (b == NO_BLOCK) {
		DBGprintf(("FATfs: in do_read, bmap returned NO_BLOCK!?\n"));
		/* Reading from a nonexistent block.  Must read as all zeros.*/
		bp = get_block(NO_DEV, NO_BLOCK, NO_READ);
		zero_block(bp);
	} else {
		/* Read and read ahead if convenient. */
		bp = rahead(rip, b, cvul64(position), rem_bytes);
	}
	
	/* In all cases, bp now points to a valid buffer. */
	assert(bp != NULL);
	
	/* Copy a chunk from the block buffer to user space. */
	r = sys_safecopyto(VFS_PROC_NR, gid, cum_bytes,
			 (vir_bytes) (bp->b_data+off), chunk, D);
	put_block(bp);

/* FIXME: END_OF_FILE stuff here??? */
	if (r != OK) break;	/* EOF reached */
/*
	if (rdwt_err < 0) break;
 */

	/* Update counters and pointers. */
	rem_bytes -= chunk;	/* bytes yet to be read */
	cum_bytes += chunk;	/* bytes read so far */
	position += chunk;	/* position within the file */
  }

  /* Check to see if read-ahead is called for, and if so, set it up. */
  if ( (rip->i_flags & I_SEEK)==0
    && (position & bcc.bMask) == 0) {
	rdahed_inode = rip;
	rdahedpos = position;
  }
  rip->i_flags &= ~I_SEEK;

  /* It might change later and the VFS has to know this value */
  m_out.RES_SEEK_POS_LO = position;
  m_out.RES_SEEK_POS_HI = m_in.REQ_SEEK_POS_HI;
  m_out.RES_NBYTES = cum_bytes;
  
#if 0  
  if (rdwt_err != OK) r = rdwt_err;	/* check for disk error */
/* FIXME: END_OF_FILE... */
  if (rdwt_err == END_OF_FILE) r = OK;
#endif
  if (r == OK) {
	rip->i_atime = TIME_UPDATED;
	rip->i_flags |= I_ACCESSED|I_DIRTY;	/* inode is thus now dirty */
  }
  
  return(r);
}

/*===========================================================================*
 *				do_inhibread				     *
 *===========================================================================*/
PUBLIC int do_inhibread(void)
{
/* Inhibit possible read-ahead (called as part of LSEEK system call) */
  struct inode *rip;
  
  if((rip = fetch_inode(m_in.REQ_INODE_NR)) == NULL)
	  return(EINVAL);

  rip->i_flags |= I_SEEK;	
  
  return(OK);
}

/*===========================================================================*
 *				do_bread				   *
 *===========================================================================*/
PUBLIC int do_bread(void)
{
/* Read from a block device.
 */
  int r, rw_flag, n;
  cp_grant_id_t gid;
  u64_t position;
  size_t rem_bytes;
  vir_bytes cum_bytes, chunk;
  unsigned int off;
  block_t b;
  struct buf *bp;
  struct inode *rip;
  struct inode blk_rip;  /* Pseudo inode for rw_chunk */

  r = OK;

  /* Get the values from the request message */
  if((dev_t) m_in.REQ_DEV2 != dev) return(EINVAL);
  position = make64((unsigned long) m_in.REQ_SEEK_POS_LO,
		    (unsigned long) m_in.REQ_SEEK_POS_HI);
  gid = (cp_grant_id_t) m_in.REQ_GRANT;
  rem_bytes = (size_t) m_in.REQ_NBYTES;

#if 0
  rdwt_err = OK;		/* set to EIO if disk error occurs */
#endif

  cum_bytes = 0;
  b = div64u(position, bcc.bpBlock);
  off = rem64u(position, bcc.bpBlock); /* offset in block */
  /* Split the transfer into chunks that don't span two blocks. */
  while (rem_bytes > 0) {
	chunk = rem_bytes < (bcc.bpBlock-off) ? rem_bytes : bcc.bpBlock - off;
	
	/* Read and read ahead if convenient. */
/*FIXME!!!*/ rip = &blk_rip;
	bp = rahead(rip, b, position, rem_bytes);
	
	/* In all cases, bp now points to a valid buffer. */
	assert(bp != NULL);
	
	/* Copy a chunk from the block buffer to user space. */
	r = sys_safecopyto(VFS_PROC_NR, gid, cum_bytes,
			 (vir_bytes) (bp->b_data+off), chunk, D);
	put_block(bp);

	if (r != OK) break;	/* EOF reached */
/*
	if (rdwt_err < 0) break;
 */

	/* Update counters and pointers. */
	rem_bytes -= chunk;	/* bytes yet to be read */
	cum_bytes += chunk;	/* bytes read so far */
	position = add64ul(position, chunk); /* position within the file */

	++b;			/* next block */
	off = 0;		/* start at beginning of block */
  }

  m_out.RES_SEEK_POS_LO = ex64lo(position); 
  m_out.RES_SEEK_POS_HI = ex64hi(position); 
  m_out.RES_NBYTES = cum_bytes;
  
#if 0  
  /* Check to see if read-ahead is called for, and if so, set it up. */
/*FIXME: should work... */
  if( ((unsigned int) position & sb.brelmask) == 0) {
	rdahed_inode = rip;
	rdahedpos = position;
  } 
#endif

#if 0  
  if (rdwt_err != OK) r = rdwt_err;	/* check for disk error */
  if (rdwt_err == END_OF_FILE) r = OK;
#endif

  return(r);
}

/*===========================================================================*
 *				read_ahead				     *
 *===========================================================================*/
PUBLIC void read_ahead(void)
{
/* Read a block into the cache before it is needed.
 *
 * Warning: this code is not reentrant (use static local variables, without mutex)
 */
  register struct inode *rip;
  struct buf *bp;
  block_t b;

  if(!rdahed_inode)
	return;

  rip = rdahed_inode;		/* pointer to inode to read ahead from */
  rdahed_inode = NULL;		/* turn off read ahead */

#if 0
  if ( (b = read_map(rip, rdahedpos)) == NO_BLOCK) return;	/* at EOF */
#endif
  assert(rdahedpos > 0); /* So we can safely cast it to unsigned below */

DBGprintf(("read_ahead:: rahead(rip, b=%d, cvul64( (unsigned long) rdahedpos=%ld), bcc.bpBlock=%d)\n", b, rdahedpos, bcc.bpBlock));
  bp = rahead(rip, b, cvul64( (unsigned long) rdahedpos), bcc.bpBlock);
  put_block(bp);
}

/*===========================================================================*
 *				rahead					   *
 *===========================================================================*/
PRIVATE struct buf *rahead(
  register struct inode *rip,	/* pointer to inode for file to be read */
  block_t baseblock,		/* block at current position */
  u64_t position,		/* position within file */
  unsigned bytes_ahead)		/* bytes beyond position for immediate use */
{
/* Fetch a block from the cache or the device.  If a physical read is
 * required, prefetch as many more blocks as convenient into the cache.
 * This usually covers bytes_ahead and is at least BLOCKS_MINIMUM.
 * The device driver may decide it knows better and stop reading at a
 * cylinder boundary (or after an error).  Rw_scattered() puts an optional
 * flag on all reads to allow this.
 *
 * Warning: this code is not reentrant (use static local variables, without mutex)
 */
/* Minimum number of blocks to prefetch. */
# define BLOCKS_MINIMUM		(num_bufs < 50 ? 18 : 32)
  int block_spec, scale, read_q_size;
  unsigned int blocks_ahead, fragment;
  block_t block, blocks_left;
  off_t ind1_pos;
  struct buf *bp;
  static unsigned int readqsize = 0;
  static struct buf **read_q;

  if(readqsize != bcc.nBufs) {
	if(readqsize > 0) {
		assert(read_q != NULL);
		free(read_q);
	}
	if(!(read_q = malloc(sizeof(read_q[0]) * bcc.nBufs)))
		panic("couldn't allocate read_q");
	readqsize = bcc.nBufs;
DBGprintf(("allocated %d bytes for read_q at %p\n", sizeof(read_q[0]) * bcc.nBufs, read_q));
  }

/*
  block_spec = (rip->i_mode & I_TYPE) == I_BLOCK_SPECIAL;
  if (block_spec) 
	dev = (dev_t) rip->i_zone[0];
  else 
	dev = rip->i_dev;
 */  

  block = baseblock;
  bp = get_block(dev, block, PREFETCH);
#if 0
  if (bp->b_dev != NO_DEV) return(bp);
#else
  if (bp->b_dev == dev) {
DBGprintf(("rahead found block %d in cache, return direct %p!\n", block, bp->dp));
	return(bp);
  }
#endif

  /* The best guess for the number of blocks to prefetch:  A lot.
   * It is impossible to tell what the device looks like, so we don't even
   * try to guess the geometry, but leave it to the driver.
   *
   * The floppy driver can read a full track with no rotational delay, and it
   * avoids reading partial tracks if it can, so handing it enough buffers to
   * read two tracks is perfect.  (Two, because some diskette types have
   * an odd number of sectors per track, so a block may span tracks.)
   *
   * The disk drivers don't try to be smart.  With todays disks it is
   * impossible to tell what the real geometry looks like, so it is best to
   * read as much as you can.  With luck the caching on the drive allows
   * for a little time to start the next read.
   *
   * The current solution below is a bit of a hack, it just reads blocks from
   * the current file position hoping that more of the file can be found.  A
   * better solution must look at the already available zone pointers and
   * indirect blocks (but don't call bmap!).
   */

  fragment = rem64u(position, bcc.bpBlock);
  position = sub64u(position, fragment);
  bytes_ahead += fragment;

  blocks_ahead = (bytes_ahead + bcc.bpBlock - 1) / bcc.bpBlock;

DBGprintf(("rahead1 prefetching from %ld... max %ld (%ld b.)\n", block, blocks_ahead, bytes_ahead));

#if 0
/* Change to peeking the FAT... */
  if (block_spec && rip->i_size == 0) {
	blocks_left = (block_t) NR_IOREQS;
  } else {
	blocks_left = (block_t) (rip->i_size-ex64lo(position)+bcc.bpBlock-1) /
								bcc.bpBlock;

	/* Go for the first indirect block if we are in its neighborhood. */
	if (!block_spec) {
		scale = rip->i_sp->s_log_zone_size;
		ind1_pos = (off_t) rip->i_ndzones * (bcc.bpBlock << scale);
		if ((off_t) ex64lo(position) <= ind1_pos &&
		   rip->i_size > ind1_pos) {
			blocks_ahead++;
			blocks_left++;
		}
	}
  }
#endif

  /* No more than the maximum request. */
  if (blocks_ahead > NR_IOREQS) blocks_ahead = NR_IOREQS;

#if 0
  /* Read at least the minimum number of blocks, but not after a seek. */
  if (blocks_ahead < BLOCKS_MINIMUM && rip->i_seek == NO_SEEK)
	blocks_ahead = BLOCKS_MINIMUM;
#endif

  /* Can't go past end of file. */
/*
  if (blocks_ahead > blocks_left) blocks_ahead = blocks_left;
 */
DBGprintf(("rahead2 prefetching from %ld... max %ld (%ld b.)\n", block, blocks_ahead, bytes_ahead));

  read_q_size = 0;

  /* Acquire block buffers. */
  for (;;) {
	read_q[read_q_size++] = bp;

	if (--blocks_ahead == 0) break;

	/* Don't trash the cache, leave some free. */
	if (bufs_in_use >= bcc.nBufs - KEPT_BUFS) break;

	block++;

	bp = get_block(dev, block, PREFETCH);
	if (bp->b_dev != NO_DEV) {
		/* Oops, block already in the cache, get out. */
		put_block(bp);
		break;
	}
  }
  rw_scattered(dev, read_q, read_q_size, READING);
  return(get_block(dev, baseblock, NORMAL));
}

/*===========================================================================*
 *				do_write				   *
 *===========================================================================*/
PUBLIC int do_write(void)
{
/* Write data to a file.
 */
  int r, rw_flag, n;
  cp_grant_id_t gid;
  u64_t position64;
  unsigned long position, file_size, bytes_left;
  size_t rem_bytes, chunk;
  vir_bytes cum_bytes;
  unsigned int off;
  block_t b;
  struct inode *rip;
  struct buf *bp;
/*
  int block_spec;
  int regular;
  unsigned chunk;
  mode_t mode_word;
 */ 

  if (read_only) return EROFS;

  r = OK;
  
  /* Get the values from the request msg. Do not increase the inode refcount*/
  if ((rip = fetch_inode((ino_t) m_in.REQ_INODE_NR)) == NULL)
	return(EINVAL);
  if (rip->i_Attributes & ATTR_READONLY) return(EPERM);

/*
  position64 = make64((unsigned long) m_in.REQ_SEEK_POS_LO,
  		    (unsigned long) m_in.REQ_SEEK_POS_HI);
*/
  rem_bytes = (size_t) m_in.REQ_NBYTES;
  position = (unsigned long) m_in.REQ_SEEK_POS_LO;
  gid = (cp_grant_id_t) m_in.REQ_GRANT;
  rem_bytes = (size_t) m_in.REQ_NBYTES;
  file_size = rip->i_size;
  
  /* Check in advance to see if file will grow too big.
   * Take care of possible overflows if rem_bytes>sb.maxFilesize
   */
  if ( m_in.REQ_SEEK_POS_HI
    || position > sb.maxFilesize
    || position > (sb.maxFilesize - rem_bytes) )
	return(EFBIG);

  /* Clear the zone containing present EOF if hole about
   * to be created.  This is necessary because all unwritten
   * blocks prior to the EOF must read as zeros.
   */
  if(position > file_size) set_newsize(rip, position);

#if 0
  rdwt_err = OK;		/* set to EIO if disk error occurs */
#endif

  cum_bytes = 0;
  /* Split the transfer into chunks that don't span two blocks. */
  while (rem_bytes > 0) {
	off = position & bcc.bMask; /* offset in blk*/
	chunk = rem_bytes < (bcc.bpBlock-off) ? rem_bytes : bcc.bpBlock - off;
DBGprintf(("chunk is %u bytes, at %lx (BS=%d, EOF=%u)\n", chunk, off, bcc.bpBlock, file_size));

#if 0
	bytes_left = file_size - position;
	if (position >= file_size) break;	/* we are beyond EOF */
	if (chunk > bytes_left) chunk = bytes_left;
#endif

	/* Write 'chunk' bytes. */
	b = bmap(rip, position);

	if (b == NO_BLOCK) {
#if 0
		/* Writing to a nonexistent block. Create and enter in inode.*/
		if ((bp = new_block(rip, (off_t) ex64lo(position))) == NULL)
#endif
			return /*FIXME!!! (err_code)*/ EIO;
	} else {
		/* Normally an existing block to be partially overwritten is
		 * first read in. However, a full block need not be read in.
		 * If it is already in the cache, acquire it, otherwise just
		 * acquire a free buffer.
		 */
		n = (chunk == bcc.bpBlock ? NO_READ : NORMAL);
		if (off == 0 && position >= rip->i_size) 
			n = NO_READ;
		bp = get_block(dev, b, n);
	}
	
	/* In all cases, bp now points to a valid buffer. */
	assert(bp != NULL);

/* FIXME: if we advance i_size and are going past a block boundary
 * which is NOT a cluster boundary, we should make sure the newly
 * allocated block is zero-ed...
 */

#if 0	
	if (chunk != bcc.bpBlock &&
	    (off_t) ex64lo(position64) >= rip->i_size && off == 0) {
		zero_block(bp);
	}
#else
	if (n == NO_READ)
		zero_block(bp);
#endif

	/* Copy a chunk from user space to the block buffer. */
	r = sys_safecopyfrom(VFS_PROC_NR, gid, cum_bytes,
			   (vir_bytes) (bp->b_data+off), chunk, D);
	bp->b_dirt = DIRTY;
	put_block(bp);

	if (r != OK) break;
/*CANNOT BE... EOF reached */
/*
	if (rdwt_err < 0) break;
 */

	/* Update counters and pointers. */
	cum_bytes += chunk;	/* bytes written so far */
	rem_bytes -= chunk;	/* bytes yet to be written */
	position += chunk;	/* position within the file */
  }

  /* It might change later and the VFS has to know this value */
  m_out.RES_SEEK_POS_LO = position;
  m_out.RES_SEEK_POS_HI = m_in.REQ_SEEK_POS_HI;
  m_out.RES_NBYTES = cum_bytes;
  
  /* Update file size and access time. */
/* FIXME: check what happens if some error came in... */
  if (position > rip->i_size) {
	rip->i_size = position;
	rip->i_flags |= I_DIRTY; /* inode is thus now dirty */
  }
  rip->i_flags &= ~I_SEEK;

#if 0  
  if (rdwt_err != OK) r = rdwt_err;	/* check for disk error */
  if (rdwt_err == END_OF_FILE) r = OK;
#endif
  if (r == OK) {
	rip->i_mtime = TIME_UPDATED;
	rip->i_ctime = TIME_UPDATED;
	rip->i_flags |= I_MTIME|I_DIRTY; /* inode is thus now dirty */
  }
  return(r);
}

/*===========================================================================*
 *				do_bwrite				   *
 *===========================================================================*/
PUBLIC int do_bwrite(void)
{
/* Write data to a block device.
 */
  int r, rw_flag;
  cp_grant_id_t gid;
  u64_t position;
  size_t rem_bytes, chunk;
  vir_bytes cum_bytes;
  unsigned off;
  block_t b;
  struct buf *bp;

  if (read_only) return EROFS;

  r = OK;
  
  /* Get the values from the request message */
  if((dev_t) m_in.REQ_DEV2 != dev) return(EINVAL);
  position = make64((unsigned long) m_in.REQ_SEEK_POS_LO,
  		    (unsigned long) m_in.REQ_SEEK_POS_HI);
  rem_bytes = (size_t) m_in.REQ_NBYTES;
  gid = (cp_grant_id_t) m_in.REQ_GRANT;
  
#if 0
  rdwt_err = OK;		/* set to EIO if disk error occurs */
#endif

  cum_bytes = 0;
  b = div64u(position, bcc.bpBlock);
  off = rem64u(position, bcc.bpBlock);	/* offset in blk*/
  /* Split the transfer into chunks that don't span two blocks. */
  while (rem_bytes > 0) {
	chunk = rem_bytes < (bcc.bpBlock-off) ? rem_bytes : bcc.bpBlock - off;

	/* Normally an existing block to be partially overwritten is first
	 * read in. However, a full block need not be read in.
	 * If it is already in the cache, acquire it, otherwise just
	 * acquire a free buffer.
	 */
	bp = get_block(dev, b, chunk == bcc.bpBlock ? NO_READ : NORMAL);

	/* In all cases, bp now points to a valid buffer. */
	assert(bp != NULL);
	
	/* Copy a chunk from user space to the block buffer. */
	r = sys_safecopyfrom(VFS_PROC_NR, gid, cum_bytes,
			   (vir_bytes) (bp->b_data+off), chunk, D);
	bp->b_dirt = DIRTY;
	put_block(bp);

	if (r != OK) break;	/* EOF reached */
/*
	if (rdwt_err < 0) break;
 */

	/* Update counters and pointers. */
	cum_bytes += chunk;	/* bytes written so far */
	rem_bytes -= chunk;	/* bytes yet to be written */
	position = add64ul(position, chunk); /* position within the file */

	++b;			/* next block */
	off = 0;		/* start at beginning of block */
  }

#if 0  
  if (rdwt_err != OK) r = rdwt_err;	/* check for disk error */
  if (rdwt_err == END_OF_FILE) r = OK;
#endif

  m_out.RES_NBYTES = cum_bytes;
  /* It might change later and the VFS has to know this value */
  m_out.RES_SEEK_POS_LO = ex64lo(position); 
  m_out.RES_SEEK_POS_HI = ex64hi(position); 
  
  return(r);
}

/*===========================================================================*
 *				do_ftrunc				     *
 *===========================================================================*/
PUBLIC int do_ftrunc(void)
{
  struct inode *rip;
  unsigned long start, end;
  int r;
  
  /* Get the values from the request msg. Do not increase the inode refcount*/
  if( (rip = fetch_inode((ino_t) m_in.REQ_INODE_NR)) == NULL)
	  return(EINVAL);

  start = m_in.REQ_TRC_START_LO;
  end = m_in.REQ_TRC_END_LO;

  if (end == 0 && m_in.REQ_TRC_END_HI == 0) {
	if ( m_in.REQ_TRC_START_HI || start > sb.maxFilesize )
		return(EFBIG);
	r = set_newsize(rip, start);
  }
  else {
	if ( m_in.REQ_TRC_END_HI || m_in.REQ_TRC_START_HI )
		return(EINVAL);	/* make no sense */
	r = clear_area(rip, start, end);
  }  
  return(r);
}
    
/*===========================================================================*
 *				set_newsize				     *
 *===========================================================================*/
PRIVATE int set_newsize(struct inode *rip, unsigned long newsize)
{
/* Set inode to a certain size, freeing any zones no longer referenced
 * and updating the size in the inode.
 * If the inode is extended, the extra space is allocated.
 */
  int r;
  unsigned long file_size, last_block, cut_mark;

  file_size = rip->i_size;
  /* Free the actual space if truncating. */
  if(newsize < file_size) {
/* FIXME: only do the remaining of block... 
 * but take care of case file_size pointing in the last block, just below 4GB
 */
	/* Zeroes the remaining part of the last allocated block.
	 * We should take care of avoiding overflows.
	 */
	cut_mark = newsize|sb.cMask; /* last addresseable byte after cut */
	if (newsize >= (FAT_FILESIZE_MAX & ~sb.cMask)) /* avoid overflow */
	  	r = clear_area(rip, newsize, FAT_FILESIZE_MAX);
	else 
	  	r = clear_area(rip, newsize, cut_mark+1);
  	if (r != OK)
  		return(r);
	/* Now unchain all remaining clusters (if any) */
	if ( (newsize>>sb.cnShift) < (file_size>>sb.cnShift) ) {
		/* freechain */
	}
  }

  /* Clear the rest of the last zone if expanding. */
/*
  if(newsize > file_size) clear_zone(rip, rip->i_size, 0);
 */

  /* Next correct the inode size. */
  rip->i_size = newsize;
  rip->i_mtime = TIME_UPDATED;
  rip->i_ctime = TIME_UPDATED;
  rip->i_flags |= I_MTIME|I_DIRTY; /* inode is thus now dirty */
  return(OK);
}

/*===========================================================================*
 *				clear_area				     *
 *===========================================================================*/
PRIVATE int clear_area(struct inode *rip,
  unsigned long position,
  unsigned long end)		/* range of bytes to free (end uninclusive) */
{
/* Cut an arbitrary hole in a file. With FAT, which does not support sparse
 * files, it means zeroing the relevant space.
 *
 * This function does not care whether the effect is to truncate the file
 * entirelly, which in the FAT file system is a special condition. Adjusting
 * the final size of the inode is to be done by the caller too, if wished.
 *
 * Consumers of this function currently are set_newsize(), used to
 * free or zero data blocks, but also to implement the ftruncate() and
 * truncate() system calls, and the F_FREESP fcntl().
 */
  int r, n;
  unsigned long file_size;
  size_t rem_bytes, chunk;
  unsigned int off;
  block_t b;
  struct buf *bp;

  file_size = rip->i_size;
  if(end > file_size)		/* freeing beyond end makes no sense */
	end = rip->i_size;
  if(end <= position)		/* end is uninclusive, so position<end */
	return(EINVAL);
  
  rem_bytes = end - position;
  r = OK;			/* always succeeds! */

#if 0
  rdwt_err = OK;		/* set to EIO if disk error occurs */
#endif

  /* Split the operation into chunks that don't span two blocks. */
  while (rem_bytes > 0) {
	off = position & bcc.bMask; /* offset in blk*/
	chunk = rem_bytes < (bcc.bpBlock-off) ? rem_bytes : bcc.bpBlock - off;
DBGprintf(("chunk is %u bytes, at %lx (BS=%d)\n", chunk, off, bcc.bpBlock));

	b = bmap(rip, position);

	/* Normally an existing block to be partially overwritten is first
	 * read in. However, a full block need not be read in.  If it is already in
	 * the cache, acquire it, otherwise just acquire a free buffer.
	 */
	n = (chunk == bcc.bpBlock ? NO_READ : NORMAL);

	if (b == NO_BLOCK || (bp = get_block(dev, b, n)) == NULL ) {
#if 0
Cannot happen! (unless serious failure in FAT reading, or FS incoherency)
		/* Writing to a nonexistent block. Create and enter in inode.*/
		if ((bp = new_block(rip, (off_t) ex64lo(position))) == NULL)
#endif
			return /*FIXME!!! (err_code)*/ EIO;
	}
	assert(bp != NULL);

	if (n == NO_READ)
		zero_block(bp);
	else {
		memset(& bp->b_data[off], 0, chunk);
	}

	bp->b_dirt = DIRTY;
	put_block(bp);

	/* Update counters and pointers. */
	rem_bytes -= chunk;	/* bytes yet to be cleared */
	position += chunk;	/* position within the file */
  }

  /* Update file size and access time. */
  if (r == OK) {
	rip->i_mtime = TIME_UPDATED;
	rip->i_ctime = TIME_UPDATED;
	rip->i_flags |= I_MTIME|I_DIRTY; /* inode is thus now dirty */
  }
  return(r);
}
