# Makefile for memory driver (MEMORY)

.include <bsd.own.mk>

PROG=	memory
SRCS=	memory.c ${IMGRD} 

DPADD+=	${LIBDRIVER} ${LIBSYS}
LDADD+=	-ldriver -lsys

MAN=

BINDIR?= /usr/sbin
INSTALLFLAGS+=	-S 8k

CPPFLAGS.memory.c+=	-I${MINIXSRCDIR}

.if ${COMPILER_TYPE} == "gnu"
IMGRD = image.mfs
.SUFFIXES:      .mfs .c .o

.mfs.o:
	${_MKTARGET_CREATE}
	${OBJCOPY} -Ibinary -B${ARCH} -Oelf32-${ARCH}-minix $< $@

${IMGRD}:
	ln -s ../ramdisk/image $@

CLEANFILES+=	${IMGRD}

.else # ${COMPILER_TYPE} == "ack"
IMGRD = imgrd.c
CPPFLAGS.imgrd.c+=	-I${.CURDIR}/../ramdisk -O0 -T /usr/tmp
.endif

.include <minix.bootprog.mk>
