#	Copied from tools/host-mkdep $NetBSD: Makefile,v 1.12 2012/12/02 12:44:06 apb Exp $

HOSTPROG= 	awk
HOSTPROGNAME=	${_TOOL_PREFIX}awk
HOST_BINDIR=	${TOOLDIR}/bin

NOMAN=	# defined
SRCS=	# empty

.include <bsd.hostprog.mk>

realall: awk
awk:
	cp /usr/bin/awk awk

# Use uninstalled copy of the install program
INSTALL_OBJ!=	cd ${NETBSDSRCDIR}/tools/binstall && ${PRINTOBJDIR}
INSTALL=	${INSTALL_OBJ}/xinstall

# Install rule, copied from src/tools/Makefile.host.
# We can't include Makefile.host because there is no HOST_SRCDIR
# corresponding to host-mkdep.
#
realinstall: install.host
install.host: ${HOST_BINDIR}/${HOSTPROGNAME}
${HOST_BINDIR}/${HOSTPROGNAME}:: ${HOSTPROG}
	${_MKTARGET_INSTALL}
	mkdir -p ${HOST_BINDIR}
	${HOST_INSTALL_FILE} -m ${BINMODE} ${HOSTPROG}${HOSTEXEEXT} ${.TARGET}

.if ${MKUPDATE} == "no"
.PHONY:		${HOST_BINDIR}/${HOSTPROGNAME}
.endif
