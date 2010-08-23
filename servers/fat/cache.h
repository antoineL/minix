/* Buffer (block) cache.  To acquire a block, a routine calls get_block(),
 * telling which block it wants.  The block is then regarded as "in use"
 * and has its 'b_count' field incremented.  All the blocks that are not
 * in use are chained together in an LRU list, with 'front' pointing
 * to the least recently used block, and 'rear' to the most recently used
 * block.  A reverse chain, using the field b_prev is also maintained.
 * Usage for LRU is measured by the time the put_block() is done.  The second
 * parameter to put_block() can violate the LRU order and put a block on the
 * front of the list, if it will probably not be needed soon.  If a block
 * is modified, the modifying routine must set b_dirt to DIRTY, so the block
 * will eventually be rewritten to the disk.
 *
 * Auteur: Antoine Leca, aout 2010.
 * Slavishly copied from ../mfs/buf.h (A. Tanenbaum)
 * Updated:
 */

#ifndef FAT_CACHE_H_
#define FAT_CACHE_H_

#if 0
#include <sys/dir.h>			/* need struct direct */
#include <dirent.h>
#endif

union fsdata_u {
    char b__data[SBLOCK_SIZE];		     /* ordinary user data */
#if 0
/* directory block */
    struct direct b__dir[NR_DIR_ENTRIES(_MAX_BLOCK_SIZE)];    
/* V1 indirect block */
    zone1_t b__v1_ind[V1_INDIRECTS];	     
/* V2 indirect block */
    zone_t  b__v2_ind[V2_INDIRECTS(_MAX_BLOCK_SIZE)];	     
/* V1 inode block */
    d1_inode b__v1_ino[V1_INODES_PER_BLOCK]; 
/* V2 inode block */
    d2_inode b__v2_ino[V2_INODES_PER_BLOCK(_MAX_BLOCK_SIZE)]; 
/* bit map block */
    bitchunk_t b__bitmap[FS_BITMAP_CHUNKS(_MAX_BLOCK_SIZE)];  
#endif
};

/* A block is free if b_dev == NO_DEV. */

struct buf {
  /* Data portion of the buffer. */
  union fsdata_u *bp;

  /* Header portion of the buffer. */
  struct buf *b_next;           /* used to link all free bufs in a chain */
  struct buf *b_prev;           /* used to link all free bufs the other way */
  struct buf *b_hash;           /* used to link bufs on hash chains */
  block_t b_blocknr;            /* block number of its (minor) device */
  dev_t b_dev;                  /* major | minor device where block resides */
  char b_dirt;                  /* CLEAN or DIRTY */
#define CLEAN              0	/* disk and memory copies identical */
#define DIRTY              1	/* disk and memory copies differ */
  char b_count;                 /* number of users of this buffer */
  unsigned int b_bytes;         /* Number of bytes allocated in bp */
};

/* These defs make it possible to use to bp->b_data instead of bp->b.b__data */
#define b_data   bp->b__data
#define b_dir    bp->b__dir
#define b_v1_ind bp->b__v1_ind
#define b_v2_ind bp->b__v2_ind
#define b_v1_ino bp->b__v1_ino
#define b_v2_ino bp->b__v2_ino
#define b_bitmap bp->b__bitmap

#define BUFHASH(b) ((b) % nr_bufs)

#define NORMAL	           0	/* forces get_block to do disk read */
#define NO_READ            1	/* prevents get_block from doing disk read */
#define PREFETCH           2	/* tells get_block not to read or mark dev */

/* When a block is released, the type of usage is passed to put_block(). */
#define WRITE_IMMED   0100 /* block should be written to disk now */
#define ONE_SHOT      0200 /* set if block not likely to be needed soon */

#define INODE_BLOCK        0				 /* inode block */
#define DIRECTORY_BLOCK    1				 /* directory block */
#define INDIRECT_BLOCK     2				 /* pointer block */
#define MAP_BLOCK          3				 /* bit map */
#define FULL_DATA_BLOCK    5		 	 	 /* data, fully used */
#define PARTIAL_DATA_BLOCK 6 				 /* data, partly used*/

#endif
