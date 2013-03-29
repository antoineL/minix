.include <bsd.own.mk>		# for MKDYNAMICROOT definition

BINDIR?=/usr/bin
# Copied from /{,s}bin/Makefile.inc
.if ${MKDYNAMICROOT} == "no" && \
    (${BINDIR} == "/bin" || ${BINDIR} == "/sbin")
LDSTATIC?=	-static
.endif

# BJG too many warnings here
NOGCCERROR?=    yes
NOCLANGERROR?=  yes
