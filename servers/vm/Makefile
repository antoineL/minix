# Makefile for VM server
.include <bsd.own.mk>

PROG=	vm
SRCS=	main.c alloc.c utility.c exec.c exit.c fork.c break.c \
	signal.c mmap.c slaballoc.c region.c pagefaults.c addravl.c \
	physravl.c rs.c queryexit.c yieldedavl.c regionavl.c

DPADD+=	${LIBSYS}
LDADD+=	-lsys

MAN=

BINDIR?= /usr/sbin
.if ${COMPILER_TYPE} == "gnu" && ${OBJECT_FMT} == "ELF"
LDFLAGS+= -L ${MINIXSRCDIR}/tools -T minix-serv.lds -T stack_128K.lds
.else
INSTALLFLAGS+=	-S 128k
.endif

CPPFLAGS+= -I${.CURDIR} -I${.CURDIR}/arch/${ARCH}
CPPFLAGS+=	-I${MINIXSRCDIR}

.include "arch/${ARCH}/Makefile.inc"
.include <bsd.prog.mk>
