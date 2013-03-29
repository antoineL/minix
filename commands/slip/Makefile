PROG=	slip
MAN=

# Uses asyn_*
LDADD+=-lasyn
#unfortunately LIBASYN is not initialized in bsd.lib.mk :-(
DPADD+=${LIBASYN:U${DESTDIR}/usr/lib/libasyn.a}

.include <bsd.prog.mk>
