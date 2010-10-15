# Makefile for awk.

PROG=	awk
SRCS=	awkgram.y b.c lex.c lib.c main.c parse.c proctab.c run.c tran.c
YHEADER=	yes
QUIET_YACC=	yes

CPPFLAGS+= -I. -I${.CURDIR} -DHAS_ISBLANK
HOST_CPPFLAGS+= -I. -I${.CURDIR}

build-tools: maketab

proctab.c: maketab
	${_MKTARGET_CREATE}
	./maketab > proctab.c

maketab: awkgram.h maketab.c
	${_MKTARGET_COMPILE}
	${HOST_LINK.c} -o $@ ${.CURDIR}/maketab.c

awkgram.h: awkgram.y

CLEANFILES= maketab proctab.c

.include <bsd.prog.mk>
