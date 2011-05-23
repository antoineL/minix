# Makefile for ext2 filesystem
.include <bsd.own.mk>

PROG=	ext2
SRCS=	balloc.c cache.c device.c link.c \
	mount.c misc.c open.c protect.c read.c \
	stadir.c table.c time.c utility.c \
	write.c ialloc.c inode.c main.c path.c \
	super.c optset.c
DPADD+=	${LIBSYS}
LDADD+=	-lsys

MAN=

BINDIR?= /sbin

# Boot image file system?
#.if ${COMPILER_TYPE} == "gnu" && ${OBJECT_FMT} == "ELF"
#LDFLAGS+= -L ${MINIXSRCDIR}/tools -T minix-serv.lds -T stack_128K.lds
#.else
#INSTALLFLAGS+=	-S 128k
#.endif

DEFAULT_NR_BUFS= 1024
CPPFLAGS+= -DDEFAULT_NR_BUFS=${DEFAULT_NR_BUFS}

.include <bsd.prog.mk>
