# Makefile for Reincarnation Server (RS)
.include <bsd.own.mk>

PROG=	rs
SRCS=	exec.c main.c request.c manager.c table.c utility.c memory.c error.c

DPADD+=	${LIBSYS} ${LIBEXEC}
LDADD+=	-lsys -lexec

MAN=

BINDIR?= /usr/sbin
.if ${COMPILER_TYPE} == "gnu" && ${OBJECT_FMT} == "ELF"
LDFLAGS+= -T ${MINIXSRCDIR}/tools/minix-serv.lds -T ${.CURDIR}/stack_rs.lds
.else
# Why such a size? Is it to create a hole around 00200000 for multiboot?
INSTALLFLAGS+=	-S 8125k
.endif

CPPFLAGS+=	-I${MINIXSRCDIR}

.include <bsd.prog.mk>
