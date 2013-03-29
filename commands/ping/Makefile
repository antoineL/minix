PROG=	ping
BINMODE= 4755
MAN=

# Uses oneC_sum
LDADD+=-lminlib
DPADD+=${LIBMINLIB}

# indirectly uses std_err
LDADD+=-lcompat_minix
#unfortunately LIBCOMPAT_MINIX is not initialized in bsd.lib.mk :-(
#DPADD+=${LIBCOMPAT_MINIX}

.include <bsd.prog.mk>
