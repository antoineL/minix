/* This file contains file metadata retrieval and manipulation routines.
 *
 * The entry points into this file are:
 *   dos2unixtime	convert DOS-style date and time into time_t
 *   unix2dostime	convert time_t into DOS-style date and time
 *   conv_83toname	convert FAT-style entry name into string
 *   conv_nameto83	convert string into FAT-style entry
 *   lfn_chksum		compute the checksum of a FAT entry name for LFN
 *
 * Warning: this code is not reentrant (use static local variables, without mutex)
 *
 * Auteur: Antoine Leca, septembre 2010.
 *   The basis for this file is the PCFS package targetting 386BSD 0.1,
 *   published in comp.unix.bsd on October 1992 by Paul Popelka;
 *   his work is to be rewarded; see also the notice below.
 * Updated:
 */
/*
 * Written by Paul Popelka (paulp@uts.amdahl.com)
 *
 * You can do anything you want with this software,
 *   just don't say you wrote it,
 *   and don't remove this notice.
 *
 * This software is provided "as is".
 *
 * The author supplies this software to be publicly
 * redistributed on the understanding that the author
 * is not responsible for the correct functioning of
 * this software in any circumstances and is not liable
 * for any damages caused by this software.
 *
 * October 1992
 */

#define _POSIX_SOURCE 1
/* This code is NOT MINIX-specific!
 * Do not #define _MINIX
 */

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "fat.h"

#ifndef NAME_MAX
#define NAME_MAX	14
#endif

#ifndef	PRIVATE
#define PRIVATE	static		/* PRIVATE x limits the scope of x */
#endif
#ifndef	PUBLIC
#define PUBLIC			/* PUBLIC is the opposite of PRIVATE */
#endif

#ifndef CONVNAME_ENUM_
#define CONVNAME_ENUM_
enum convname_result_e {
  CONV_OK,
  CONV_HASLOWER,
  CONV_NAMETOOLONG,
  CONV_TRAILINGDOT,
  CONV_INVAL
};
#endif

/* Private read-only global variables: */
/* Days in each month in a regular year. */
PRIVATE int regyear[] = {
	31,	28,	31,	30,	31,	30,
	31,	31,	30,	31,	30,	31
};

/* Days in each month in a leap year. */
PRIVATE int leapyear[] = {
	31,	29,	31,	30,	31,	30,
	31,	31,	30,	31,	30,	31
};

/* The number of seconds between Jan 1st, 1970 and Jan 1st, 1980.
 * In that interval there were 8 regular years and 2 leap years.
 */
#define	SECONDSTO1980	(((8 * 365) + (2 * 366)) * (24 * 60 * 60))

/* No Private functions: */

/* warning: the following lines are not failsafe macros */
#define	get_le16(arr) ((u16_t)( (arr)[0] | ((arr)[1]<<8) ))
#define	get_le32(arr) ( get_le16(arr) | ((u32_t)get_le16((arr)+2)<<16) )

#ifndef DBGprintf
 #if NDEBUG
  #define DBGprintf(x)
 #else
 #include <stdio.h>
  #if DEBUG
   #define DBGprintf(x) printf x
  #else
   extern int verbose;
   #define DBGprintf(x) if(verbose)printf x
  #endif 
 #endif
#endif /*DBGprintf*/

/*===========================================================================*
 *				dos2unixtime				     *
 *===========================================================================*/
