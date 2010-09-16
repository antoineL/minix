/* This file contains file and directory reading file system call handlers.
 *
 * The entry points into this file are:
 *   do_read		perform the READ file system request
 *   do_bread		perform the BREAD file system request
 *   do_write		perform the WRITE file system request
 *   do_bwrite		perform the BWRITE file system request
 *   do_readwrite	perform the READ, WRITE, BREAD and BWRITE requests
 *   do_inhibread	perform the INHIBREAD file system request
 *   read_ahead		xxx
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

/*
 */

FORWARD _PROTOTYPE( struct buf *rahead, (struct inode *rip, block_t baseblock,
			u64_t position, unsigned bytes_ahead)		);

PRIVATE off_t rdahedpos;         /* position to read ahead */
PRIVATE struct inode *rdahed_inode;      /* pointer to inode to read ahead */

/*===========================================================================*
 *				do_read					   *
 *===========================================================================*/
PUBLIC int do_read(void)
{
/* Read from a file.
 */
  int r, rw_flag, n;
  cp_grant_id_t gid;
  u64_t position64;
  off_t position, f_size, bytes_left;
  size_t nrbytes;
  unsigned int off, cum_io, chunk, block_size;
  block_t b;
  struct inode *rip;
  struct buf *bp;

  
  r = OK;
  
  /* Get the values from the request msg. Do not increase the inode refcount*/
  if ((rip = fetch_inode((ino_t) m_in.REQ_INODE_NR)) == NULL)
	return(EINVAL);
  position64 = make64((unsigned long) m_in.REQ_SEEK_POS_LO,
  		    (unsigned long) m_in.REQ_SEEK_POS_HI);
  position = (off_t) m_in.REQ_SEEK_POS_LO;
/* BUG if !block && (position<0 || posHi!=0 */
  gid = (cp_grant_id_t) m_in.REQ_GRANT;
  nrbytes = (size_t) m_in.REQ_NBYTES;
  f_size = rip->i_size;
  
#if 0
  rdwt_err = OK;		/* set to EIO if disk error occurs */
#endif

  cum_io = 0;
  /* Split the transfer into chunks that don't span two blocks. */
  while (nrbytes > 0) {
	off = rem64u(position64, block_size);
#if 0
	off = ((unsigned int) position) % block_size; /* offset in blk*/
	chunk = min(nrbytes, block_size - off);
#else
	chunk = nrbytes < (block_size-off) ? nrbytes : block_size - off;
#endif
	bytes_left = f_size - position;
	if (position >= f_size) break;	/* we are beyond EOF */
	if (chunk > (unsigned int) bytes_left) chunk = bytes_left;

	/* Read or write 'chunk' bytes. */
	if (ex64hi(position64) != 0)
		panic("rw_chunk: position too high");
	b = bmap(rip, (off_t) ex64lo(position64));
	
	if (b == NO_BLOCK) {
		DBGprintf(("FATfs: in do_read, bmap returned NO_BLOCK...???\n"));
		/* Reading from a nonexistent block.  Must read as all zeros.*/
		bp = get_block(NO_DEV, NO_BLOCK, NORMAL);    /* get a buffer */
		zero_block(bp);
	} else {
		/* Read and read ahead if convenient. */
		bp = rahead(rip, b, position64, nrbytes);
	}
	
	/* In all cases, bp now points to a valid buffer. */
	if (bp == NULL) 
		panic("bp not valid in rw_chunk; this can't happen");
	
	/* Copy a chunk from the block buffer to user space. */
	r = sys_safecopyto(VFS_PROC_NR, gid, (vir_bytes) /*buf_off*/ cum_io,
			 (vir_bytes) (bp->b_data+off), (size_t) chunk, D);
	put_block(bp);

	if (r != OK) break;	/* EOF reached */
/*
	if (rdwt_err < 0) break;
 */

	/* Update counters and pointers. */
	nrbytes -= chunk;	      /* bytes yet to be read */
	cum_io += chunk;	      /* bytes read so far */
	position64 = add64ul(position64, chunk);	/* position within the file */
	position += (off_t) chunk;	/* position within the file */
  }

  m_out.RES_SEEK_POS_LO = position; /* It might change later and the VFS
					 has to know this value */
  m_out.RES_SEEK_POS_LO = ex64lo(position64); 
  m_out.RES_SEEK_POS_HI = ex64hi(position64); 
  
#if 0
  /* Check to see if read-ahead is called for, and if so, set it up. */
  if(rip->i_seek == NO_SEEK &&
     (unsigned int) position % block_size == 0 &&
     (regular || mode_word == I_DIRECTORY)) {
	rdahed_inode = rip;
	rdahedpos = position;
  } 
  rip->i_seek = NO_SEEK;
#endif  

#if 0  
  if (rdwt_err != OK) r = rdwt_err;	/* check for disk error */
  if (rdwt_err == END_OF_FILE) r = OK;

  if (r == OK) {
	if (rw_flag == READING) rip->i_update |= ATIME;
	if (rw_flag == WRITING) rip->i_update |= CTIME | MTIME;
	rip->i_dirt = DIRTY;		/* inode is thus now dirty */
  }
#endif

  m_out.RES_NBYTES = cum_io;
  
  return(r);
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
  u64_t position64;
  off_t position, f_size, bytes_left;
  size_t nrbytes;
  unsigned int off, cum_io, chunk, block_size;
  block_t b;
  struct inode *rip;
  struct buf *bp;
/*
  int block_spec;
  int regular;
  unsigned chunk;
  mode_t mode_word;
 */ 
  struct inode blk_rip;  /* Pseudo inode for rw_chunk */

  
  r = OK;
  
  /* Get the values from the request message */
  assert(m_in.REQ_DEV2 == dev);
  position64 = make64((unsigned long) m_in.REQ_SEEK_POS_LO,
  		    (unsigned long) m_in.REQ_SEEK_POS_HI);
  position = (off_t) m_in.REQ_SEEK_POS_LO;
/* BUG if !block && (position<0 || posHi!=0 */
  gid = (cp_grant_id_t) m_in.REQ_GRANT;
  nrbytes = (size_t) m_in.REQ_NBYTES;

#if 0
  rdwt_err = OK;		/* set to EIO if disk error occurs */
#endif

  cum_io = 0;
  /* Split the transfer into chunks that don't span two blocks. */
  while (nrbytes > 0) {
	off = rem64u(position64, block_size);	/* offset in blk*/
#if 0
	off = ((unsigned int) position) % block_size; /* offset in blk*/
	chunk = min(nrbytes, block_size - off);
#else
	chunk = nrbytes < (block_size-off) ? nrbytes : block_size - off;
#endif
	b = div64u(position64, block_size);
	
	/* Read and read ahead if convenient. */
/*FIXME*/ rip = &blk_rip;
	bp = rahead(rip, b, position64, nrbytes);
	
	/* In all cases, bp now points to a valid buffer. */
	if (bp == NULL) 
		panic("bp not valid in rw_chunk; this can't happen");
	
	/* Copy a chunk from the block buffer to user space. */
	r = sys_safecopyto(VFS_PROC_NR, gid, (vir_bytes) /*buf_off*/ cum_io,
			 (vir_bytes) (bp->b_data+off), (size_t) chunk, D);
	put_block(bp);

	if (r != OK) break;	/* EOF reached */
/*
	if (rdwt_err < 0) break;
 */

	/* Update counters and pointers. */
	nrbytes -= chunk;	      /* bytes yet to be read */
	cum_io += chunk;	      /* bytes read so far */
	position64 = add64ul(position64, chunk);	/* position within the file */
	position += (off_t) chunk;	/* position within the file */
  }

  m_out.RES_SEEK_POS_LO = position; /* It might change later and the VFS
					 has to know this value */
  m_out.RES_SEEK_POS_LO = ex64lo(position64); 
  m_out.RES_SEEK_POS_HI = ex64hi(position64); 

#if 0  
  /* Check to see if read-ahead is called for, and if so, set it up. */
/*FIXME: should work... */
  if((unsigned int) position % block_size == 0) {
	rdahed_inode = rip;
	rdahedpos = position;
  } 
#endif

#if 0  
  if (rdwt_err != OK) r = rdwt_err;	/* check for disk error */
  if (rdwt_err == END_OF_FILE) r = OK;
#endif

  m_out.RES_NBYTES = cum_io;
  
  return(r);
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
  off_t position, f_size, bytes_left;
  size_t nrbytes;
  unsigned int off, cum_io, chunk, block_size;
  block_t b;
  struct inode *rip;
  struct buf *bp;
/*
  int block_spec;
  int regular;
  unsigned chunk;
  mode_t mode_word;
 */ 
  struct inode blk_rip;  /* Pseudo inode for rw_chunk */

  if (read_only)
	return EROFS;

  r = OK;
  
  /* Get the values from the request msg. Do not increase the inode refcount*/
  if ((rip = fetch_inode((ino_t) m_in.REQ_INODE_NR)) == NULL)
	return(EINVAL);
  position64 = make64((unsigned long) m_in.REQ_SEEK_POS_LO,
  		    (unsigned long) m_in.REQ_SEEK_POS_HI);
  position = (off_t) m_in.REQ_SEEK_POS_LO;
/* BUG if !block && (position<0 || posHi!=0 */
  gid = (cp_grant_id_t) m_in.REQ_GRANT;
  nrbytes = (size_t) m_in.REQ_NBYTES;
  f_size = rip->i_size;
  
  /* Check in advance to see if file will grow too big. */
/* FIXME: check overflow */
  if (position > (off_t) (sb.maxFilesize - nrbytes))
	return(EFBIG);

#if 0
/* Clear the zone containing present EOF if hole about
 * to be created.  This is necessary because all unwritten
 * blocks prior to the EOF must read as zeros.
 */
  if(position > f_size) clear_zone(rip, f_size, 0);
#endif
	    
#if 0
  rdwt_err = OK;		/* set to EIO if disk error occurs */
#endif

  cum_io = 0;
  /* Split the transfer into chunks that don't span two blocks. */
  while (nrbytes > 0) {
	off = rem64u(position64, block_size);	/* offset in blk*/
#if 0
	off = ((unsigned int) position) % block_size; /* offset in blk*/
	chunk = min(nrbytes, block_size - off);
#else
	chunk = nrbytes < (block_size-off) ? nrbytes : block_size - off;
#endif
	if (ex64hi(position64) != 0)
		panic("rw_chunk: position too high");
	b = bmap(rip, (off_t) ex64lo(position64));
	
	if (b == NO_BLOCK) {
#if 0
		/* Writing to a nonexistent block. Create and enter in inode.*/
		if ((bp = new_block(rip, (off_t) ex64lo(position))) == NULL)
#endif
			return /*FIXME!!! (err_code)*/ EIO;
	} else {
		/* Normally an existing block to be partially overwritten is first read
		 * in.  However, a full block need not be read in.  If it is already in
		 * the cache, acquire it, otherwise just acquire a free buffer.
		 */
		n = (chunk == block_size ? NO_READ : NORMAL);
		if (off == 0 && (off_t) ex64lo(position64) >= rip->i_size) 
			n = NO_READ;
		bp = get_block(dev, b, n);
	}
	
	/* In all cases, bp now points to a valid buffer. */
	if (bp == NULL) 
		panic("bp not valid in rw_chunk; this can't happen");
	
	if (chunk != block_size &&
	    (off_t) ex64lo(position64) >= rip->i_size && off == 0) {
		zero_block(bp);
	}

	/* Copy a chunk from user space to the block buffer. */
	r = sys_safecopyfrom(VFS_PROC_NR, gid, (vir_bytes) /*buf_off*/ cum_io,
			   (vir_bytes) (bp->b_data+off), (size_t) chunk, D);
	bp->b_dirt = DIRTY;
	put_block(bp);

	if (r != OK) break;	/* EOF reached */
/*
	if (rdwt_err < 0) break;
 */

	/* Update counters and pointers. */
	nrbytes -= chunk;	      /* bytes yet to be read */
	cum_io += chunk;	      /* bytes read so far */
	position64 = add64ul(position64, chunk);	/* position within the file */
	position += (off_t) chunk;	/* position within the file */
  }

  m_out.RES_SEEK_POS_LO = position; /* It might change later and the VFS
					 has to know this value */
  m_out.RES_SEEK_POS_LO = ex64lo(position64); 
  m_out.RES_SEEK_POS_HI = ex64hi(position64); 
  
#if 0
  /* On write, update file size and access time. */
  if (regular || mode_word == I_DIRECTORY) {
	if (position > f_size) rip->i_size = position;
  } 
  rip->i_seek = NO_SEEK;
#endif  

#if 0  
  if (rdwt_err != OK) r = rdwt_err;	/* check for disk error */
  if (rdwt_err == END_OF_FILE) r = OK;

  if (r == OK) {
	if (rw_flag == READING) rip->i_update |= ATIME;
	if (rw_flag == WRITING) rip->i_update |= CTIME | MTIME;
	rip->i_dirt = DIRTY;		/* inode is thus now dirty */
  }
#endif

  m_out.RES_NBYTES = cum_io;
  
  return(r);
}

