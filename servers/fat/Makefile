# Makefile for FAT fs

#NR_BUFS= 100
#CPPFLAGS+= -DNR_BUFS=${NR_BUFS}

PROG=	fat
SRCS=	main.c table.c driver.c inode.c lookup.c \
	mount.c read.c stat.c statfs.c # utility.c cache.c

DPADD+=	${LIBSYS}
# ${LIBTIMERS}
LDADD+=	-lsys
# -ltimers

MAN=

BINDIR?= /sbin

.include <bsd.prog.mk>
