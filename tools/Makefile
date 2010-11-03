# Makefile for the kernel image.

# ?
u=/usr
MDEC=	/usr/mdec

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

# Pad the kernel text only if ACK compiler is used. padtext does not work for
# gnu-like compilers as they generate common I&D and a linker script can do the
# same in a nicer way
.if ${COMPILER_TYPE} == "gnu"
PAD_KERNEL_TEXT := ${HOST_INSTALL_FILE} ../kernel/kernel kernel
.else
PAD_KERNEL_TEXT := padtext ../kernel/kernel kernel
.endif

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

Tall: services image
realall: image

# for fast complie kernel and generate image, skip servers and drivers
image_mb: includes
	cd ../kernel && $(MAKE)
	${PAD_KERNEL_TEXT}
	${_MKTARGET_LINK}
	$(TOOLS_INSTALLBOOT) -image $@ $(PROGRAMS)
	
Timage:  includes services
# PROG=
image: $(PROGRAMS)
	${_MKTARGET_LINK}
	$(TOOLS_INSTALLBOOT) -image $@ $(PROGRAMS)

.PRECIOUS: image

# rebuild the program or system libraries
Tincludes:
	$(MAKE) -C .. includes

Tdepend: includes
	$(MAKE) -C ../ depend

services: includes kernel servers .WAIT drivers

kernel: includes
	$(MAKE) -C ../kernel 
	${PAD_KERNEL_TEXT}

servers: includes
	$(MAKE) -C ../servers all install

drivers: includes servers
	$(MAKE) -C ../drivers all install

libraries: includes
	cd ../lib && sh ack_build.sh clean obj depend all install

# make bootable and place system images
bootable:
	exec su root ${.CURDIR}/mkboot bootable

hdboot:	image
	exec su root ${.CURDIR}/mkboot $@
	@sync

fdboot:	image
	exec su root ${.CURDIR}/mkboot $@
	@sync

Tinstall: includes services hdboot

# clean up compile results
Tclean:
	$(MAKE) -C ../kernel $@
	$(MAKE) -C ../servers $@
	$(MAKE) -C ../drivers $@
	rm -rf *.bak image kernel *.iso *.iso.gz cdfdimage rootimage src

Tcleandepend::
	$(MAKE) -C ../kernel $@
	$(MAKE) -C ../servers  $@
	$(MAKE) -C ../drivers  $@


.include <bsd.prog.mk>
