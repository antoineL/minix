# Makefile for Pipe File System (PFS)
.include <bsd.own.mk>

PROG=	pfs
SRCS=	open.c table.c inode.c main.c super.c link.c \
	buffer.c read.c misc.c utility.c stadir.c \
	uds.c dev_uds.c

DPADD+=	${LIBDRIVER} ${LIBSYS}
LDADD+=	-ldriver -lsys

MAN=

BINDIR?= /usr/sbin
.if ${COMPILER_TYPE} == "gnu" && ${OBJECT_FMT} == "ELF"
LDFLAGS+= -L ${MINIXSRCDIR}/tools -T minix-serv.lds -T stack_32K.lds
.else
INSTALLFLAGS+=	-S 128k
.endif

.include <bsd.prog.mk>
