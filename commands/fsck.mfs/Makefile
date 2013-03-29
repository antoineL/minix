FSCK=   ${NETBSDSRCDIR}/sbin/fsck
PROG=	fsck.mfs
SRCS=	fsck.c
CPPFLAGS+= -I${NETBSDSRCDIR}/servers -I${FSCK}
BINDIR=	/sbin
MAN=

# Uses bitmapsizes
LDADD+=-lminlib
DPADD+=${LIBMINLIB}

# indirectly uses std_err
LDADD+=-lcompat_minix
#unfortunately LIBCOMPAT_MINIX is not initialized in bsd.lib.mk :-(
#DPADD+=${LIBCOMPAT_MINIX}

.include <bsd.prog.mk>
