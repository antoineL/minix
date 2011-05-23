# Makefile for Minix File System (MFS)
.include <bsd.own.mk>

PROG=	mfs
SRCS=	cache.c device.c link.c \
	mount.c misc.c open.c protect.c read.c \
	stadir.c stats.c table.c time.c utility.c \
	write.c inode.c main.c path.c super.c

DPADD+=	${LIBSYS}
LDADD+=	-lsys

MAN=

BINDIR?= /sbin
.if ${COMPILER_TYPE} == "gnu" && ${OBJECT_FMT} == "ELF"
LDFLAGS+= -L ${MINIXSRCDIR}/tools -T minix-serv.lds -T stack_128K.lds
.else
INSTALLFLAGS+=	-S 128k
.endif

DEFAULT_NR_BUFS= 1024
CPPFLAGS+= -DDEFAULT_NR_BUFS=${DEFAULT_NR_BUFS}

.include <bsd.prog.mk>
