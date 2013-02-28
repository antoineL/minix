# Makefile for service
PROGS=	service printconfig
SRCS.service=service.c parse.c util.c configfile.c
SRCS.printconfig=print.c parse.c util.c configfile.c

BINDIR= /bin
MAN.service=	service.8
MAN.printconfig=

CPPFLAGS+=	-I${NETBSDSRCDIR}

.include <bsd.prog.mk>