PUBLIC time_t dos2unixtime(uint8_t deDate[2],uint8_t deTime[2])
{
/* Convert from DOS idea of time to UNIX [a trademark of The Open Group.]
 * This will probably only be called from the REQ_STAT requests,
 * and so probably need not be too efficient.
 *
 * Variables used to remember parts of the last time conversion. 
 * Maybe we can avoid a full conversion.
 * Warning: this code is not reentrant (use static local variables, without mutex)
 */
  static unsigned lastdosdate=0;	/* cached DOS date */
  static unsigned long lastseconds=0;	/* cached corresponding value */
  unsigned dosdate, dostime;
  unsigned long seconds;
  long days;
  int yr, mo;
  int dosmonth;
  int * months;

  dosdate = deDate ? get_le16(deDate) : 0;
  if (dosdate == 0) return 0;	/* return the Epoch if uninitialized */

  dostime = deTime ? get_le16(deTime) : 0;
  seconds = ((dostime & DT_2SECONDS_MASK) >> DT_2SECONDS_SHIFT) * 2
	  + ((dostime & DT_MINUTES_MASK) >> DT_MINUTES_SHIFT) * 60
	  + ((dostime & DT_HOURS_MASK) >> DT_HOURS_SHIFT) * 3600;

/* If the year, month, and day from the last conversion
 * are the same then use the saved value.
 */
  if (lastdosdate != dosdate) {
	lastdosdate = dosdate;
		days = 0;
  /* FIXME: use formula... */
	for (yr = 0; yr < (dosdate & DD_YEAR_MASK) >> DD_YEAR_SHIFT; yr++) {
		days += yr & 0x03 ? 365 : 366;
	}
	months = yr & 0x03 ? regyear : leapyear;
	dosmonth = (dosdate & DD_MONTH_MASK) >> DD_MONTH_SHIFT;
	if (dosmonth == 0) {
	/* Prevent enter infinite loop. */
		DBGprintf(("dos2unixtime(): month value out of range (%d)\n",
			dosmonth));
		dosmonth = 1;
	}
	for (mo = 0; mo < dosmonth; mo++) {
		days += months[mo];
	}
	days += ((dosdate & DD_DAY_MASK) >> DD_DAY_SHIFT) - 1;
	lastseconds = (days * 24 * 60 * 60) + SECONDSTO1980;
  }

#if 0 /* timeval interface... */
  tvp->tv_sec = seconds + lastseconds + (tz.tz_minuteswest * 60)
		/* -+ daylight savings time correction */;
  tvp->tv_usec = 0;
#else
/* FIXME: daylight savings time correction */
  return seconds + lastseconds /* -+ daylight savings time correction */ ;
#endif
}

/*===========================================================================*
 *				unix2dostime				     *
 *===========================================================================*/
PUBLIC void unix2dostime(time_t t, uint8_t deDate[2],uint8_t deTime[2])
{
/* Convert the UNIX version of time to DOS, to be used in file timestamps.
 * The passed-in UNIX time_t is assumed to be in UTc.
 *
 * Variables used to remember parts of the last time conversion. 
 * Maybe we can avoid a full conversion.
 * Warning: this code is not reentrant (use static local variables, without mutex)
 */
  static time_t lasttime=0;	/* cached UNIX time */
  static long lastdays=0;
  static unsigned lastddate=0;	/* cached corresponding values */
  static unsigned lastdtime=0;
  unsigned long seconds;
  long days;
  int year, inc, month;
  int * months;

  /* Remember DOS idea of time is relative to 1980. UNIX is relative to 1970.
   * If we get a time before 1980, do not give totally crazy results.
   */
/* FIXME: daylight savings time correction!!! */
  if (t < SECONDSTO1980) {
	if (deDate) {
		deDate[0] = deDate[1] = 0;
	}
	if (deTime) {
		deTime[0] = deTime[1] = 0;
	}
  return; /* we are done */
  }

  /* If the time from the last conversion is the same as now,
   * then skip the computations and use the saved result.
   */
  if (lasttime != t) {
#if 0 /* timeval interface... */
	lasttime = tvp->tv_sec - (tz.tz_minuteswest * 60)
			/* +- daylight savings time correction */;
#else
	lasttime = t;
#endif
/* FIXME: daylight savings time correction
	t +-= daylight savings time correction
 */
	lastdtime = (((t /     2 ) % 30) << DT_2SECONDS_SHIFT)
	          + (((t /    60 ) % 60) << DT_MINUTES_SHIFT)
	          + (((t /(60*60)) % 24) << DT_HOURS_SHIFT);

	days = t / (24 * 60 * 60);
	/* If the number of days since 1970 is the same as the
	 * last time we did the computation then skip all this
	 * leap year and month stuff.
	 */
	if (days != lastdays) {
		lastdays = days;
		for (year = 1970; ; year++) {
			inc = year & 0x03 ? 365 : 366;
			if (days < inc) break;
			days -= inc;
		}
		months = year & 0x03 ? regyear : leapyear;
		for (month = 0; month < 12; month++) {
			if (days < months[month]) break;
			days -= months[month];
		}
		lastddate = ((days  + 1) << DD_DAY_SHIFT)
		          + ((month + 1) << DD_MONTH_SHIFT)
		          + ((year-1980) << DD_YEAR_SHIFT);
	}
  }

  if (deDate) {
	deDate[0] = lastddate & 0xFF;
	deDate[1] = lastddate << 8;
  }
  if (deTime) {
	deTime[0] = lastdtime & 0xFF;
	deTime[1] = lastdtime << 8;
  }
}

/*===========================================================================*
 *				conv_83toname				     *
 *===========================================================================*/
