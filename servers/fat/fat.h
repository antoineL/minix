/* Types describing the on-disk structures used by the FAT file system.
 *
 * The basis for this file is the PCFS package targetting
 * 386BSD 0.1, published in comp.unix.bsd on October 1992
 * by Paul Popelka; his work is to be rewarded.
 * Auteur: Antoine Leca, aout 2010.
 */

#ifndef FAT_H_
#define FAT_H_

#include <stddef.h>
#include <stdint.h>

/* The following structures represent how the FAT structures
 * look on disk; they are just character arrays of the
 * appropriate lengths.  This is because the compiler would
 * force shorts and longs to align on incorrect boundaries.
 * Everything on-disk is stored in little-endian form.
 */ 

/* This is the detail of the extended BPB structure,
 * which can be placed at two different positions depending
 * on the FAT kind. More explanations below.
 */
struct fat_extbpb {
  uint8_t exDriveNumber;	/* BIOS drive number (fd0=0x00, hd0=0x80) */
  uint8_t exNeedCheck;		/* check fs on mount; used by NT, KB140418 */
#define EXTBPB_NEEDFSCK	0x01		/* need file system check (chkdsk) */
#define EXTBPB_NEEDBBCK	0x02		/* need badblock check */
  uint8_t exBootSignature;	/* extended boot signature (0x29) */
#define	EXBOOTSIG	0x29
#define	EXBOOTSIG_ALT	0x28		/* said to be also used? */
  uint8_t exVolumeID[4];	/* volume ID number */
  char exVolumeLabel[11];	/* volume label */
  char exFileSysType[8];	/* fs type: */
#define FAT_FSTYPE	"FAT     "	/* FAT, but without size indication */
#define FAT12_FSTYPE	"FAT12   "	/* asserts this is FAT12 */
#define FAT16_FSTYPE	"FAT16   "	/* asserts this is FAT16 */
#define FAT32_FSTYPE	"FAT32   "	/* asserts this is FAT32 */
};

/* Format of a superblock, also known as boot sector.
 * This is the first sector on a FAT file system.
 * DOS 1.x FAT floppies did not use this format (the parameters
 * were rather hardcoded in the system, indexed from FAT[0].)
 */
struct fat_bootsector {
  uint8_t bsJump[3];		/* jump inst; E9xxxx or EBxx90 */
  char bsOemName[8];		/* OEM name and version */

/* Here starts the BPB (BIOS Parameter Block), an important
 * structure in MS-DOS.
 */
  uint8_t bpbBytesPerSec[2];	/* bytes per sector */
  uint8_t bpbSecPerClust;	/* sectors per cluster */
  uint8_t bpbResSectors[2];	/* number of reserved sectors */
  uint8_t bpbFATs;		/* number of FATs */
  uint8_t bpbRootDirEnts[2];	/* number of root directory entries (!FAT32) */
  uint8_t bpbSectors[2];	/* (small) total number of sectors */
  uint8_t bpbMedia;		/* media descriptor */
  uint8_t bpbFATsecs[2];	/* number of sectors per FAT12/16 */
/* Here ends the commonest part, as defined by MS-DOS 2.0 and
 * all the subsequent versions. Now begins the extensions.
 *
 * First some uncontroversial members, introduced by DOS 3:
 */
  uint8_t bpbSecPerTrack[2];	/* sectors per track */
  uint8_t bpbHeads[2];		/* number of heads */

/* Here ended the uncontroversial. Now begins the fun...
 *
 * DOS 3 considers only a 16-bit quantity for the following member;
 * later version turned it into a 32-bit quantity; unfortunately
 * there are no good ways to detect the version which formatted
 * a given file system: the two most often used are to interpret
 * the bsOemName above, or to assume the booting pointed to by
 * the bsJump instruction above lies jst after the BPB, hence
 * indicating indirectly where it ends; neither way is foolproof
 * (which is why MS engineers added the "extended part" later.)
 */
  uint8_t bpbHiddenSecs[4];	/* number of sectors before on the disk */

/* DOS 3.31 added the possibility to have more than 64K sectors... */
  uint8_t bpbHugeSectors[4];	/* (large) nr of sect. if(bpbSectors==0) */

/* Here lies the "extended BPB" structures for FAT12/16 files sytems
 * created after 1990 (DOS 4).
 * However FAT32 file systems need more informationss and have
 * overwritten that extended BPB: as a result the extension
 * can be placed at two different positions...
 */
  union {
   struct fat_extbpb ExtBPB;	/* BPB extension (assuming FAT12/16) */
   struct {
    uint8_t bpbBigFATsecs[4];	/* number of sectors per FAT32 */
    uint8_t bpbExtFlags[2];	/* some FAT32 flags: */
#define	FATONEACTIVE	0x80		/* only 1 FAT is active */
#define	FATNUM		0x0f		/* mask for number of active FAT */
    uint8_t bpbFSVers[2];	/* filesystem version: */
#define	FSVERS		0		/* only version 0 we known about */
    uint8_t bpbRootClust[4];	/* start cluster for root directory */
    uint8_t bpbFSInfo[2];	/* filesystem info FSInfo sectors */
    uint8_t bpbBackup[2];	/* backup boot sectors (shall be 6) */
    uint8_t bpbReserved[12];	/* reserved for future expansion (?) */
/* End of BPB as defined for FAT32. Here 
begins exFAT
 * datas (not documented here). Also here would stands
 * the extended BPB in case of a FAT32 file system.
 */
    struct fat_extbpb ExtBPB32;	/* BPB extension (assuming FAT32) */
   } f32bpb;
  } u;

