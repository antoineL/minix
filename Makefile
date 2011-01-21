# Master Makefile to compile everything in /usr/src except the system.

NOOBJ=	# defined; to avoid cd ${.OBJDIR}

# SUBDIR+= etc	# recursing through etc is done only upon request
		# (using etcfiles or etcforce)
SUBDIR+= include .WAIT lib .WAIT
# FIXME: the following subdirs should NOT be walked if just doing
# 'make includes'; doing so is just waste; probably the best way to deal
# with that overhead is to amend the includes target of the sub Makefiles.
SUBDIR+= commands man # share
SUBDIR+= kernel servers drivers
#SUBDIR+= boot
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
	$(MAKE) -C etc MKUPDATE=no install

world: bootstrap objdirs includes depend install etcforce

# Force the current set of *.mk files to be in place.
# This is probably useless when DESTDIR is set, or when make -mxxx is used...
bootstrap:
	$(MAKE) -m share/mk -C share/mk MKUPDATE=yes install
mkfiles: bootstrap	# compatibility

.if defined(MAKEOBJDIRPREFIX) || defined(MAKEOBJDIR) || ${MKOBJ:Uno} != "no"
objdirs: obj # Create the .OBJDIR directories
.else
objdirs:: # defined
.endif

# Force some system files to be reset at the end of world building
etcforce::
	$(MAKE) -C etc MKUPDATE=yes install

# Maintenance of GCC 'fixed' headers
MKHEADERS411=/usr/gnu/libexec/gcc/i386-pc-minix/4.1.1/install-tools/mkheaders
MKHEADERS443=/usr/gnu/libexec/gcc/i686-pc-minix/4.4.3/install-tools/mkheaders
MKHEADERS443_PKGSRC=/usr/pkg/gcc44/libexec/gcc/i686-pc-minix/4.4.3/install-tools/mkheaders

# updates the GCC-specific headers; unfortunately the GCC 'mkheaders' program
# touches many files, causing MAKE to rebuild the whole tree
# See bug #531
gnu-includes:
	if [ -f $(MKHEADERS411) ] ; \
	then SHELL=/bin/sh ${SUDO_SH:U${HOST_SH}} -e $(MKHEADERS411) ; fi
	if [ -f $(MKHEADERS443) ] ; \
	then ${SUDO_SH:U${HOST_SH}} -e $(MKHEADERS443) ; fi
	if [ -f $(MKHEADERS443_PKGSRC) ] ; \
	then ${SUDO_SH:U${HOST_SH}} -e $(MKHEADERS443_PKGSRC) ; fi

#includes: gnu-includes	# currently disabled, too much overhead

# Convenience targets:
# quickly updates the ACK (ie. default) headers; avoid overhead
ack-includes:
	$(MAKE) -C include includes
	$(MAKE) -C lib includes

libraries: includes
	$(MAKE) -C lib depend install

# obsolete targets; behaviour really depends upon ${CC} and/or ${COMPILER_TYPE}
gnu-libraries: includes gnu-includes
	CC=gcc COMPILER_TYPE=gnu $(MAKE) -C lib depend install
clang-libraries: includes gnu-includes
	CC=clang COMPILER_TYPE=gnu $(MAKE) -C lib depend install

.PHONY: usage world etcfiles bootstrap mkfiles objdirs etcforce
.PHONY: ack-includes libraries gnu-includes gnu-libraries clang-libraries

.if !empty(.TARGETS) && !make(usage)  # Avoid recursion if just giving information
.include <bsd.subdir.mk>
.endif
