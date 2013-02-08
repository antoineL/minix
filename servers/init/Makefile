# Makefile for the init program (INIT)
.include <bsd.own.mk>

PROG=	init
SRCS=	init.c

MAN=

BINDIR?= /usr/sbin

# AL Currently VM is not able to launch a dynamically-linked binary at boot time
LDSTATIC?=	-static

# AL20130404 Init is much like a MINIX service, but for unkown (to me) reasons
# it is built as a regular program.
.include <bsd.prog.mk>
