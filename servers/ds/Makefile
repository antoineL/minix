# Makefile for Data Store Server (DS)
.include <bsd.own.mk>

PROG=	ds
SRCS=	main.c store.c

DPADD+=	${LIBSYS}
LDADD+=	-lsys

MAN=

BINDIR?= /usr/sbin
.if ${COMPILER_TYPE} == "gnu" && ${OBJECT_FMT} == "ELF"
LDFLAGS+= -L ${MINIXSRCDIR}/tools -T minix-serv.lds -T stack_16K.lds
.else
INSTALLFLAGS+=	-S 16k
.endif

.include <bsd.prog.mk>
