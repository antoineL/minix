# Makefile for destination tree
#
# Doing a make install builds nothing ?!

NOOBJ=	# defined; to avoid cd ${.OBJDIR}

# The tree should be in place before actually copying files;
# fortunately, the realinstall target in the src/etc directory
# is proceeded after install-subdir, that is, this realinstall

realinstall: mtree

SHFLAGS= -e
.if ${MAKEVERBOSE} >= 2
SHFLAGS+= -x
.endif

mtree: minix.tree
	@echo "Making hierarchy.."
	awk ' \
  NF==4 { print "${INSTALL_DIR} -m "$$1" -o "$$2" -g "$$3" ${DESTDIR}"$$4" " } \
  NF==3 { print "rm -f ${DESTDIR}"$$1" ; ${INSTALL_SYMLINK} "$$3" ${DESTDIR}"$$1" "  } \
	' ${.ALLSRC} | ${HOST_SH} ${SHFLAGS}

# make install should do the same as make includes
#realinstall: includes
# ... or the reverse? 
#includes: mtree

.PHONY: mtree

.include <bsd.prog.mk>
