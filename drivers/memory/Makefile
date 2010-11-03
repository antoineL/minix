# Makefile for memory driver (MEMORY)
.include <bsd.own.mk>

PROG=	memory
SRCS=	memory.c imgrd.c

DPADD+=	${LIBDRIVER} ${LIBSYS}
LDADD+=	-ldriver -lsys

MAN=

BINDIR?= /usr/sbin
STACKSIZE=	8k

CPPFLAGS.memory.c+=	-I${MINIXSRCDIR}
CPPFLAGS.imgrd.c+=	-I${.CURDIR}/../ramdisk -I../ramdisk -T /usr/tmp

# Where is image.c, relative to us?
REL_IMAGE_C=	../ramdisk/image.c

imgrd.d: touch-genfiles

# If image.c does not exist (yet) while making depend, create it (in ${.OBJDIR})
touch-genfiles: .PHONY
	[ -e ${.CURDIR}/${REL_IMAGE_C} -o -e ${REL_IMAGE_C} ] \
	|| touch -t 197001010000.00 ${REL_IMAGE_C}

.include <bsd.prog.mk>
