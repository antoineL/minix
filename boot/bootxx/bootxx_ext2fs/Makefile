# $NetBSD: Makefile,v 1.1 2010/09/11 13:06:37 tsutsui Exp $

FS=ext2fs

.ifdef __MINIX
CPPFLAGS+=	-DCHECK_MINIX_SUBPART -DBOOT_FROM_EXT2FS
.endif

.include <../Makefile.bootxx>
