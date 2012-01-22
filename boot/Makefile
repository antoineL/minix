# Makefile for the boot monitor package.

.include <bsd.own.mk>

# 16-bit code; various boot blocks
SUBDIR= masterboot bootblock cdbootblock

# Supplementary MBR code (alternate to masterboot)
SUBDIR+= jumpboot

# various MBR blocks from NetBSD, ELF-compatible
SUBDIR+= mbr

# 16-bit code; needs the i86 ACK library
SUBDIR+= boot

# Userland MINIX code
SUBDIR+= edparams

.include <bsd.subdir.mk>
