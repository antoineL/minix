/* Types describing the on-disk structures used by the FAT file system.
 *
 * The basis for this file is based on the PCFS package targetting
 * 386BSD 0.1, published in comp.unix.bsd on October 1992 by
 * Paul Popelka; his work is to be rewarded.
 *
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

/* Format of a superblock. This is the first sector on a FAT
 * file system.
 * DOS 1.x FAT floppies did not use this format (the parameters
 * were rather hardcoded in the system, indexed from FAT[0].)
 */
struct fat_superblock {
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

/* Here ended the uncontroversive. Now begins the fun...
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
  uint8_t bpbHiddenSecs[4];	/* number of hidden sectors */

/* DOS 3.31 added the possibility to have more than 64K sectors... */
  uint8_t bpbHugeSectors[4];	/* (large) nr of sect. if(bpbSectors==0) */

/* Here lies the "extended BPB" structures for FAT12/16 files sytems
 * created after 1990 (DOS 4).
 * However FAT32 file systems need more informationss and have
 * overwritten that extended BPB: as a result it can be placed
 * at two different positions...
 */
#define offsetEXTBPB16	offsetof(struct fat_superblock, bpbBigFATsecs)

  uint8_t bpbBigFATsecs[4];	/* number of sectors per FAT32 */
  uint8_t bpbExtFlags[2];	/* some FAT32 flags: */
#define	FATONEACTIVE	0x80		/* only 1 FAT is active */
#define	FATNUM		0x0f		/* number of active FAT */
  uint8_t bpbFSVers[2];		/* filesystem version: */
#define	FSVERS		0		/* only version 0 exists */
  uint8_t bpbRootClust[4];	/* start cluster for root directory */
  uint8_t bpbFSInfo[2];		/* filesystem info FSInfo sectors */
  uint8_t bpbBackup[2];		/* backup boot sector (shall be 6) */
  uint8_t bpbReserved[12];	/* reserved for future expansion (?) */
#define offsetEXTBPB32	offsetof(struct fat_superblock, bsExtBPB32)
  uint8_t bsExtBPB32[26];	/* BPB extension (assuming FAT32) */

  uint8_t bsBootCode[420];	/* pad so structure is 512b */

/* Near the end of the first sector, there is also a 16-bit magic
 * number. The position does not depend on the size of a sector.
 * (How this was supposed to work with the old 128- and
 *  256-byte-sectored floppies is now forgotten: these
 *  formats have been formally 'obsoleted'.)
 */
  char bsBootSectSig[2];
#define	BOOTSIG		"\x55\xaa"
};

/* This is the detail of the extended BPB structure,
 * which can be placed at two different positions depending
 * on the FAT kind.
 */
struct extbpb {
  uint8_t exDriveNumber;	/* BIOS drive number (fd0=0x00, hd0=0x80) */
  uint8_t exReserved1;		/* reserved: */
#define EXTBPB_NEEDFSCK	0x01		/* as used by WinNT (MS KB140418) */
#define EXTBPB_NEEDBBCK	0x02		/* "need badblock check" */
  uint8_t exBootSignature;	/* ext. boot signature (0x29) */
#define	EXBOOTSIG	0x29
#define	EXBOOTSIG_ALT	0x28		/* said to be also used? */
  uint8_t exVolumeID[4];	/* volume ID number */
  char exVolumeLabel[11];	/* volume label */
  char exFileSysType[8];	/* fs type (FAT, FAT12, FAT16, FAT32) */
};

/* FAT32 FSInfo block. It currently holds a few indicative
 * counters, which helps performance-wise because of the
 * potential big size of FAT32 structures.
 */
