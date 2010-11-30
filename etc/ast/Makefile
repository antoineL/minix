# Makefile for user homedirs
#
# Doing a make install builds ...?

NOOBJ=	# defined; to avoid cd ${.OBJDIR}
.include <bsd.own.mk>

FILES=	.ashrc .ellepro.b1 .ellepro.e .exrc .profile

FILESDIR=	/usr/ast
FILESDIRALSO=	/root	# each will also be used as FILESDIR
FILESMODE=	640
FILESMODE./root=600

# Overriding bsd.files.mk to install in many places: .if !target(__fileinstall)
##### Basic targets
realinstall:	filesinstall
realall:	filesbuild

##### Default values
#FILESDIR=	/@JOKER@
FILESOWN?=	${BINOWN}
FILESGRP?=	${BINGRP}
FILESMODE?=	${NONBINMODE}

##### Build rules
filesbuild:
.PHONY:		filesbuild

##### Install rules
filesinstall::	# ensure existence
.PHONY:		filesinstall

configfilesinstall:: .PHONY

__fileinstall: .USE
	${_MKTARGET_INSTALL}
	${INSTALL_FILE} \
	    -o ${FILESOWN_${.ALLSRC:T}:U${FILESOWN.${FILESDIR}:U${FILESOWN}}} \
	    -g ${FILESGRP_${.ALLSRC:T}:U${FILESGRP.${FILESDIR}:U${FILESGRP}}} \
	    -m ${FILESMODE_${.ALLSRC:T}:U${FILESMODE.${FILESDIR}:U${FILESMODE}}} \
	    ${SYSPKGTAG} ${.ALLSRC} ${.TARGET}
.for d in $(FILESDIRALSO)
	${INSTALL_FILE} \
	    -o ${FILESOWN_${.ALLSRC:T}:U${FILESOWN.$d:U${FILESOWN}}} \
	    -g ${FILESGRP_${.ALLSRC:T}:U${FILESGRP.$d:U${FILESGRP}}} \
	    -m ${FILESMODE_${.ALLSRC:T}:U${FILESMODE.$d:U${FILESMODE}}} \
	    ${SYSPKGTAG} ${.ALLSRC} ${.TARGET:S,${FILESDIR},$d,}
.endfor

# .end of override .if !target(__fileinstall)

beforeinstall:
	@echo "Installing user homedirs.."

.include <bsd.prog.mk>
