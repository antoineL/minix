# Makefile for the init program (INIT)
.include <bsd.own.mk>

PROG=	init
SRCS=	init.c

DPADD+=	${LIBSYS}
LDADD+=	-lsys

MAN=

BINDIR?= /usr/sbin
.if ${COMPILER_TYPE} == "gnu" && ${OBJECT_FMT} == "ELF"
LDFLAGS+= -L ${MINIXSRCDIR}/tools -T minix-serv.lds -T stack_64K.lds
.else
INSTALLFLAGS+=	-S 64k
.endif

.include <bsd.prog.mk>
