# Makefile for the make configuration files

NOOBJ=	# defined; to avoid cd ${.OBJDIR}

.include <bsd.own.mk>

.if ${MKSHARE} != "no"
INCS=	bsd.ack.mk bsd.dep.mk bsd.files.mk \
	bsd.gcc.mk bsd.gcov.mk bsd.inc.mk \
	bsd.init.mk bsd.kinc.mk bsd.klinks.mk \
	bsd.lib.mk bsd.links.mk bsd.man.mk bsd.obj.mk bsd.own.mk \
	bsd.prog.mk bsd.subdir.mk bsd.sys.mk \
	sys.mk

INCSDIR=/usr/share/mk
NONBINMODE=444

# make install should do the same as make includes
realinstall: includes
.endif

.include <bsd.kinc.mk>
