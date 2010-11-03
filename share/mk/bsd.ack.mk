#CPPFLAGS+= -wo
CC+= -wo

.SUFFIXES:	.o .e .S

# Treated like a C file
.e.o:
	${_MKTARGET_COMPILE}
	${COMPILE.c} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} ${.IMPSRC} -o ${.TARGET}
# .if !defined(CFLAGS) || empty(CFLAGS:M*-g*)
# 	${OBJCOPY} -x ${.TARGET}
# .endif

ASMCONV=gas2ack
ASMCONVFLAGS+=-mi386

APPFLAGS?=	${AFLAGS}
APPFLAGS+=	-D__ASSEMBLY__ -w -wo
.if empty(APPFLAGS:M-nostdinc)
APPFLAGS+=	${DESTDIR:D-nostdinc ${CPPFLAG_ISYSTEM} ${DESTDIR}/usr/include}
.endif

CPP.s=${CC} -E ${APPFLAGS}

# Need to convert ACK assembly files to GNU assembly before building
.S.o:
	${_MKTARGET_COMPILE}
	${CPP.s} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} \
		 -o ${.PREFIX}.gnu.s ${.IMPSRC}
	${ASMCONV} ${ASMCONVFLAGS} ${.PREFIX}.gnu.s ${.PREFIX}.ack.s
	${COMPILE.s} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} \
		  -o ${.TARGET} ${.PREFIX}.ack.s
	rm -rf ${.PREFIX}.ack.s ${.PREFIX}.gnu.s

###### Minix rule to set up mem allocations for boot image services
.if defined(STACKSIZE)
LDFLAGS+= -stack ${STACKSIZE}
.endif
