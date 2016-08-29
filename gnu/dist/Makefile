# Trigger the fetch phase even when not building tools

.include <bsd.own.mk>

MODULE=	texinfo
GNUHOSTDIST=	${.CURDIR}/${MODULE}

.include "${.CURDIR}/../../minix/Makefile.fetchgnu"

# We considere work to be done as soon as the fetch step is done,
# as everything else has to be triggered from /tools or ../usr.bin

.if ${USETOOLS} != "yes"
realdepend realall realinstall: ${fetch_done}
.endif

