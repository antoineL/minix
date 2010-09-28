/* The file system maintains a buffer cache to reduce the number of disk
 * accesses needed.  Whenever a read or write to the disk is done, a check is
 * first made to see if the block is in the cache.  This file manages the
 * cache.
 *
 * The entry points into this file are:
 *   init_cache	initialize the buffer pool
 *   do_sync	perform the SYNC file system call
 *   do_flush	perform the FLUSH file system call
 *   get_block	request to fetch a block for reading or writing from cache
 *   zero_block	overwrite a block with zeroes
 *   put_block	return a block previously requested with get_block
	_PROTOTYPE( void rw_scattered, (dev_t dev,
			struct buf **bufq, int bufqsize, int rw_flag)	);
 *
 * This current version assumes somewhat that 'dev' is unique,
 * and assumes in many places that blocks are all of the same size
 * named block_size, a global variable which value can be changed
 * at init time with init_cache().
 *
 * Warning: this code is not reentrant (use static local variables, without mutex)
 *
 * Auteur: Antoine Leca, aout 2010. From ../mfs/cache.c
 * Updated:
 */

#include "inc.h"

#include <stdlib.h>
#include <string.h>

#ifdef	COMPAT316
#include "compat.h"	/* MUST come before <minix/sysutil.h> */
#endif
#include <minix/u64.h>
#include <minix/com.h>
#include <minix/dmap.h>		/* MEMORY_MAJOR */
#include <minix/sysutil.h>	/* panic */
#include <minix/syslib.h>	/* alloc_contig, free_contig */

#ifdef	USE_VMCACHE
#include <minix/vm.h>
# ifndef VM_BLOCKID_NONE
#  undef USE_VMCACHE	/* disable USE_VMCACHE if not supported */
# endif
#endif

/* FIXME: work needed on the detection/reporting of error... */
#define END_OF_FILE   (-104)	/* eof detected */

/* CHECKME/FIXME: documentation! */
#define HASHSIZE	(num_bufs)
#define BUFHASH(b)	((b) % HASHSIZE)

/* Private global variables: */
  /* size of each buffer */
PRIVATE unsigned block_size;
  /* number of buffers in the cache */
PRIVATE int num_bufs;
  /* dynamically-allocated array of buf descriptors */
PRIVATE struct buf *buf;
  /* least recently used free block list */
PRIVATE TAILQ_HEAD(lruhead, buf) lru;
  /* the buffer hash table (dynamically-allocated) */
PRIVATE LIST_HEAD(bufhashhead, buf) *buf_hash;

#ifdef USE_VMCACHE
/*FIXME: some text to describe the interaction with the level 2 VM cache... */
PRIVATE int vmcache_avail = -1; /* 0 if not available, >0 if available. */
#endif

/* Private functions:
	_PROTOTYPE( void flushall, (dev_t dev)					);
 *   invalidate:  remove all the cache blocks on some device
 *   rm_lru:
 *   rw_block:    read or write a block from the disk itself
 */
FORWARD _PROTOTYPE( void invalidate, (dev_t) );
FORWARD _PROTOTYPE( void flushall, (dev_t) );
FORWARD _PROTOTYPE( void rw_block, (struct buf *, int) );
FORWARD _PROTOTYPE( void rm_lru, (struct buf *bp) );

/*===========================================================================*
 *				init_cache                                   *
 *===========================================================================*/
