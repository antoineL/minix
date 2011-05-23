# Makefile for terminal driver (TTY)

.include <bsd.own.mk>

PROG=	tty
SRCS=	tty.c console.c keyboard.c pty.c rs232.c

DPADD+=	${LIBDRIVER} ${LIBSYS} ${LIBTIMERS}
LDADD+=	-ldriver -lsys -ltimers

MAN=

BINDIR?= /usr/sbin
.if ${COMPILER_TYPE} == "gnu" && ${OBJECT_FMT} == "ELF"
LDFLAGS+= -L ${MINIXSRCDIR}/tools -T minix-serv.lds -T stack_16K.lds
.else
INSTALLFLAGS+=	-S 16k
.endif

SUBDIR= keymaps

# Needs kernel/const.h, etc
CPPFLAGS+=	-I${MINIXSRCDIR}

.include <bsd.prog.mk>
.include <bsd.subdir.mk>
