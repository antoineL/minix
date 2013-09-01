#	$NetBSD: Makefile,v 1.1 2002/12/23 17:52:08 jdolecek Exp $

INCSDIR= /usr/include/isofs/cd9660

.if !defined(__MINIX)
INCS=	cd9660_extern.h cd9660_mount.h cd9660_node.h cd9660_rrip.h iso.h \
	iso_rrip.h
.else
# MINIX does not use the kernel driver, but the isofs FS server and
# the makefs(8) utility uses NetBSD code hence these headers:
INCS=	cd9660_extern.h cd9660_rrip.h iso.h iso_rrip.h
.endif

.include <bsd.kinc.mk>
