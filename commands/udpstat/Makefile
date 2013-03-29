PROG=	udpstat
CPPFLAGS+= -I${NETBSDSRCDIR}/servers -D'ARGS(a)=a'
MAN=

# Uses paramvalue
LDADD+=-lminlib
DPADD+=${LIBMINLIB}

# indirectly uses std_err
LDADD+=-lcompat_minix
#unfortunately LIBCOMPAT_MINIX is not initialized in bsd.lib.mk :-(
#DPADD+=${LIBCOMPAT_MINIX}

.include <bsd.prog.mk>
