# Makefile for Virtual File System (VFS)
.include <bsd.own.mk>

PROG=	vfs
SRCS=	main.c open.c read.c write.c pipe.c dmap.c \
	path.c device.c mount.c link.c exec.c \
	filedes.c stadir.c protect.c time.c \
	lock.c misc.c utility.c select.c table.c \
	vnode.c vmnt.c request.c fscall.c gcov.c

DPADD+=	${LIBSYS} ${LIBTIMERS} ${LIBEXEC}
LDADD+=	-lsys -ltimers -lexec

MAN=

BINDIR?= /usr/sbin
.if ${COMPILER_TYPE} == "gnu" && ${OBJECT_FMT} == "ELF"
LDFLAGS+= -L ${MINIXSRCDIR}/tools -T minix-serv.lds -T stack_16K.lds
.else
INSTALLFLAGS+=	-S 16k
.endif

.include <bsd.prog.mk>
