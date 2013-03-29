# LSC For Now
NOGCCERROR:= yes
PROG=	chown
MAN=

LINKS+=	${BINDIR}/chown ${BINDIR}/chgrp

# Uses std_err
LDADD+=-lcompat_minix
#unfortunately LIBCOMPAT_MINIX is not initialized in bsd.lib.mk :-(
#DPADD+=${LIBCOMPAT_MINIX}

.include <bsd.prog.mk>