  uint8_t bsBootCode[420];	/* pad so structure is 512 bytes long */

/* Near the end of the first sector, there is also a 16-bit magic
 * number. The position does not depend on the size of a sector.
 * (How this was supposed to work with the old 128- and
 *  256-byte-sectored floppies is now forgotten: these
 *  formats have been formally 'obsoleted'.)
 */
  char bsBootSectSig[2];
#define	BOOTSIG		"\x55\xaa"
};

/* FAT32 FSInfo block. It currently holds a few indicative
 * counters, which helps performance-wise because of the
 * potential big size of FAT32 structures.
 */
struct fat_fsinfo {
  char fsiSig1[4];		/* magic value at start of sector */
#define FSI_SIG1	"RRaA"
  uint8_t fsiFill1[480];	/* usually zeroes */
  char fsiSig2[4];		/* second magic just before the datas */
#define FSI_SIG2	"rrAa"
  uint8_t fsinFree[4];		/* count of free clusters */
  uint8_t fsinNextFree[4];	/* index for next free cluster */
  uint8_t fsiFill2[14];
  char fsiSig3[2];		/* another magic just before 512 */ 
#define FSI_SIG3	BOOTSIG
  uint8_t fsiCodeExt[510];	/* booting code for DOS/Windows9x */
  char fsiSig4[2];		/* and another magic just before 1024 */
#define FSI_SIG4	BOOTSIG
};

/* Format of a FAT directory entry. This also plays the role
 * of the inode. Missing are the members for permissions,
 * uid/gid, or count of hard links, since there are no such
 * things on MS-DOS; the only data block addressed is the
 * first one (indexed by its cluster number); the other data
 * clusters will be found via the FAT itself, which is a
 * linked list of "next cluster numbers".
 */
struct fat_direntry {
  unsigned char deName[8];	/* filename, space filled */
#define	SLOT_EMPTY	0x00		/* slot has never been used */
#define	SLOT_DELETED	0xe5		/* file in this slot deleted */
#define	SLOT_E5		0x05		/* the real value is '\xe5' */
#define	SLOT_DOT	0x2E		/* '.' and '..' at start of subdir */
#define	NAME_DOT	".          "	/* '.' entry at start of subdir */
#define	NAME_DOT_DOT	"..         "	/* '..' entry, 2nd in subdir */
  unsigned char deExtension[3];	/* extension, space filled */
#define	EXT_NONE	"   "		/* no extension, thus kill the '.' */
  uint8_t deAttributes;		/* file attributes */
#define	ATTR_NORMAL	0x00		/* normal file */
#define	ATTR_READONLY	0x01		/* file is readonly */
#define	ATTR_HIDDEN	0x02		/* file is hidden */
#define	ATTR_SYSTEM	0x04		/* file is a system file */
#define	ATTR_VOLUME	0x08		/* entry is a volume label */
#define	ATTR_DIRECTORY	0x10		/* entry is a directory name */
#define	ATTR_ARCHIVE	0x20		/* file is new or modified */
  uint8_t deLCase;		/* tolower case changer (WinNT) */
#define	LCASE_NAME	0x08		/* name is really lowercase */
#define	LCASE_EXTENSION	0x10		/* extension is really lowercase */
  uint8_t deBHundredth;		/* hundredth of seconds in BTime */
  uint8_t deBTime[2];		/* "birth" (creation) time */
  uint8_t deBDate[2];		/* "birth" (creation) date */
  uint8_t deADate[2];		/* access date; note: no time */
  uint8_t deHighClust[2];	/* high bytes of cluster number (FAT32) */
	/* also used by OS/2 to store extended attribute access index */
  uint8_t deMTime[2];		/* last update time */
  uint8_t deMDate[2];		/* last update date */
  uint8_t deStartCluster[2];	/* starting cluster of file */
  uint8_t deFileSize[4];	/* size of file in bytes */
};
#define DIR_ENTRY_SIZE	(sizeof(struct fat_direntry))

/* Format of a long filename (LFN) directory entry.
 * Introduced by Windows 95's "VFAT.vxd" driver, so often named
 * with one of these two labels.
 *
 * The LFN entries precede the matching struct direntry
 * and are stored in reverse order, ie the tail of the filename
 * (indicated by the LFN_LAST bit set) occurs first; the
 * checksum (which is function of the "short" filename)
 * warrants that the link has not been severed.
 */
