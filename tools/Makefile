# Makefile for the kernel image.
#
# Can be used both as standalone Makefile, or
# .included by another Makefile to provide the traditionnal
# behaviour of a BSD Makefile, creating a 'program' which
# in this case is the MINIX image. Only the .included variant
# actually builds something; the standalone variant just
# spawns the relevant Makefile, including this one id needed.

PROG=	image

# FIXME: what should do make install when invoked as submake?
# should it invoke hdboot? but what about DESTDIR then?
#BINDIR=	/boot/image
# extract from hdboot the logic to build the name?
#PROGNAME=	3.1.9r4
MAN=	#none

#u=/usr
#MDEC=	/usr/mdec

# Specify the programs that are part of the system image.
PROGRAMS=	kernel \
	../servers/ds/ds \
	../servers/rs/rs \
	../servers/pm/pm \
	../servers/sched/sched \
	../servers/vfs/vfs \
	../drivers/memory/memory \
	../drivers/log/log \
	../drivers/tty/tty \
	../servers/mfs/mfs \
	../servers/vm/vm \
	../servers/pfs/pfs \
	../servers/init/init

TOOLS_INSTALLBOOT?=	installboot
TOOLS_PADTEXT?=		padtext
SUDO_SH?=		exec su root

.if ${.MAKE.LEVEL} != 0
# for fast compile kernel and generate image, skip servers and drivers
image_mb: kernel
	${_MKTARGET_LINK}
	$(TOOLS_INSTALLBOOT) -image $@ $(PROGRAMS)

#realall:
# Override the normal rule to make a program
image:
	${_MKTARGET_LINK}
	$(TOOLS_INSTALLBOOT) -image $@ $(PROGRAMS)

#.PRECIOUS: image

image.d:
	${_MKTARGET_CREATE}
	echo>$@ image: $(PROGRAMS)

# Pad the kernel text only if ACK compiler is used. padtext does not work for
# gnu-like compilers as they generate common I&D and a linker script can do the
# same in a nicer way
kernel:
	$(MAKE) -C ../kernel 
.if ${COMPILER_TYPE} == "gnu"
	${HOST_INSTALL_FILE} ../kernel/kernel kernel
.else
	$(TOOLS_PADTEXT) ../kernel/kernel kernel
.endif

CLEANFILES+=	image_mb kernel

.include <bsd.prog.mk>

.else # .if ${.MAKE.LEVEL} == 0

usage:	
	@echo " " >&2
	@echo "Master Makefile to create new MINIX configuration." >& 2
	@echo "Root privileges are required." >&2
	@echo " " >&2
	@echo "Usage:" >&2
	@echo "	make includes   # Install include files" >&2
	@echo "	make depend     # Generate dependency files" >&2
	@echo "	make libraries  # Make system libraries" >&2
	@echo "	make services   # Compile and install all services" >&2
	@echo "	make image      # Make needed services and create boot image" >&2
	@echo "	make install    # Make image, and install to hard disk" >&2
	@echo "	make hdboot     # Make image, and install to hard disk" >&2
	@echo "	make fdboot     # Make image, and install to floppy disk" >&2
	@echo "	make bootable   # Make hard disk bootable" >&2
	@echo "	make clean      # Remove all compiler results, except libs" >&2
	@echo " " >&2
	@echo "To create a fresh MINIX configuration, try:" >&2
	@echo "	make clean install      # new boot image" >&2
	@echo "	make fresh install      # new everything" >&2
	@echo " " >&2

all: services image

# create a fresh configuration or system image
fresh: cleanlibs clean libraries services

# rebuild the program or system libraries
# FIXME: does not rebuild he GCC 'fixed' includes
includes:
	$(MAKE) -C ../ ack-includes

depend: includes
	$(MAKE) -C ../ depend

cleanlibs:
# useless: done as part of the libraries target below
#	$(MAKE) -C ../lib clean

# CHECK: obj before clean, or the reverse as it was?
# CHECK: what if MAKEOBJDIR is not set? cf. ../Makefile, objdirs:
libraries: includes
	$(MAKE) -C ../lib obj clean depend all install

# Old version: did not handle ${COMPILER_TYPE} == "gnu"
#	cd ../lib && sh ack_build.sh clean obj depend all install


image_mb: includes .MAKE
	$(MAKE) $@
image: includes services .MAKE
	$(MAKE) all

services: includes kernel servers .WAIT drivers

kernel: includes .MAKE
	$(MAKE) $@

servers: includes
	$(MAKE) -C ../servers all install

# why servers?
drivers: includes #servers
	$(MAKE) -C ../drivers all install

# make bootable and place system images
bootable:
	${SUDO_SH:U${HOST_SH}} ${.CURDIR}/mkboot bootable

hdboot:	image
	${SUDO_SH:U${HOST_SH}} ${.CURDIR}/mkboot $@
	@sync

fdboot:	image
	${SUDO_SH:U${HOST_SH}} ${.CURDIR}/mkboot $@
	@sync

install: includes services hdboot

# clean up compile results
clean:
	$(MAKE) -C ../kernel $@
	$(MAKE) -C ../servers $@
	$(MAKE) -C ../drivers $@
	$(MAKE) $@
	rm -rf *.bak image kernel *.iso *.iso.gz cdfdimage rootimage src
# consider CLEANFILES...

cleandepend::
	$(MAKE) -C ../kernel $@
	$(MAKE) -C ../servers  $@
	$(MAKE) -C ../drivers  $@
	$(MAKE) cleandir


.PHONY: all fresh includes depend cleanlibs libraries
.PHONY: services servers drivers bootable hdboot fdboot install
.PHONY: clean cleandepend

.endif 	# if ${.MAKE.LEVEL} != 0
