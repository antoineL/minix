# Makefile for memory driver (MEMORY)
.include <bsd.own.mk>

PROG=	memory
SRCS=	memory.c imgrd.c

DPADD+=	${LIBDRIVER} ${LIBSYS}
LDADD+=	-ldriver -lsys

MAN=

BINDIR?= /usr/sbin
.if ${COMPILER_TYPE} == "gnu" && ${OBJECT_FMT} == "ELF"
LDFLAGS+= -L ${MINIXSRCDIR}/tools -T minix-serv.lds -T stack_8K.lds
.else
INSTALLFLAGS+=	-S 8k
.endif

CPPFLAGS.memory.c+=	-I${MINIXSRCDIR}
CPPFLAGS.imgrd.c+=	-I${.CURDIR}/../ramdisk -O0

imgrd.d: touch-genfiles

touch-genfiles:
	[ -e ${.CURDIR}/../ramdisk/image.c ] || touch -t 197001010000.00 ${.CURDIR}/../ramdisk/image.c

.include <bsd.prog.mk>
