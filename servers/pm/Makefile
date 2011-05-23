# Makefile for Process Manager (PM)
.include <bsd.own.mk>

PROG=	pm
SRCS=	main.c forkexit.c break.c exec.c time.c alarm.c \
	signal.c utility.c table.c trace.c getset.c misc.c \
	profile.c dma.c mcontext.c schedule.c

DPADD+=	${LIBSYS} ${LIBTIMERS}
LDADD+=	-lsys -ltimers

MAN=

BINDIR?= /usr/sbin
.if ${COMPILER_TYPE} == "gnu" && ${OBJECT_FMT} == "ELF"
LDFLAGS+= -L ${MINIXSRCDIR}/tools -T minix-serv.lds -T stack_32K.lds
.else
INSTALLFLAGS+=	-S 32k
.endif

CPPFLAGS.main.c+=	-I${MINIXSRCDIR}
CPPFLAGS.misc.c+=	-I${MINIXSRCDIR}
CPPFLAGS.schedule.c+=	-I${MINIXSRCDIR}
CPPFLAGS.utility.c+=	-I${MINIXSRCDIR}

.include <bsd.prog.mk>
