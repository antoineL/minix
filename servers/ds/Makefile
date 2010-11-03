# Makefile for Data Store Server (DS)
PROG=	ds
SRCS=	main.c store.c

DPADD+=	${LIBSYS}
LDADD+=	-lsys

MAN=

BINDIR?= /usr/sbin
STACKSIZE=	16k

.include <bsd.prog.mk>
