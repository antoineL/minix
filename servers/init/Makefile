# Makefile for the init program (INIT)
.include <bsd.own.mk>

PROG=	init
SRCS=	init.c

MAN=

BINDIR?= /usr/sbin

noLDFLAGS+= -Wl,--section-start=.init=0x0

.include <bsd.prog.mk>