/*===========================================================================*
 *				do_bwrite				   *
 *===========================================================================*/
PUBLIC int do_bwrite(void)
{
/* Write data to a block device.
 */
  int r, rw_flag, n;
  cp_grant_id_t gid;
  u64_t position64;
  off_t position, f_size, bytes_left;
  size_t nrbytes;
  unsigned int off, cum_io, chunk, block_size;
  block_t b;
  struct inode *rip;
  struct buf *bp;
/*
  int block_spec;
  int regular;
  unsigned chunk;
  mode_t mode_word;
 */ 
  struct inode blk_rip;  /* Pseudo inode for rw_chunk */

  if (read_only)
	return EROFS;

  r = OK;
  
  /* Get the values from the request message */
  assert(m_in.REQ_DEV2 == dev);
  position64 = make64((unsigned long) m_in.REQ_SEEK_POS_LO,
  		    (unsigned long) m_in.REQ_SEEK_POS_HI);
  position = (off_t) m_in.REQ_SEEK_POS_LO;
/* BUG if !block && (position<0 || posHi!=0 */
  gid = (cp_grant_id_t) m_in.REQ_GRANT;
  nrbytes = (size_t) m_in.REQ_NBYTES;
  
#if 0
  rdwt_err = OK;		/* set to EIO if disk error occurs */
#endif

  cum_io = 0;
  /* Split the transfer into chunks that don't span two blocks. */
  while (nrbytes > 0) {
	off = rem64u(position64, block_size);	/* offset in blk*/
#if 0
	off = ((unsigned int) position) % block_size; /* offset in blk*/
	chunk = min(nrbytes, block_size - off);
#else
	chunk = nrbytes < (block_size-off) ? nrbytes : block_size - off;
#endif
	b = div64u(position64, block_size);
	
	/* Normally an existing block to be partially overwritten is first read
	 * in.  However, a full block need not be read in.  If it is already in
	 * the cache, acquire it, otherwise just acquire a free buffer.
	 */
	n = (chunk == block_size ? NO_READ : NORMAL);
	bp = get_block(dev, b, n);

	/* In all cases, bp now points to a valid buffer. */
	if (bp == NULL) 
		panic("bp not valid in rw_chunk; this can't happen");
	
	/* Copy a chunk from user space to the block buffer. */
	r = sys_safecopyfrom(VFS_PROC_NR, gid, (vir_bytes) /*buf_off*/ cum_io,
			   (vir_bytes) (bp->b_data+off), (size_t) chunk, D);
	bp->b_dirt = DIRTY;
	put_block(bp);

	if (r != OK) break;	/* EOF reached */
/*
	if (rdwt_err < 0) break;
 */

	/* Update counters and pointers. */
	nrbytes -= chunk;	      /* bytes yet to be read */
	cum_io += chunk;	      /* bytes read so far */
	position64 = add64ul(position64, chunk);	/* position within the file */
	position += (off_t) chunk;	/* position within the file */
  }

  m_out.RES_SEEK_POS_LO = position; /* It might change later and the VFS
					 has to know this value */
  m_out.RES_SEEK_POS_LO = ex64lo(position64); 
  m_out.RES_SEEK_POS_HI = ex64hi(position64); 
  
#if 0  
  if (rdwt_err != OK) r = rdwt_err;	/* check for disk error */
  if (rdwt_err == END_OF_FILE) r = OK;
#endif

  m_out.RES_NBYTES = cum_io;
  
  return(r);
}

