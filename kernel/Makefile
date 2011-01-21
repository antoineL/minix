# Makefile for kernel
.include <bsd.own.mk>

PROG=	kernel

# first-stage, arch-dependent startup code
SRCS=	mpx.S
SRCS+=	start.c table.c main.c proc.c \
	system.c clock.c utility.c debug.c interrupt.c \
	watchdog.c cpulocals.c

.ifdef CONFIG_SMP
SRCS += smp.c
.endif

DPADD+=	${LIBTIMERS} ${LIBSYS}
LDADD+=	-ltimers -lsys

CFLAGS += -D__kernel__

.if ${COMPILER_TYPE} == "ack"
LDFLAGS+= -.o
.elif ${COMPILER_TYPE} == "gnu"
CPPFLAGS+= -ffreestanding -fno-stack-protector
LDFLAGS+= -T ${.CURDIR}/arch/${ARCH}/kernel.lds
LDFLAGS+= -nostdlib -L${DESTDIR}${LIBDIR}
CFLAGS+=-march=i386
DPADD+=	${LIBC}
#LDADD+=	-lgcc -lc -lgcc
LDADD+=	-lc
.endif

CPPFLAGS+=	-I${.CURDIR} -I${.CURDIR}/arch/${ARCH}/include -I${MINIXSRCDIR}
AFLAGS+=	-I${.CURDIR} -I${.CURDIR}/arch/${ARCH}/include -I${MINIXSRCDIR}

INSTALLFLAGS+=	-S 0
BINDIR=	/usr/sbin
MAN=

.include "system/Makefile.inc"
.include "arch/${ARCH}/Makefile.inc"

# These come last, so the profiling buffer is at the end of the data segment
SRCS+= profile.c do_sprofile.c
.include <bsd.prog.mk>

debug.d: extracted-errno.h extracted-mfield.h extracted-mtype.h

extracted-errno.h: ${MINIXSRCDIR}/include/errno.h
	${_MKTARGET_CREATE}
	sed -n -e '    y/	/ / ; \
	           /^#/s/^# *define *\([A-Z_][A-Z0-9_]*\)  *(_SIGN *[0-9][0-9]* *).*/IDENT(\1)/p' ${.ALLSRC} | \
	sort > ${.TARGET}

extracted-mfield.h: extract-mfield.sh
	${_MKTARGET_CREATE}
	${HOST_SH} ${.CURDIR}/extract-mfield.sh \
	            ${MINIXSRCDIR}/lib/libc/other \
	            ${MINIXSRCDIR}/lib/libc/posix \
	            ${MINIXSRCDIR}/lib/libsys > ${.TARGET}

extracted-mfield.h: ${MINIXSRCDIR}/lib/libc/other/*.c
extracted-mfield.h: ${MINIXSRCDIR}/lib/libc/posix/*.c
extracted-mfield.h: ${MINIXSRCDIR}/lib/libsys/*.c

extracted-mtype.h: ${MINIXSRCDIR}/include/minix/com.h \
		${MINIXSRCDIR}/include/minix/callnr.h
	${_MKTARGET_CREATE}
	( sed -n -e '  y/	/ / ; \
	           /^# *define *NCALLS */d ; \
	           /^#/s/^# *define *\([A-Z_][A-Z0-9_]*\)  *[0-9].*/IDENT(\1)/p' \
			${MINIXSRCDIR}/include/minix/callnr.h ; \
	  sed -n -e '  y/	/ / ; \
	           /^#/s/^# *define *\([A-Z_][A-Z0-9_]*\)  *( *[A-Z0-9_][A-Z0-9_]*_BASE *+.*/IDENT(\1)/p ; \
	           /^#/s/^# *define *\([A-Z_][A-Z0-9_]*\)  *( *KERNEL_CALL *+.*/IDENT(\1)/p' \
			${MINIXSRCDIR}/include/minix/com.h ; \
	) | sort > ${.TARGET}

clean:
	rm -f extracted-errno.h extracted-mfield.h extracted-mtype.h
CLEANFILES+=	extracted-errno.h extracted-mfield.h extracted-mtype.h
