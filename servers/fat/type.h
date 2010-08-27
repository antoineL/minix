/* Various types used by the FAT file system.
 * Some parts have their own, separated headers
 * which hold the type descriptions:
 *   fat.h	description of the ondisk structures
 *   super.h	file system fundamental values
 *   inode.h	inode structure (also hold dir.entry)
 *
 * Auteur: Antoine Leca, aout 2010.
 * Updated:
 */

#ifndef FAT_TYPE_H_
#define FAT_TYPE_H_

#include "super.h"

/* Buffer (block) cache.
 * To acquire a block, a routine calls get_block(), telling which block it
 *  wants. The block is then regarded as "in use" and has its 'b_count'
 * field incremented. All the blocks that are not in use are chained
 * together in an LRU list, implemented as a tail-queue, with '_FIRST'
 * pointing to the least recently used block, and '_TAIL' to the most
 * recently used block. Usage for LRU is measured by the time the
 * put_block() is done. If a block is modified, the modifying routine must
 * set b_dirt to DIRTY, so the block will eventually be rewritten to the disk.
 */

#include <sys/queue.h>

/* CHECKME!!! better use b_bytes==0 ? make some macros */
/* A block is free if b_dev == NO_DEV. */

struct buf {
  /* Data portion of the buffer. Uninterpreted by cache. */
  union blkdata_u *bp;

  /* Header portion of the buffer. */
  TAILQ_ENTRY(buf) b_next;	/* used to link all free bufs in a chain */
  LIST_ENTRY( buf) b_hash;	/* used to link bufs on hash chains */
  unsigned int b_bytes;         /* Number of bytes allocated in bp */
  block_t b_blocknr;            /* block number of its (minor) device */
  dev_t b_dev;                  /* major | minor device where block resides */
  char b_dirt;                  /* CLEAN or DIRTY */
#define CLEAN              0	/* disk and memory copies identical */
#define DIRTY              1	/* disk and memory copies differ */
  char b_count;                 /* number of users of this buffer */
};

/* CHECKME... */
#define BUFHASH(b) ((b) % nr_bufs)

 
#endif