PUBLIC void init_cache(int new_num_bufs, unsigned int new_block_size)
{
/* Initialize the buffer pool, and set the (unique) size of the blocks. */
  register struct buf *bp;

  assert(new_num_bufs > 0);
  assert(new_block_size > 0);

  if (have_used_inode())
	panic("init_cache with inode(s) in use");

  if(num_bufs > 0) {
	assert(buf);
	(void) do_sync();
  	for (bp = &buf[0]; bp < &buf[num_bufs]; bp++) {
		if(bp->dp) {
			assert(bp->b_bytes > 0);
			free_contig(bp->dp, bp->b_bytes);
		}
	}
  }

  if(buf)
	free(buf);
  if(!(buf = calloc(sizeof(buf[0]), new_num_bufs)))
	panic("couldn't allocate buf list (%d)", new_num_bufs);

  if(buf_hash)
	free(buf_hash);
  if(!(buf_hash = calloc(sizeof(buf_hash[0]), new_num_bufs)))
	panic("couldn't allocate buf hash list (%d)", new_num_bufs);
  /* FIXME: consider explicit _INIT for the hash heads. */

  num_bufs = new_num_bufs;
  block_size = new_block_size;

  DBGprintf(("FATfs: %d blocks, %u+%u bytes each, = %lu bytes\n",
	num_bufs, (unsigned)block_size, usizeof(struct buf),
	(unsigned long)(block_size+sizeof(buf[0])) * num_bufs));

  bufs_in_use = 0;
  TAILQ_INIT(&lru);
  LIST_INIT(&buf_hash[0]);

  for (bp = &buf[num_bufs]; bp-- > &buf[0]; ) {
        bp->b_bytes = 0;
        bp->b_blocknr = NO_BLOCK;
        bp->b_dev = NO_DEV;
        bp->dp = NULL;
	TAILQ_INSERT_HEAD(&lru, bp, b_next);
	LIST_INSERT_HEAD(&buf_hash[0], bp, b_hash);
  }

#ifdef USE_VMCACHE
  vm_forgetblocks();
#endif
}

/*===========================================================================*
 *				do_sync					     *
 *===========================================================================*/
PUBLIC int do_sync()
{
/*FIXME: revise text (rw_inode) */
/* Perform the REQ_SYNC request.  Flush all the tables. 
 * The order in which the various tables are flushed is critical.  The
 * blocks must be flushed last, since rw_inode() leaves its results in
 * the block cache.
 */
/*
  struct inode *rip;
  struct buf *bp;
 */

  assert(num_bufs > 0);
  assert(buf);

  flush_inodes();

/* if FAT12, if the whole FAT is projected in a special area,
 * should be flushed NOW... in all the mirrors!
 */
/* update FSInfo (FAT32) => DIRTY
 * beware: cannot use cache, should be done using sector I/O
 */

#if 0 /*obsolete*/
  for(bp = &buf[0]; bp < &buf[num_bufs]; bp++)
	  if(!IS_FREE_BLOCK(bp) && bp->b_dirt == DIRTY) 
		  flushall(bp->b_dev);
#endif
  flushall(dev);
  return(OK);		/* sync() can't fail */
}


/*===========================================================================*
 *				do_flush				     *
 *===========================================================================*/
PUBLIC int do_flush()
{
/* Flush the blocks of a device from the cache after writing any dirty blocks
 * to disk.
 * Since we are not the / file system, and given that this request is intended
 * for unmounted devices, chances are this is never used.
 */
  dev_t req_dev = (dev_t) m_in.REQ_DEV;

  /* We do not fiddle with other partitions or file systems: */
  if(dev != req_dev) return(EINVAL);
#if 0
  if(dev == req_dev) return(EBUSY);
#endif

  /* sync'ing will write al dirty inodes to the cache */
  do_sync();

  flushall(req_dev);
  invalidate(req_dev);
  
  return(OK);
}

/*===========================================================================*
 *				flushall				     *
 *===========================================================================*/
