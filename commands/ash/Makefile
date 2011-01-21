# Makefile for ash.

.include <bsd.own.mk>

YHEADER=1
PROG=	sh

SHSRCS=	alias.c arith.y arith_lex.l cd.c echo.c error.c eval.c exec.c expand.c \
	histedit.c input.c jobs.c mail.c main.c memalloc.c miscbltin.c \
	mystring.c options.c output.c parser.c redir.c show.c \
	trap.c var.c setmode.c expr.c regexp.c
GENSRCS= builtins.c init.c nodes.c syntax.c operators.c signames.c
GENHDRS= builtins.h nodes.h syntax.h token.h operators.h signames.h
SRCS= ${SHSRCS} ${GENSRCS}
DPSRCS+=${GENHDRS}
BINDIR=	/bin
MAN=

DPADD+= ${LIBL} ${LIBEDIT}
LDADD+= -ll -ledit

LFLAGS=	-8	# 8-bit lex scanner for arithmetic
YFLAGS=	-d

# The .depend file can get references to these temporary files
.OPTIONAL: lex.yy.c y.tab.c

# Enable this line to disable command line editing
#EDIT=-DNO_HISTORY
# Enable this line to use the editline library instead of libedit
EDIT=-DEDITLINE

# Enable this line if your system does not have a <paths.h>
#NO_PATHS_H=-DNO_PATHS_H

# Enable this if you don't want job control
NO_JOBS=-DJOBS=0
MKB_NO_JOBS=-j

CPPFLAGS+=-DSHELL -I. -I${.CURDIR}
CPPFLAGS+=${EDIT} ${NO_PATHS_H} ${NO_JOBS}
HOST_CPPFLAGS+=-DSHELL -I. -I${.CURDIR}
HOST_CPPFLAGS+=${EDIT} ${NO_PATHS_H} ${NO_JOBS}

.PATH:	${.CURDIR}/bltin

CLEANFILES+= mkinit mkinit.o mknodes mknodes.o \
	mksyntax mksyntax.o mksignames mksignames.o
CLEANFILES+= ${GENSRCS} ${GENHDRS} y.tab.h

build-tools: mkinit mknodes mksyntax mksignames

.ORDER: builtins.c builtins.h
builtins.c builtins.h: mkbuiltins builtins.def shell.h
	${_MKTARGET_CREATE}
	cd ${.CURDIR}; ${HOST_SH} mkbuiltins \
		${MKB_NO_JOBS} ${.OBJDIR} shell.h builtins.def

init.c: mkinit.c alias.c eval.c exec.c input.c jobs.c options.c parser.c \
		redir.c trap.c var.c
	${_MKMSG_COMPILE} mkinit
	${HOST_LINK.c} -o ${.OBJDIR}/mkinit ${.CURDIR}/mkinit.c
	${_MKTARGET_CREATE}
	${.OBJDIR}/mkinit ${.ALLSRC:C/.*mkinit.c$//}

.ORDER: nodes.c nodes.h
nodes.c nodes.h: mknodes.c nodetypes nodes.c.pat
	${_MKMSG_COMPILE} mksyntax
	${HOST_LINK.c} -o ${.OBJDIR}/mknodes ${.CURDIR}/mknodes.c
	${_MKTARGET_CREATE}
	${.OBJDIR}/mknodes ${.ALLSRC:C/.*mknodes.c$//}

.ORDER: syntax.c syntax.h
syntax.c syntax.h: mksyntax.c
	${_MKMSG_COMPILE} mksyntax
	${HOST_LINK.c} -o ${.OBJDIR}/mksyntax ${.CURDIR}/mksyntax.c
	${_MKTARGET_CREATE}
	${.OBJDIR}/mksyntax

token.h: mktokens
	${_MKTARGET_CREATE}
	${HOST_SH} ${.ALLSRC}

.ORDER: signames.c signames.h
signames.c signames.h: mksignames.c
	${_MKMSG_COMPILE} mksignames
	${HOST_LINK.c} -o ${.OBJDIR}/mksignames ${.CURDIR}/mksignames.c
	${_MKTARGET_CREATE}
	${.OBJDIR}/mksignames

.ORDER: operators.c operators.h
operators.c operators.h: mkexpr unary_op binary_op
	${_MKTARGET_CREATE}
	#sh ${.CURDIR}/bltin/mkexpr ${.CURDIR}/bltin/unary_op ${.CURDIR}/bltin/binary_op
	${HOST_SH} ${.ALLSRC}

arith.h: arith.c
arith.c: arith.y

.include <bsd.prog.mk>
