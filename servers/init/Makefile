# Makefile for the init program (INIT)
PROG=	init
SRCS=	init.c

DPADD+=	${LIBSYS}
LDADD+=	-lsys

MAN=

BINDIR?= /usr/sbin
STACKSIZE=	64k

.include <bsd.prog.mk>