struct fsinfo {
  char fsiSig1[4];
#define FSI_SIG1	"RRaA"
  uint8_t fsiFill1[480];
  char fsiSig2[4];
#define FSI_SIG2	"rrAa"
  uint8_t fsinFree[4];		/* count of free clusters */
  uint8_t fsinNextFree[4];	/* index for next free cluster */
  uint8_t fsiFill2[14];
  char fsiSig3[2];
#define FSI_SIG3	BOOTSIG
  uint8_t fsiFill3[510];
  char fsiSig4[2];
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
struct direntry {
  unsigned char deName[8];	/* filename, space filled */
#define	SLOT_EMPTY	0x00		/* slot has never been used */
#define	SLOT_DELETED	0xe5		/* file in this slot deleted */
#define	SLOT_E5		0x05		/* the real value is '\xe5' */
  unsigned char deExtension[3];	/* extension, space filled */
  uint8_t deAttributes;		/* file attributes */
#define	ATTR_NORMAL	0x00		/* normal file			*/
#define	ATTR_READONLY	0x01		/* file is readonly		*/
#define	ATTR_HIDDEN	0x02		/* file is hidden		*/
#define	ATTR_SYSTEM	0x04		/* file is a system file	*/
#define	ATTR_VOLUME	0x08		/* entry is a volume label	*/
#define	ATTR_DIRECTORY	0x10		/* entry is a directory name	*/
#define	ATTR_ARCHIVE	0x20		/* file is new or modified	*/
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

/* Format of a long filename (LFN) directory entry.
 * Introduced by Windows 95's "VFAT.vxd" driver, so often named
 * with one of these two labels.
 *
 * The LFN entries precede the matching struct direntry
 * and are stored in reverse order, ie the tail of the filename
 * (indicated by the LFN_LAST bit set) occurs first; the
 * checksum (which is function of the "short" filename)
 * assures that the link has not been severed.
 */
struct lfnentry {
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
#define	CLUST_ROOT	0	/* cluster 0 also means the root dir */
	#define	PCFSFREE	CLUST_FREE

#define	CLUST_FIRST	2	/* first legal cluster number */
	#define	CLUST_RSRVS	0xfff0	/* start of reserved cluster range	*/
	#define	CLUST_RSRVE	0xfff6	/* end of reserved cluster range	*/
#define	CLUST_MAXFAT12	0x00000ff6	/* max cluster number for FAT12 */
#define	CLUST_MAXFAT16	0x0000fff6	/* max cluster number for FAT16 */

#define	CLUST_BAD	0xfffffff7	/* a cluster with a defect */
#define	CLUST_EOFS	0xfffffff8	/* start of eof cluster range */
#define	CLUST_EOFE	0xffffffff	/* end of eof cluster range */

#define	FAT12_MASK	0x00000fff	/* mask for 12 bit cluster numbers */
#define	FAT16_MASK	0x0000ffff	/* mask for 16 bit cluster numbers */
#define	FAT32_MASK	0x0fffffff	/* mask for 32 bit cluster numbers */

/*
 *  Return true if filesystem uses 12 bit fats.
 *  Microsoft Programmer's Reference says if the
 *  maximum cluster number in a filesystem is greater
 *  than 4086 then we've got a 16 bit fat filesystem.
 */
#define	FAT12(pmp)	(pmp->pm_maxcluster <= CLUST_MAXFAT12)
#define	FAT16(pmp)	( (unsigned)(pmp->pm_maxcluster-CLUST_MAXFAT12-1) \
				<= (CLUST_MAXFAT16-CLUST_MAXFAT12-1) )

#define	pcfsFAT12(pmp)	(pmp->pm_maxcluster <= 4086)
#define	pcfsFAT16(pmp)	(pmp->pm_maxcluster >  4086)

/*
 * MSDOSFS:
 * Return true if filesystem uses 12 bit fats. Microsoft Programmer's
 * Reference says if the maximum cluster number in a filesystem is greater
 * than 4078 ((CLUST_RSRVS - CLUST_FIRST) & FAT12_MASK) then we've got a
 * 16 bit fat filesystem. While mounting, the result of this test is stored
 * in pm_fatentrysize.
 * GEMDOS-flavour (atari):
 * If the filesystem is on floppy we've got a 12 bit fat filesystem, otherwise
 * 16 bit. We check the d_type field in the disklabel struct while mounting
 * and store the result in the pm_fatentrysize. Note that this kind of
 * detection gets flakey when mounting a vnd-device.
 */
#define	FAT12(pmp)	(pmp->pm_fatmask == FAT12_MASK)
#define	FAT16(pmp)	(pmp->pm_fatmask == FAT16_MASK)
#define	FAT32(pmp)	(pmp->pm_fatmask == FAT32_MASK)

/* REVISEME */
#define	PCFSEOF(cn)	(((cn) & 0xfff8) == 0xfff8)

/*
 * M$ in it's unlimited wisdom desided that EOF mark is anything
 * between 0xfffffff8 and 0xffffffff (masked by appropriate fatmask,
 * of course).
 * Note that cn is supposed to be already adjusted accordingly to FAT type.
 */
#define	MSDOSFSEOF(cn, fatmask)	\
	(((cn) & CLUST_EOFS) == (CLUST_EOFS & (fatmask)))

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

#endif