/*===========================================================================*
 *				read_ahead				   *
 *===========================================================================*/
PUBLIC void read_ahead(void)
{
/* Read a block into the cache before it is needed. */
  register struct inode *rip;
  struct buf *bp;
  block_t b;

  if(!rdahed_inode)
	return;

  rip = rdahed_inode;		/* pointer to inode to read ahead from */
  rdahed_inode = NULL;	/* turn off read ahead */
#if 0
  if ( (b = read_map(rip, rdahedpos)) == NO_BLOCK) return;	/* at EOF */
#endif
  assert(rdahedpos > 0); /* So we can safely cast it to unsigned below */

  bp = rahead(rip, b, cvul64( (unsigned long) rdahedpos), block_size);
  put_block(bp /*, PARTIAL_DATA_BLOCK */);
}


/*===========================================================================*
 *				rahead					   *
 *===========================================================================*/
PRIVATE struct buf *rahead(rip, baseblock, position, bytes_ahead)
register struct inode *rip;	/* pointer to inode for file to be read */
block_t baseblock;		/* block at current position */
u64_t position;			/* position within file */
unsigned bytes_ahead;		/* bytes beyond position for immediate use */
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
/*
  dev_t dev;
 */
  struct buf *bp;
  static unsigned int readqsize = 0;
  static struct buf **read_q;

  if(readqsize != num_bufs) {
	if(readqsize > 0) {
		assert(read_q != NULL);
		free(read_q);
	}
	if(!(read_q = malloc(sizeof(read_q[0])*num_bufs)))
		panic("couldn't allocate read_q");
	readqsize = num_bufs;
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
  if (bp->b_dev != NO_DEV) return(bp);

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

  fragment = rem64u(position, block_size);
  position = sub64u(position, fragment);
  bytes_ahead += fragment;

  blocks_ahead = (bytes_ahead + block_size - 1) / block_size;

#if 0
  if (block_spec && rip->i_size == 0) {
	blocks_left = (block_t) NR_IOREQS;
  } else {
	blocks_left = (block_t) (rip->i_size-ex64lo(position)+(block_size-1)) /
								block_size;

	/* Go for the first indirect block if we are in its neighborhood. */
	if (!block_spec) {
		scale = rip->i_sp->s_log_zone_size;
		ind1_pos = (off_t) rip->i_ndzones * (block_size << scale);
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
  if (blocks_ahead > blocks_left) blocks_ahead = blocks_left;

  read_q_size = 0;

  /* Acquire block buffers. */
  for (;;) {
	read_q[read_q_size++] = bp;

	if (--blocks_ahead == 0) break;

	/* Don't trash the cache, leave 4 free. */
/* FIXME: beware of the FAT12 cache... const.h? */
	if (bufs_in_use >= num_bufs - 4) break;

	block++;

	bp = get_block(dev, block, PREFETCH);
	if (bp->b_dev != NO_DEV) {
		/* Oops, block already in the cache, get out. */
		put_block(bp /*, FULL_DATA_BLOCK */);
		break;
	}
  }
  rw_scattered(dev, read_q, read_q_size, READING);
  return(get_block(dev, baseblock, NORMAL));
}

/*===========================================================================*
 *				do_inhibread				     *
 *===========================================================================*/
PUBLIC int do_inhibread(void)
{
  struct inode *rip;
  
  if((rip = fetch_inode(m_in.REQ_INODE_NR)) == NULL)
	  return(EINVAL);

  /* inhibit read ahead */
  rip->i_flags |= I_SEEK;	
  
  return(OK);
}
