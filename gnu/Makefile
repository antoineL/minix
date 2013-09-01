#	$NetBSD: Makefile,v 1.17 2010/12/03 21:38:47 plunky Exp $

.if 0
# Hack to force the download and untar of texinfo.tgz
# FIXME: find a better way, this way does not work properly...
SUBDIR+= dist .WAIT usr.bin
.else # Normel NetBSD way
SUBDIR+= usr.bin
.endif

# Speedup stubs for some subtrees that don't need to run these rules
includes-libexec:
	@true

.include <bsd.subdir.mk>
