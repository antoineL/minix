# Makefile for log driver
.include <bsd.own.mk>

PROG=	log
SRCS=	log.c diag.c liveupdate.c

DPADD+=	${LIBDRIVER} ${LIBSYS}
LDADD+=	-ldriver -lsys

MAN=

BINDIR?= /usr/sbin
.if ${COMPILER_TYPE} == "gnu" && ${OBJECT_FMT} == "ELF"
LDFLAGS+= -L ${MINIXSRCDIR}/tools -T minix-serv.lds -T stack_32K.lds
.else
INSTALLFLAGS+=	-S 32k
.endif

.include <bsd.prog.mk>
