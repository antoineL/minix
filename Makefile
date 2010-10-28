# Master Makefile to compile everything in /usr/src except the system.

NOOBJ=	# defined; to avoid cd ${.OBJDIR}

SUBDIR+= etc
SUBDIR+= include .WAIT lib .WAIT
SUBDIR+= boot commands man share
SUBDIR+= kernel servers drivers
SUBDIR+= tools

usage:
	@echo ""
	@echo "Master Makefile for MINIX commands and utilities."
	@echo "Root privileges are required for some actions."
	@echo ""
	@echo "Usage:"
	@echo "	make world       # Compile and install everything (libraries & commands)"
	@echo "	make includes    # Install include files from src/"
	#@echo "	make gnu-includes  # Install include files for GCC"
	@echo "	make libraries   # Compile and install libraries"
	#@echo "	make gnu-libraries # Compile and install libraries for GCC"
	#@echo "	make clang-libraries # Compile and install libraries for GCC with clang"
	@echo "	make all         # Compile all, commands and system, but do not install"
	@echo "	make install     # Recompile and install commands and system"
	@echo "	make depend      # Generate required .depend files"
	@echo "	make clean       # Remove all compiler results"
	@echo ""
	#@echo "Run 'make' in tools/ to create a new MINIX configuration."
	#@echo ""

# world has to be able to make a new system, even if there
# is no complete old system. it has to install commands, for which
# it has to install libraries, for which it has to install includes,
# for which it has to install /etc (for users and ownerships).
# etcfiles also creates a directory hierarchy in its
# 'make install' target.
#
# etcfiles has to be done first.

etcfiles::
	$(MAKE) -C etc install

world: bootstrap objdirs includes depend libraries install etcforce

# Force the current set of *.mk files to be in place.
# This is probably useless when DESTDIR is set, or when make -mxxx is used...
bootstrap:
	$(MAKE) -m share/mk -C share/mk includes
mkfiles: bootstrap	# compatibility

.if defined(MAKEOBJDIRPREFIX) || defined(MAKEOBJDIR) || ${MKOBJ:Uno} != "no"
objdirs: obj # Create the .OBJDIR directories
.else
objdirs:: # defined
.endif

# Force some system files to be reset at the end of world building
etcforce::
	$(MAKE) -C etc installforce

MKHEADERS411=/usr/gnu/libexec/gcc/i386-pc-minix/4.1.1/install-tools/mkheaders
MKHEADERS443=/usr/gnu/libexec/gcc/i686-pc-minix/4.4.3/install-tools/mkheaders
MKHEADERS443_PKGSRC=/usr/pkg/gcc44/libexec/gcc/i686-pc-minix/4.4.3/install-tools/mkheaders
#.if ${MAKEVERBOSE} >= 3
#VERBOSE_MKHEADERS?=	-v
#.endif

# quickly updates the ACK (ie. default) headers; avoid overhead
ack-includes:
	$(MAKE) -C include includes
	$(MAKE) -C lib includes

#includes: gnu-includes	# currently disabled, too much overhead

# updates the GCC-specific headers; unfortunately the GCC 'mkheaders'
# touches all the files, causing MAKE to rebuild the whole tree
# See bug #531
gnu-includes:
	if [ -f $(MKHEADERS411) ] ; then SHELL=/bin/sh sh -e $(MKHEADERS411) ; fi
	if [ -f $(MKHEADERS443) ] ; then sh -e $(MKHEADERS443) ; fi
	if [ -f $(MKHEADERS443_PKGSRC) ] ; \
	then sh -e $(MKHEADERS443_PKGSRC) $(VERBOSE_MKHEADERS:D-v) ; fi

libraries: includes
	$(MAKE) -C lib depend install

# obsolete targets; behaviour really depends upon ${CC} and/or ${COMPILER_TYPE}
gnu-libraries: includes gnu-includes
	$(MAKE) -C lib depend install
clang-libraries: includes gnu-includes
	$(MAKE) -C lib depend install

.PHONY: usage etcfiles world bootstrap mkfiles objdirs
.PHONY: ack-includes gnu-includes gnu-libraries clang-libraries

.if !empty(.TARGETS) && !make(usage)  # Avoid recursion if just giving information
.include <bsd.subdir.mk>
.endif
