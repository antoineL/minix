# Makefile for FAT fs

#NR_BUFS= 100
#CPPFLAGS+= -DNR_BUFS=${NR_BUFS}

PROG=	fat
SRCS=	main.c table.c driver.c cache.c inode.c \
	mount.c lookup.c readwrite.c stat.c statfs.c # utility.c

DPADD+=	${LIBSYS}
# ${LIBTIMERS}
LDADD+=	-lsys
# -ltimers

MAN=

BINDIR?= /sbin

.include <bsd.prog.mk>