PUBLIC int conv_83toname(
  struct fat_direntry *fatdp,	/* FAT directory entry, as read on disk */
  char string[NAME_MAX+1])	/* where to put the equivalent name */
{
/* ...
 * This functions returns
 *   CONV_OK if conversion was trouble free,
 *   CONV_INVAL if invalid characters were encountered.
 */
/* FIXME: ATTR_HIDDEN could be transformed to leading dot */
  const unsigned char *p;
  char *q;

  assert(NAME_MAX >= 8 + 1 + 3);

  if (fatdp->deLCase & LCASE_NAME) {
	p = (const unsigned char*) & fatdp->deName[0];
	q = & string[0];
	while (p < (const unsigned char*) &fatdp->deName[8])
		*q++ = tolower(*p++);
  } else {
	memcpy(string, fatdp->deName, 8);
	p = (const unsigned char*) & fatdp->deName[8];
	q = & string[8];
  }
  assert(p == (const unsigned char*) &fatdp->deName[8]);
  assert(q == & string[8]);

  while (q-- >= & string[1])
	if (*q != ' ') break;

  if (q == & string[0] && *q == ' ') {
	/* only spaces characters... */
	DBGprintf(("FATfs: encountered entry without name (all spaces).\n"));
	return(CONV_INVAL);
  }

  assert(q >= & string[0]);
  assert(q <  & string[8]);

  if (fatdp->deName[0] == SLOT_E5)
	string[0] = SLOT_DELETED;	/* DOS was hacked too! */

  ++q;
  *q++ = '.';

  assert(p == (const unsigned char*) & fatdp->deExtension[0]);

  if (fatdp->deLCase & LCASE_EXTENSION) {
	while (p < (const unsigned char*) & fatdp->deExtension[3])
		*q++ = tolower(*p++);
  } else {
	memcpy(q, fatdp->deExtension, 3);
	p += 3;
	q += 3;
  }

  assert(p == (const unsigned char*) & fatdp->deExtension[3]);
  assert(q >= & string[0]);
  assert(q <= & string[NAME_MAX+1]);
  assert(q <= & string[8+1+3+1]);
  assert(q >  & string[4]);
  assert(q[-4] == '.');

  while (*--q == ' ') ;	/* will stop at '.' anyway */
  if (*q == '.')
	--q;			/* no extension, remove the dot */
  assert(q >= & string[0]);
  assert(q <  & string[8+1+3]);
  assert(q <  & string[NAME_MAX]);
  *++q = '\0';
  if (strlen(string) != q - string) {
	/* some '\0' are embedded in the FAT name */
	DBGprintf(("FATfs: encountered NUL character in a name.\n"));
	return(CONV_INVAL);
  } else if (strchr(string, '/') != NULL) {
	DBGprintf(("FATfs: encountered '/' character in a name.\n"));
	return(CONV_INVAL);
  } else
	return(CONV_OK);
}

/*===========================================================================*
 *				conv_nameto83				     *
 *===========================================================================*/
