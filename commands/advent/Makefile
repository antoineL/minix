# Makefile for advent

PROG=	advent
SRCS=	advent.c database.c english.c initial.c itverb.c score.c \
	travel.c turn.c utility.c verb.c vocab.c

# Where to put the adventure text files
# Need the trailing "/" on TEXTDIR
TEXTDIR=	/usr/lib/advent/
FILESDIR=	${TEXTDIR}
DATFILES=	advent1.dat advent2.dat advent3.dat advent4.dat
FILES=		${DATFILES}
MAN=

database.o: advtext.h

setup: setup.c advent.h
	${_MKTARGET_COMPILE}
	${HOST_LINK.c} -o $@ ${.CURDIR}/setup.c

DPSRCS+=	setup.c

advent1.dat advent2.dat advent3.dat advent4.dat advtext.h ${.OBJDIR}/advtext.h: \
		setup advent1.txt advent2.txt advent3.txt advent4.txt
	${_MKMSG_COMPILE} $(DATFILES)
	./setup ${.CURDIR}

.if !exists(advtext.h)
# Deal with the dependency to advtext.h 
database.d: touch-genfiles

touch-genfiles:
	if test ! -e advtext.h ; \
	then echo > advtext.h ; \
	     touch -t 197001010000.00 advtext.h ; fi
.endif
# Alternative way to deal with depend; currently disabled;
# require to compile and run ./setup just to create dependencies.
#database.d: advtext.h

CPPFLAGS.database.c= -I.

CPPFLAGS.advent.c= -DTEXTDIR='"${TEXTDIR}"'

CLEANFILES+=	${DATFILES} advtext.h setup

.include <bsd.prog.mk>

