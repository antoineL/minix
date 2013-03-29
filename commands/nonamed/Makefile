PROG=	nonamed
MAN=

# Uses dhcp_gettags, asyn_*
LDADD+=-lminlib -lasyn
#unfortunately LIBASYN is not initialized in bsd.lib.mk :-(
DPADD+=${LIBMINLIB} ${LIBASYN:U${DESTDIR}/usr/lib/libasyn.a}

# indirectly uses std_err
LDADD+=-lcompat_minix
#unfortunately LIBCOMPAT_MINIX is not initialized in bsd.lib.mk :-(
#DPADD+=${LIBCOMPAT_MINIX}

.include <bsd.prog.mk>
