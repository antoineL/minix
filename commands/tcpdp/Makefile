.include <bsd.own.mk>

PROG=	tcpdp
SRCS=	tcpd.c
CPPFLAGS+= -DPARANOID=1
MAN=

.PATH: ${NETBSDSRCDIR}/commands/tcpd

# Uses servxchack
LDADD+=-lminlib
DPADD+=${LIBMINLIB}

# indirectly uses std_err
LDADD+=-lcompat_minix
#unfortunately LIBCOMPAT_MINIX is not initialized in bsd.lib.mk :-(
#DPADD+=${LIBCOMPAT_MINIX}

.include <bsd.prog.mk>
