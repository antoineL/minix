# Makefile for the boot monitor package.

.include <bsd.own.mk>

# 16-bit code; various boot blocks
SUBDIR= bootblock cdbootblock

# various boot blocks from NetBSD, ELF-compatible
SUBDIR+= mbr cdboot

# 16-bit code; needs the i86 ACK library
SUBDIR+= boot

# Userland MINIX code
SUBDIR+= edparams

.include <bsd.subdir.mk>