PRIVATE void flushall(
  dev_t dev			/* device to flush */
)
{
/* Flush all dirty blocks for one device.
 * This is the only place where blocks are actually written to disk.
 *
 * Warning: this code is not reentrant (use static local variables, without mutex)
 */
  register struct buf *bp;
  static struct buf **dirty;	/* static so it isn't on stack */
  static unsigned int dirtylistsize = 0;
  int ndirty;

  if(dirtylistsize != num_bufs) {
	if(dirtylistsize > 0) {
		assert(dirty != NULL);
		free(dirty);
	}
	if(!(dirty = malloc(sizeof(dirty[0])*num_bufs)))
		panic("couldn't allocate dirty buf list");
	dirtylistsize = num_bufs;
  }

/* FIXME: when the buffer is for some FAT sector,
 * it should be mirrored (written several times)
 * when flushed to disk... NOW!
 */

#if 0
  for (bp = &buf[0], ndirty = 0; bp < &buf[num_bufs]; bp++)
	if (bp->b_dirt == DIRTY && bp->b_dev == dev) dirty[ndirty++] = bp;
#else
  for (bp = &buf[0], ndirty = 0; bp < &buf[num_bufs]; bp++)
	if (bp->b_dirt == DIRTY)
		dirty[ndirty++] = bp;
#endif
  rw_scattered(dev, dirty, ndirty, WRITING);
}

/*===========================================================================*
 *				invalidate				     *
 *===========================================================================*/
PRIVATE void invalidate(
  dev_t device			/* device whose blocks are to be purged */
)
{
/* Remove all the blocks belonging to some device from the cache. */

  register struct buf *bp;

  for (bp = &buf[0]; bp < &buf[num_bufs]; bp++)
	if (bp->b_dev == device) {
		bp->b_blocknr = NO_BLOCK;
		bp->b_dev = NO_DEV;
	}

#ifdef USE_VMCACHE
  vm_forgetblocks();
#endif
}

/*===========================================================================*
 *				get_block				     *
 *===========================================================================*/
