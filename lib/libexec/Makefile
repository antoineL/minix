# Makefile for libexec

LIB=	exec
INCS=	libexec.h
SRCS=	exec_memmap.c exec_aout.c exec_elf.c
MAN+=	exec_memmap.3

.if (${NBSD_LIBC} != "no")
INCSDIR= /usr/netbsd/include
.else
INCSDIR= /usr/include
.endif
.include <bsd.lib.mk>
.include <bsd.man.mk>
