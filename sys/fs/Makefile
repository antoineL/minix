#	$NetBSD: Makefile,v 1.20 2011/06/27 11:52:24 uch Exp $

.if !defined(__MINIX)
SUBDIR=	adosfs cd9660 efs filecorefs hfs msdosfs nilfs ntfs ptyfs	\
	puffs smbfs sysvbfs tmpfs udf union v7fs
.else
SUBDIR= cd9660
.endif

.include <bsd.kinc.mk>