PUBLIC int conv_nameto83(
  char string[NAME_MAX+1],	/* some file name (passed by VFS) */
  struct fat_direntry *fatdp)	/* where to put the 8.3 compliant equivalent
				 * with resulting LCase attribute also */
{
/* Transforms a filename (as part of a path) into the form suitable to be
 * used in FAT directory entries, named 8.3 because it could only have up
 * to 8 characters before the (only) dot, and 3 characters after it.
 * This functions returns
 *   CONV_OK if conversion was trouble free,
 *   CONV_HASLOWER if conversion was done with some character uppercased,
 *   CONV_NAMETOOLONG if it does not fit the 8.3 limits,
 *   CONV_TRAILINGDOT if the name has a trailing dot (valid with DOS/Windows),
 *   CONV_INVAL if invalid characters were encountered.
 */
/* FIXME: leading dot could be transformed to ATTR_HIDDEN */
  const unsigned char *p = (const unsigned char *)string;
  unsigned char *q;
  int prev, next, haslower, hasupper, res;

  memset(fatdp->deName, ' ', 8+3);	/* fill with spaces */
  fatdp->deLCase &= ~ (LCASE_NAME|LCASE_EXTENSION);

  switch (next = *p) {
  case '.':
	if (p[1] == '\0') {
		memcpy(fatdp->deName, NAME_DOT, 8+3);
		return(CONV_OK);
	} else if (p[1] == '.' && p[2] == '\0') {
		memcpy(fatdp->deName, NAME_DOT_DOT, 8+3);
		return(CONV_OK);
	} else
	/* cannot store a filename starting with . */
		return(CONV_INVAL);
  case '\0':
	/* filename staring with NUL ? */
	DBGprintf(("FATfs: passed filename starting with \\0\n"));
	return(CONV_INVAL);
  case ' ':
	/* filename staring with space; legal but slippery */
	DBGprintf(("FATfs: warning: passed filename starting with space\n"));
	break;
  case SLOT_E5:
	DBGprintf(("FATfs: warning: initial character '\\005' in a filename, "
		"will be mangled into '\\xE5'\n"));
  case SLOT_DELETED:
	next = SLOT_E5;
	break;
  }

  haslower = hasupper = prev = 0;
  for (q=&fatdp->deName[0]; q < &fatdp->deName[8]; ++q) {
	if (next == '.' || next == '\0') {
		if (prev == ' ') {
			DBGprintf(("FATfs: warning: final space character "
				"in a name, will be dropped.\n"));
		}
		break;
	} else if (strchr(FAT_FORBIDDEN_CHARS, next) != NULL) {
		/* some characters cannot enter in FAT filenames */
		return(CONV_INVAL);
	} else {
		hasupper |= isupper(next);
		haslower |= islower(next);
		*q = toupper(next);
	}
	prev = next;
	next = *++p;
  }
/* FIXME: move to caller when creating... */
/* check q-d==3 &&
   static const char *dev3[] = {"CON", "AUX", "PRN", "NUL", "   "};
 * check q-d==4 && isdigit(q[-1])
   static const char *dev4[] = {"COM", "LPT" };
 */
  if (haslower && !hasupper)
	fatdp->deLCase |= LCASE_NAME;
  res = haslower ? CONV_HASLOWER : CONV_OK;

  if (*p == '\0') {
	/* no extension */
	return(res);
  }
  if (*p++ != '.') {
	/* more than 8 chars in name... */
	return(CONV_NAMETOOLONG);
  }
  next = *p;
  if (next == '\0') {
	/* initial filename was "foobar." (trailing dot)
	 * DOS and Windows recognize it the same as "foobar"
	 */
	DBGprintf(("FATfs: filename \"%s\" has a trailing dot...\n",
			string));
	return(CONV_TRAILINGDOT);	/* FIXME: haslower is not registered */
  }

  haslower = hasupper = prev = 0;
  for (q=&fatdp->deExtension[0]; q < &fatdp->deExtension[3]; ++q) {
	if (next == '\0') {
		break;
	} else if (strchr(FAT_FORBIDDEN_CHARS, next) != NULL) {
		/* some characters cannot enter in FAT filenames */
		return(CONV_INVAL);
	} else {
		hasupper |= isupper(next);
		haslower |= islower(next);
		*q = toupper(next);
	}
	prev = next;
	next = *++p;
  }

  if (next == '\0') {
	if (prev == ' ') {
		DBGprintf(("FATfs: warning: final space character "
			"in an extension, will be dropped.\n"));
	}
	if (haslower && !hasupper)
		fatdp->deLCase |= LCASE_EXTENSION;
	return(haslower ? CONV_HASLOWER : res);
  }
  /* more than 3 chars in extension... */
  return(CONV_NAMETOOLONG);
}

/*===========================================================================*
 *				lfn_chksum				     *
 *===========================================================================*/
PUBLIC int lfn_chksum(struct fat_direntry * fatdp)
{
/* Compute the checksum of a FAT entry name for LFN check. */
  int i;
  unsigned char sum=0;
  const unsigned char *p = fatdp->deName;

  for (i = 8+3; --i >= 0; sum += *p++)
	sum = (sum << 7)|(sum >> 1);
  return sum;
}

/*===========================================================================*
 *				conv_lfntoname				     *
 *===========================================================================*/
PUBLIC int conv_lfntoname(int count, /* number of LFN entries */
  struct fat_lfnentry lfnda[],	/* array of LFN entries, as read on disk */
  char string[],		/* where to put the equivalent name */
  size_t *len)			/* input:  sizeof(string);
				 * output: strlen(string); */
{
/* ...
 * This functions returns
 *   CONV_OK if conversion was trouble free,
 *   CONV_INVAL if invalid characters were encountered.
 */
  return(CONV_INVAL); /*fails*/
}

/*===========================================================================*
 *				comp_name_lfn				     *
 *===========================================================================*/
PUBLIC int comp_name_lfn(
  char string[NAME_MAX+1],	/* some file name (passed by VFS) */
  int count,			/* number of LFN entries, as read on disk */
  struct fat_lfnentry lfnda[])	/* array of LFN entries, as read on disk */
{
  return 1; /*fails*/
}

/*===========================================================================*
 *				conv_nametolfn				     *
 *===========================================================================*/
PUBLIC int conv_nametolfn(
  char string[NAME_MAX+1],	/* some file name (passed by VFS) */
  int * count,			/* number of LFN entries to be written */
  struct fat_lfnentry lfnda[],	/* array of LFN entries to write on disk */
  struct fat_direntry *fatdp)	/* where to put the 8.3 compliant equivalent
				 * with resulting LCase attribute also */
{
  return(CONV_INVAL); /*fails*/
}