struct fat_lfnentry {
  uint8_t lfnOrd;		/* ordinal number of LFN entry */
#define	LFN_DELETED	0x80
#define	LFN_LAST	0x40
#define	LFN_ORD		0x3f
  uint8_t lfnPart1[5*2];	/* long filename, as Unicode chars */
  uint8_t lfnAttributes;	/* always ATTR_LFN */
#define	ATTR_LFN	0x0f
  uint8_t lfnReserved;
  uint8_t lfnChksum;		/* checksum of matching direntry */
  uint8_t lfnPart2[6*2];
  uint8_t lfnFakeCluster[2];	/* should be 0 */
  uint8_t lfnPart3[2*2];
};
#define	LFN_CHARS	13	/* Number of chars per lfnentry */


/* Some useful cluster numbers.
 */
#define	CLUST_FREE	0	/* cluster 0 means a free cluster */
#define	CLUST_NONE	0	/* cluster 0 also means no data allocated */
#define	CLUST_ROOT	0	/* cluster 0 also means the root dir */

#define	CLUST_FIRST	2	/* first legal cluster number */
	#define	CLUST_RSRVS	0xfff0	/* start of reserved cluster range	*OBS*	*/
	#define	CLUST_RSRVE	0xfff6	/* end of reserved cluster range	*OBS*	*/
#define	CLUST_MAXFAT12	0x00000ff6	/* max cluster number for FAT12 */
#define	CLUST_MAXFAT16	0x0000fff6	/* max cluster number for FAT16 */

#define	CLUST_BAD	0xfffffff7	/* a cluster with a defect */
#define	CLUST_EOFS	0xfffffff8	/* start of eof cluster range */
#define	CLUST_EOFE	0xffffffff	/* end of eof cluster range */

#define	FAT12_MASK	0x00000fff	/* mask for 12-bit cluster numbers */
#define	FAT16_MASK	0x0000ffff	/* mask for 16 bit cluster numbers */
#define	FAT32_MASK	0x0fffffff	/* mask for 32 bit cluster numbers */

/* Determine the kind of FATs used by a given file system.
 * Strictly a function of the maximum cluster number in the filesystem:
 *   if less or equal to CLUST_MAXFAT12 (4086), it is FAT12
 *   else, if less or equal to CLUST_MAXFAT16 (65526), it is FAT16
 *   else [if strictly higher than CLUST_MAXFAT16], it is FAT32
 *
 * Note that since clusters 0 and 1 do not exist, the
 * "total number of clusters" is one less than "maxcluster".
 * Please do the maths twice before logging any bug report. Thanks.
 */
#define	FS_IS_FAT12(maxClust)	(maxClust <= CLUST_MAXFAT12)
#define	FS_IS_FAT16(maxClust)  ( (unsigned long)(maxClust-CLUST_MAXFAT12-1) \
					<=(CLUST_MAXFAT16-CLUST_MAXFAT12-1) )
#define	FS_IS_FAT32(maxClust)	(maxClust > CLUST_MAXFAT16)

/* In the (endless?) list of quirks, it was decided that EOF mark would be
 * anything between CLUST_EOFS and CLUST_EOFE (0xFF8 and 0xFFF initially).
 *
 * And some OSes (like Linux) took this to the letter, and used various
 * values... just to find later that some mainstream OS were not paying
 * the due attention (!), so they reverted to CLUST_EOFE, which is the
 * correct value to write.
 * However while reading, we should deal with that too...
 */
#define	CLUSTMASK_EOF12	(CLUST_EOFS&FAT12_MASK)	/* mask for 12-bit eof mark*/
#define	CLUSTMASK_EOF16	(CLUST_EOFS&FAT16_MASK)	/* mask for 16-bit eof mark*/
#define	CLUSTMASK_EOF32	(CLUST_EOFS&FAT32_MASK)	/* mask for 32-bit eof mark*/

/* warning: the following line is not a proper macro */
#define	ATEOF(cn, eofmask)	(((cn) & eofmask) == eofmask)

/* As seen above, cluster numbers begins at 2; so the
 * first two entries of any FAT are not used; they
 * should normally be set to all-1s, with some quirks.
 *
 * The lower byte of the first entry, FAT[0],
 * should have the same value as (super).bpbMedia,
 * as it was the way used in DOS 1 to identify
 * the size of the floppies. Details are omitted
 * here in order to protect the innocents.
 *
 * The high bits of second entry, FAT[1], has been reused
 * in Windows 95 to store the flag about the state of the
 * file system while unmounting.
 */
#define FAT16_CLEAN	0x8000		/* do not need fsck at mounting */
#define FAT16_NOHARDERR	0x4000		/* do not need to search badblock */
#define FAT32_CLEAN	0x08000000	/* do not need fsck at mounting */
#define FAT32_NOHARDERR	0x04000000	/* do not need to search badblock */

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
