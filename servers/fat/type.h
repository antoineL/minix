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


/*
 *  This is the format of the contents of the deTime
 *  field in the direntry structure.
 */
struct DOStime {
	unsigned /*short*/
			dt_2seconds:5,	/* seconds divided by 2		*/
			dt_minutes:6,	/* minutes			*/
			dt_hours:5;	/* hours			*/
};

/*
 *  This is the format of the contents of the deDate
 *  field in the direntry structure.
 */
struct DOSdate {
	unsigned /*short*/
			dd_day:5,	/* day of month			*/
			dd_month:4,	/* month			*/
			dd_year:7;	/* years since 1980		*/
};

/*
 * This is the format of the contents of the deTime field in the direntry
 * structure.
 * We don't use bitfields because we don't know how compilers for
 * arbitrary machines will lay them out.
 */
#define DT_2SECONDS_MASK	0x001F	/* seconds divided by 2 */
#define DT_2SECONDS_SHIFT	0
#define DT_MINUTES_MASK		0x07E0	/* minutes */
#define DT_MINUTES_SHIFT	5
#define DT_HOURS_MASK		0xF800	/* hours */
#define DT_HOURS_SHIFT		11

/*
 * This is the format of the contents of the deDate field in the direntry
 * structure.
 */
#define DD_DAY_MASK		0x001F	/* day of month */
#define DD_DAY_SHIFT		0
#define DD_MONTH_MASK		0x01E0	/* month */
#define DD_MONTH_SHIFT		5
#define DD_YEAR_MASK		0xFE00	/* year - 1980 */
#define DD_YEAR_SHIFT		9

 
#endif
