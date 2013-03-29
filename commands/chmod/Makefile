# LSC For now...
NOGCCERROR:= yes
PROG=	chmod
MAN=
BINDIR=/bin
SYMLINKS+= $(BINDIR)/$(PROG) /usr/bin/chmod

# Uses std_err
LDADD+=-lcompat_minix
#unfortunately LIBCOMPAT_MINIX is not initialized in bsd.lib.mk :-(
#DPADD+=${LIBCOMPAT_MINIX}

.include <bsd.prog.mk>
