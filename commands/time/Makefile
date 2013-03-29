PROG=	time
MAN=

# uses read_tsc, std_err
LDADD+=-lminlib -lcompat_minix
#unfortunately LIBCOMPAT_MINIX is not initialized in bsd.lib.mk :-(
#DPADD+=$LIBMINLIB} ${LIBCOMPAT_MINIX}

.include <bsd.prog.mk>