PUBLIC struct buf *get_block(
  register dev_t req_dev,	/* on which device is the block? */
  register block_t block,	/* which block is wanted? */
  enum get_blk_arg_e only_search /* if NO_READ or PREFETCH, don't read */
)
{
/* REPHRASE-ME */
/* Check to see if the requested block is in the block cache.  If so, return
 * a pointer to it.  If not, evict some other block and fetch it (unless
 * 'only_search' is 1).  All the blocks in the cache that are not in use
 * are linked together in a chain, with 'front' pointing to the least recently
 * used block and 'rear' to the most recently used block.  If 'only_search' is
 * NO_READ, the block being requested will be overwritten in its entirety, so it is
 * only necessary to see if it is in the cache; if it is not, any free buffer
 * will do.  It is not necessary to actually read the block in from disk.
 * If 'only_search' is PREFETCH, the block need not be read from the disk,
 * and the device is not to be marked on the block, so callers can tell if
 * the block returned is valid.
 * In addition to the LRU chain, there is also a hash chain to link together
 * blocks, for faster lookups.
 *
 * Warning: this code is not reentrant (use static local variables, without mutex)
 */

  int b;
  static struct buf *bp;
#ifdef USE_VMCACHE
  u64_t yieldid = VM_BLOCKID_NONE, getid = make64(dev, block);
  int vmcache = 0;

  assert(buf_hash);
  assert(buf);
  assert(num_bufs > 0);

  if(vmcache_avail < 0) {
	/* Test once for the availability of the vm yield block feature. */
	if(vm_forgetblock(VM_BLOCKID_NONE) == ENOSYS) {
		vmcache_avail = 0;
	} else {
		vmcache_avail = 1;
	}
  }

  /* use vmcache if it's available, and allowed, and we're not doing
   * i/o on a ram disk device.
   */
  if(vmcache_avail && may_use_vmcache && major(dev) != MEMORY_MAJOR)
	vmcache = 1;
#endif

  /* this cache only deals with a single dev */
  assert(req_dev == dev || (req_dev == NO_DEV && block == NO_BLOCK) );
  assert(block_size > 0);

DBGprintf(("FATfs: get_block %ld ...", block));

  /* Search the hash chain for (dev, block). */
  assert(dev != NO_DEV);
  b = BUFHASH(block);
  LIST_FOREACH(bp, &buf_hash[b], b_hash) {
	if (bp->b_blocknr == block && bp->b_dev == dev) {
		/* Block needed has been found. */
		if (bp->b_count == 0) rm_lru(bp);
		bp->b_count++;	/* record that block is in use */
		assert(bp->b_bytes == block_size);
		assert(bp->b_dev == dev);
		assert(bp->b_dev != NO_DEV);
		assert(bp->dp);
DBGprintf(("found in cache! returns %d @ %p\n", bp-buf, bp->dp));
		return(bp);
	}
  }

  /* Desired block is not on available chain.  Take oldest block. */
  if ((bp = TAILQ_FIRST(&lru)) == NULL)
	panic("all buffers in use: %d", num_bufs);

DBGprintf(("not in cache!\nOldest was %d ...", bp-buf));

  if(bp->b_bytes < block_size) {
	assert(!bp->dp);
	assert(bp->b_bytes == 0);
/* FIXME: allocate buffers using pages, to avoid wasting memory */
	if(!(bp->dp = alloc_contig( (size_t) block_size, 0, NULL))) {
		printf("MFS: couldn't allocate a new block.\n");
		bp = TAILQ_FIRST(&lru);
		while(bp && bp->b_bytes < block_size)
			bp = TAILQ_NEXT(bp, b_next);
		if(!bp) {
			panic("no buffer available");
		}
DBGprintf(("cannot alloc, so will try %p ...", bp));
	} else {
DBGprintf(("allocated...", bp->dp));
		bp->b_bytes = block_size;
	}
  }

  assert(bp);
  assert(bp->dp);
  assert(bp->b_bytes == block_size);
  assert(bp->b_count == 0);

  rm_lru(bp);
DBGprintf(("@ %.8p...", bp->dp));

  /* Remove the block that was just taken from its hash chain. */
/*
  b = BUFHASH(bp->b_blocknr);
 */
  LIST_REMOVE(bp, b_hash);

  /* If the block taken is dirty, make it clean by writing it to the disk.
   * Avoid hysteresis by flushing all other dirty blocks for the same device.
   */
#if 0
  if (bp->b_dev != NO_DEV) {
#else
  if (!IS_FREE_BLOCK(bp)) {
#endif
	if (bp->b_dirt == DIRTY) flushall(bp->b_dev);

#ifdef USE_VMCACHE
	/* Are we throwing out a block that contained something?
	 * Give it to VM for the second-layer cache.
	 */
	yieldid = make64(bp->b_dev, bp->b_blocknr);
	assert(bp->b_bytes == block_size);
	bp->b_blocknr = NO_BLOCK;
	bp->b_dev = NO_DEV;
#endif
DBGprintf(("old flushed..."));
  }

  /* Fill in block's parameters and add it to the hash chain where it goes. */
  bp->b_dev = dev;		/* fill in device number */
  bp->b_blocknr = block;	/* fill in block number */
  bp->b_count++;		/* record that block is being used */
  b = BUFHASH(bp->b_blocknr);
  LIST_INSERT_HEAD(&buf_hash[b], bp, b_hash);

#ifdef USE_VMCACHE
/* FIXME: check... */
  if(dev == NO_DEV) {
	if(vmcache && cmp64(yieldid, VM_BLOCKID_NONE) != 0) {
		vm_yield_block_get_block(yieldid, VM_BLOCKID_NONE,
			bp->dp, block_size);
	}
DBGprintf(("and found in L2 (VM) cache! paged in @ %.8p!\n", bp->dp));
	return(bp);	/* If the caller wanted a NO_DEV block, work is done. */
  }
#endif

  /* Go get the requested block unless searching or prefetching. */
  if(only_search == PREFETCH || only_search == NORMAL) {
	/* Block is not found in our cache, but we do want it
	 * if it's in the vm cache.
	 */
#ifdef USE_VMCACHE
	if(vmcache) {
		/* If we can satisfy the PREFETCH or NORMAL request 
		 * from the vm cache, work is done.
		 */
		if(vm_yield_block_get_block(yieldid, getid,
			bp->dp, block_size) == OK) {
			return bp;
		}
	}
#endif
  }

#if 0
  if(only_search == PREFETCH) {
	/* PREFETCH: don't do i/o. */
	bp->b_dev = NO_DEV;
  } else if (only_search == NORMAL) {
	rw_block(bp, READING);
DBGprintf(("read..."));
  } else if(only_search == NO_READ) {
#ifdef USE_VMCACHE
	/* we want this block, but its contents
	 * will be overwritten. VM has to forget
	 * about it.
	 */
	if(vmcache) {
		vm_forgetblock(getid);
	}
#endif
  } else
	panic("unexpected only_search value: %d", only_search);
#else
  switch (only_search) {
  case PREFETCH:
	/* PREFETCH: don't do i/o. */
	bp->b_dev = NO_DEV;
/* FIXME: 
	bp->b_blocknr = NO_BLOCK;
 */
	bp->b_dirt = NOTREAD;
	break;
  case NORMAL:
	rw_block(bp, READING);
DBGprintf(("read..."));
	break;
  case NO_READ:
#ifdef USE_VMCACHE
	/* we want this block, but its contents
	 * will be overwritten. VM has to forget
	 * about it.
	 */
	if(vmcache) {
		vm_forgetblock(getid);
	}
#endif
	break;
  default:
	panic("unexpected only_search value: %d", only_search);
  }
#endif
  assert(bp->dp);

DBGprintf(("and returned!\n"));
  return(bp);			/* return the newly acquired block */
}

/*===========================================================================*
 *				rm_lru					     *
 *===========================================================================*/
PRIVATE void rm_lru(struct buf *bp)
{
/* Remove a block from its LRU chain. */

  bufs_in_use++;
  TAILQ_REMOVE(&lru, bp, b_next);
}

/*===========================================================================*
 *				zero_block				     *
 *===========================================================================*/
PUBLIC void zero_block(struct buf *bp)	/* pointer to buffer to zero */
{
/* Zero a block. */
  assert(bp->b_bytes > 0);
  assert(bp->dp);
  memset(bp->dp, 0, (size_t) bp->b_bytes);
  bp->b_dirt = DIRTY;
}

/*===========================================================================*
 *				put_block				     *
 *===========================================================================*/
PUBLIC void put_block(struct buf *bp) /* buffer to be released */
{
/* Return a block to the list of available blocks. */

  if (bp == NULL) return;	/* it is easier to check here than in caller */

  bp->b_count--;		/* there is one use fewer now */
  if (bp->b_count != 0) return;	/* block is still in use */

  bufs_in_use--;		/* one fewer block buffers in use */

  /* Put this block back on the LRU chain.
   * Block probably will be needed quickly.  Put it on rear of chain.
   * It will not be evicted from the cache for a long time.
   */
  TAILQ_INSERT_TAIL(&lru, bp, b_next);
}

/*===========================================================================*
 *				rw_block				     *
 *===========================================================================*/
PRIVATE void rw_block(
  register struct buf *bp,	/* buffer pointer */
  int rw_flag)			/* READING or WRITING */
{
/* Read or write one disk block.
<TRASHME>
 This is the only routine in which actual disk
 * I/O is invoked. If an error occurs, a message is printed here, but the error
 * is not reported to the caller.  If the error occurred while purging a block
 * from the cache, it is not clear what the caller could do about it anyway.
 */
  int r, op, op_failed;
  u64_t pos;
  dev_t dev;

  op_failed = 0;

  if ( (dev = bp->b_dev) != NO_DEV) {
	pos = mul64u(bp->b_blocknr, block_size);
/* WORK NEEDED ? */
	op = (rw_flag == READING ? DEV_READ_S : DEV_WRITE_S);
	r = seqblock_dev_io(op, bp->dp, pos, block_size);
	if (r < 0) {
		printf("FATfs: I/O error on device %d/%d, block %lu\n",
			major(dev), minor(dev), bp->b_blocknr);
		op_failed = 1;
	} else if( (unsigned) r != block_size) {
		r = END_OF_FILE;
		op_failed = 1;
	}

	if (op_failed) {
/* FIXME: should we clear bp->b_size = 0; ??? or rephrase? */
		bp->b_dev = NO_DEV;	/* invalidate block */

		/* Report read errors to interested parties. */
/* WORK NEEDED */
		if (rw_flag == READING) /* FIXME!!! rdwt_err = */ r;
	}
  }

  bp->b_dirt = CLEAN;
}

/*===========================================================================*
 *				rw_scattered				     *
 *===========================================================================*/
PUBLIC void rw_scattered(
  dev_t dev,			/* major-minor device number */
  struct buf **bufq,		/* pointer to array of buffers */
  int bufqsize,			/* number of buffers */
  int rw_flag			/* READING or WRITING */
)
{
/* Read or write multiple, scattered, buffers from or to a device.
 *
 * Warning: this code is not reentrant (use static local variables, without mutex)
 */
  register struct buf *bp;
  register int i;
  int j, r, gap;
  iovec_t *iop;
  u64_t pos;
  static iovec_t *iovec = NULL;

  STATICINIT(iovec, NR_IOREQS);	/* allocated once, made contiguous */

  /* (Shell) sort buffers on b_blocknr. */
#if 0
  gap = 1;
  do
	gap = 3 * gap + 1;
  while (gap <= bufqsize);
#else
  for (gap = 3+1; gap <= bufqsize; gap = 3 * gap + 1) ;
#endif
  while (gap != 1) {
	gap /= 3;
	for (j = gap; j < bufqsize; j++) {
		for (i = j - gap;
		     i >= 0 && bufq[i]->b_blocknr > bufq[i + gap]->b_blocknr;
		     i -= gap) {
			bp = bufq[i];
			bufq[i] = bufq[i + gap];
			bufq[i + gap] = bp;
		}
	}
  }

  /* Set up I/O vector and do I/O.  The result of dev_io is OK if everything
   * went fine, otherwise the error code for the first failed transfer.
   */  
  while (bufqsize > 0) {
	for (j = 0, iop = iovec; j < NR_IOREQS && j < bufqsize; j++, iop++) {
		bp = bufq[j];
		if (bp->b_blocknr != (block_t) bufq[0]->b_blocknr + j) break;
		iop->iov_addr = (vir_bytes) bp->dp;
		iop->iov_size = (vir_bytes) block_size;
	}
/* WORK NEEDED ? */
	pos = mul64u(bufq[0]->b_blocknr, block_size);
	r = scattered_dev_io(rw_flag == WRITING ? DEV_SCATTER_S : DEV_GATHER_S,
		iovec, pos, j);

	/* Harvest the results.  Dev_io reports the first error it may have
	 * encountered, but we only care if it's the first block that failed.
	 */
	for (i = 0, iop = iovec; i < j; i++, iop++) {
		bp = bufq[i];
		if (iop->iov_size != 0) {
			/* Transfer failed. An error? Do we care? */
			if (r != OK && i == 0) {
				printf(
				"FATfs: I/O error, device %d/%d, block %lu\n",
					major(dev), minor(dev), bp->b_blocknr);
				bp->b_dev = NO_DEV;	/* invalidate block */
#ifdef USE_VMCACHE
  				vm_forgetblocks();
#endif
			}
			break;
		}
		if (rw_flag == READING) {
			bp->b_dev = dev;	/* validate block */
			put_block(bp);
		} else {
			bp->b_dirt = CLEAN;
		}
	}
	bufq += i;
	bufqsize -= i;
	if (rw_flag == READING) {
		/* Don't bother reading more than the device is willing to
		 * give at this time.  Don't forget to release those extras.
		 */
		while (bufqsize > 0) {
			put_block(*bufq++);
			bufqsize--;
		}
	}
	if (rw_flag == WRITING && i == 0) {
		/* We're not making progress, this means we might keep
		 * looping. Buffers remain dirty if un-written. Buffers are
		 * lost if invalidate()d or LRU-removed while dirty. This
		 * is better than keeping unwritable blocks around forever.
		 */
		break;
	}
  }
}
