/* This file contains file metadata retrieval and manipulation routines.
 *
 * The entry points into this file are:
 *   dos2unixtime	convert DOS-style date and time into time_t
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

#include <stdint.h>
#include <time.h>

#include "fat.h"

#ifndef	PRIVATE
#define PRIVATE	static		/* PRIVATE x limits the scope of x */
#endif
#ifndef	PUBLIC
#define PUBLIC			/* PUBLIC is the opposite of PRIVATE */
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
