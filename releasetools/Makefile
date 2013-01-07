# Makefile for the kernel image.
.include <bsd.own.mk>
.include "nbsd.config"

.include <bsd.own.mk>
.include <bsd.sys.mk>

GEN_FILES=	*.bak kernel *.iso *.iso.gz src

# Specify the programs that are part of the system image, and its source.
KERNEL=		kernel	${NETBSDSRCDIR}/kernel
# PROGRAMS are in the order they should be loaded by boot
PROGRAMS=	ds	${NETBSDSRCDIR}/servers/ds
PROGRAMS+=	rs	${NETBSDSRCDIR}/servers/rs
PROGRAMS+=	pm	${NETBSDSRCDIR}/servers/pm
PROGRAMS+=	sched	${NETBSDSRCDIR}/servers/sched
PROGRAMS+=	vfs	${NETBSDSRCDIR}/servers/vfs
PROGRAMS+=	memory	${NETBSDSRCDIR}/drivers/memory
.if ${MACHINE_ARCH} == "i386"
PROGRAMS+=	log	${NETBSDSRCDIR}/drivers/log
.endif
PROGRAMS+=	tty	${NETBSDSRCDIR}/drivers/tty
PROGRAMS+=	mfs	${NETBSDSRCDIR}/servers/mfs
PROGRAMS+=	vm	${NETBSDSRCDIR}/servers/vm
PROGRAMS+=	pfs	${NETBSDSRCDIR}/servers/pfs
PROGRAMS+=	init	${NETBSDSRCDIR}/servers/init

usage:	
	@echo " " >&2
	@echo "Master Makefile to create new MINIX configuration." >& 2
	@echo "Root privileges are required." >&2
	@echo " " >&2
	@echo "Usage:" >&2
	@echo "	make includes   # Install include files" >&2
	@echo "	make depend     # Generate dependency files" >&2
	@echo "	make services   # Compile and install all services" >&2
	@echo "	make install    # Make image, and install to hard disk" >&2
	@echo "	make hdboot     # Make image, and install to hard disk" >&2
	@echo "	make nbsd_fetch	# Download current NetBSD reference sources" >&2
	@echo "	make nbsd_diff	# Update minix-port.patch in NetBSD sources" >&2
	@echo "	make clean      # Remove all compiler results, except libs" >&2
	@echo " " >&2
	@echo "To create a fresh MINIX configuration, try:" >&2
	@echo "	make clean install      # new boot image" >&2
	@echo " " >&2

all: services

# rebuild the program or system libraries
includes:
	$(MAKE) -C ../ includes

depend: includes .gitignore
	$(MAKE) -C ../ depend

.gitignore: Makefile
	echo $(GEN_FILES) | tr ' ' '\n' >.gitignore

services: includes kernel servers .WAIT drivers

kernel: includes
	$(MAKE) -C ../kernel 

servers: includes
	$(MAKE) -C ../servers all install

drivers: includes servers
	$(MAKE) -C ../drivers all install

# make bootable and place system images
hdboot: services .WAIT do-hdboot

# Where to put the new image
.if ${MKINSTALLBOOT:Uno} != "no" && \
   (!defined(DESTDIR) || ${DESTDIR} == "" || ${DESTDIR} == "/")
# should be in the same file system as final destination
_tempDIR=	${DESTDIR}/boot/minix/.temp

# XXX This stuff does not work with MKUNPRIVED=yes, with wrong writes to
# METALOG because of gzip and the rename done as part of mkboot below.
MINIX_INSTALL_FILE= ${INSTALL_FILE} -o ${BINOWN} -g ${BINGRP} -m 640 -s

.else # cross-compilation case:

_tempDIR=	${DESTDIR}/multiboot
MINIX_INSTALL_FILE= ${INSTALL_FILE} -o ${BINOWN} -g ${BINGRP} \
		-m ${NONBINMODE} ${STRIPFLAG}
.endif

##### Detect where are built the services
.for _bin _dir in ${KERNEL} ${PROGRAMS}
.if !defined(_OBJD.${_bin})
_OBJD.${_bin}!=	cd ${_dir} && ${PRINTOBJDIR}
.endif
.endfor

##### Names of the programs as they appear in the system image
# "mod%.2d" is used to make alphabetical order equal to the boot order
n:=	0
.for _bin _dir in ${PROGRAMS}
n!=	expr $n + 1
_pfx!=	printf "mod%.2d" $n
_MODNAME.${_bin}:=	${_tempDIR}/${_pfx}_${_bin}
system-modules: install-module-${_bin}
install-module-${_bin}:
	#${_MKMSG_INSTALL} ${_bin}
	@${MINIX_INSTALL_FILE} ${_OBJD.${_bin}}/${_bin} ${_MODNAME.${_bin}}
.endfor

do-hdboot: boot-dir .WAIT system-modules .WAIT make-bootable

boot-dir:
	@rm -rf ${_tempDIR}/
	${INSTALL_DIR} ${_tempDIR}

system-modules:
	#${_MKMSG_INSTALL} kernel
	@${MINIX_INSTALL_FILE} ${_OBJD.kernel}/kernel ${_tempDIR}/

# Invent a MKBOOTABLE knob!
make-bootable:
.if ${MKINSTALLBOOT:Uno} != "no" && \
   (!defined(DESTDIR) || ${DESTDIR} == "" || ${DESTDIR} == "/")
	# XXX stripping is already done as part of MINIX_INSTALL_FILE above
	${STRIP} -s ${_tempDIR}/*
	${TOOL_GZIP} ${_tempDIR}/mod*
	${HOST_SH} ${.CURDIR}/mkboot hdboot ${DESTDIR}
	${HOST_SH} ${.CURDIR}/../commands/update_bootcfg/update_bootcfg.sh
.endif

install:
	${MAKE} includes services hdboot

# download and update NetBSD reference sources.
nbsd_fetch:
	export CVS_RSH=ssh; 							\
	export OLDPWD=`pwd`;							\
	echo "retrieving hierarchies from ${NBSD_CVSROOT}";			\
	IFS=,;									\
	cd ..;									\
	cat releasetools/nbsd_ports | grep -v '^#' | while read port ;		\
	do	set $$port;							\
		date=$$1; minixpath=$$2; origpath=$$3;				\
		if [ $$# -lt 3 ]; then origpath=$$2; fi;			\
		echo "retrieving $$origpath ..";				\
		cvs -q -d ${NBSD_CVSROOT} co -N -D "$$date UTC" -d nbsdsrc "src/$$origpath" ; \
	done;									\
	cd $${OLDPWD};

nbsd_diff:
	find .. -name minix-port.patch | xargs rm
	cat nbsd_ports | grep -v '^#' | \
	( cd .. && awk -F, '{ minixpath=$$2; origpath=$$3; if(NF < 3) { origpath=$$2; } system("sh releasetools/nbsd_diff.sh " \
	   "nbsdsrc/src/"origpath" "minixpath" "minixpath"/minix-port.patch");}' )
	find .. -name minix-port.patch | xargs wc -l | sort -n


# clean up compile results
clean:
	$(MAKE) -C ../kernel $@
	$(MAKE) -C ../servers $@
	$(MAKE) -C ../drivers $@
	rm -rf $(GEN_FILES)

cleandepend::
	$(MAKE) -C ../kernel $@
	$(MAKE) -C ../servers  $@
	$(MAKE) -C ../drivers  $@
