# Makefile for shutdown / halt / reboot.

PROGS=	shutdown halt
SRCS.shutdown=	shutdown.c sh_wall.c log.c
SRCS.halt=	halt.c log.c
BINDIR=/bin
BINMODE= 4754
SYMLINKS+=	${BINDIR}/halt ${BINDIR}/reboot
SYMLINKS+=	${BINDIR}/halt /usr/bin/halt
SYMLINKS+=	${BINDIR}/shutdown /usr/bin/shutdown
SYMLINKS+=	${BINDIR}/reboot /usr/bin/reboot
MAN.shutdown=
MAN.halt=

# Uses itoa
LDADD+=-lminlib
DPADD+=${LIBMINLIB}

# indirectly uses std_err
LDADD+=-lcompat_minix
#unfortunately LIBCOMPAT_MINIX is not initialized in bsd.lib.mk :-(
#DPADD+=${LIBCOMPAT_MINIX}

.include <bsd.prog.mk>
