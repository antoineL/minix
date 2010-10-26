# Makefile for FAT file system server

CPPFLAGS+= -DDEBUG
NR_BUFS= 100
CPPFLAGS+= -DNR_BUFS=${NR_BUFS}

PROG=	fat
SRCS=	main.c table.c optset.c driver.c cache.c inode.c \
	mount.c lookup.c readwrite.c stat.c statfs.c \
	fat.c directory.c direntry.c utility.c

DPADD+=	${LIBSYS}
LDADD+=	-lsys

MAN=	fat.8

BINDIR?= /sbin

.include <bsd.prog.mk>
