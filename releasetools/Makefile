# Makefile for the kernel image.
.include <bsd.own.mk>

GEN_FILES=	*.bak *.iso *.iso.gz cdfdimage rootimage src

# LSC detect where were built the objects files
PROGROOT:= ..
.if "${MAKEOBJDIR:S,${.CURDIR},,}" != ""
PROGROOT:= ${MAKEOBJDIR:S,releasetools,,}
.endif

# Specify the programs that are part of the system image.
KERNEL_OBJDIR!=	cd ${NETBSDSRCDIR}/minix/kernel; ${PRINTOBJDIR}
KERNEL= ${KERNEL_OBJDIR}/kernel

# PROGRAMS, in the order they should be loaded by boot
PROGRAMS=
.for dir file in \
		minix/servers/ds	ds \
		minix/servers/rs	rs \
		minix/servers/pm	pm \
		minix/servers/sched	sched \
		minix/servers/vfs	vfs \
		minix/drivers/storage/memory memory \
		minix/drivers/tty/tty	tty \
		minix/servers/mib	mib \
		minix/servers/vm	vm \
		minix/fs/pfs		pfs \
		minix/fs/mfs		mfs \
		sbin/init		init
  _$(file)_OBJDIR!=	cd ${NETBSDSRCDIR}/$(dir); ${PRINTOBJDIR}
  PROGRAMS+= ${_$(file)_OBJDIR}/$(file)
.endfor


all usage help:
	@echo " " >&2
	@echo "Master Makefile to create new MINIX configuration." >&2
	@echo "Root privileges are required." >&2
	@echo " " >&2
	@echo "Usage:" >&2
	@echo "	make includes   # Install include files" >&2
	@echo "	make depend     # Generate dependency files" >&2
	@echo "	make services   # Compile and install all services" >&2
	@echo "	make hdboot     # Make image, and install to hard disk" >&2
	@echo "	make clean      # Remove all compiler results" >&2
	@echo " " >&2
	@echo "To create a fresh MINIX configuration, try:" >&2
	@echo "	make clean install      # new boot image" >&2
	@echo " " >&2

.gitignore: Makefile
	echo ${GEN_FILES} | tr ' ' '\n' >.gitignore

includes:
	${MAKE} -C ${NETBSDSRCDIR} includes

depend: includes .gitignore
	${MAKE} -C ${NETBSDSRCDIR} depend

libraries: includes
	${MAKE} -C ${NETBSDSRCDIR} do-lib

kernel: libraries
	${MAKE} -C ${NETBSDSRCDIR}/minix/kernel

servers: libraries
	${MAKE} -C ${NETBSDSRCDIR}/minix/fs all install
	${MAKE} -C ${NETBSDSRCDIR}/minix/net all install
	${MAKE} -C ${NETBSDSRCDIR}/minix/servers all install

sbin: libraries
	${MAKE} -C ${NETBSDSRCDIR}/sbin all install
	${MAKE} -C ${NETBSDSRCDIR}/minix/sbin all install

drivers: libraries
	${MAKE} -C ${NETBSDSRCDIR}/minix/drivers all install

services: kernel servers sbin .WAIT drivers

# AL20160822: gzip disabled becuase it breaks the distrib/sets building logic
do-hdboot: 
	@rm -rf ${DESTDIR}/boot/minix_latest
	${INSTALL_DIR} ${DESTDIR}/boot/minix_latest
# mod_0 is used to make alphabetical order equal to the boot order
	@n=0;							\
	for i in ${PROGRAMS};					\
	do							\
	n=`expr $$n + 1`;					\
	[ "$$n" -ge 10 ] && prefix="mod" || prefix="mod0";	\
	newname="${DESTDIR}/boot/minix_latest/$${prefix}$${n}_`basename $$i`"; \
	${INSTALL_FILE} $$i $$newname;				\
	echo ${INSTALL_FILE} $$i $$newname;			\
	done
	@${INSTALL_FILE} ${KERNEL} ${DESTDIR}/boot/minix_latest
	@if [ "${MKINSTALLBOOT:Uno}" != "no" ] ; then		\
	  ${STRIP} -s ${DESTDIR}/boot/minix_latest/* ;		\
	  : gzip ${DESTDIR}/boot/minix_latest/mod* ;		\
	fi

hdboot: services .WAIT do-hdboot

clean:
	${MAKE} -C ${NETBSDSRCDIR}/lib $@
	${MAKE} -C ${NETBSDSRCDIR}/minix/kernel $@
	${MAKE} -C ${NETBSDSRCDIR}/minix/fs $@
	${MAKE} -C ${NETBSDSRCDIR}/minix/net $@
	${MAKE} -C ${NETBSDSRCDIR}/minix/servers $@
	${MAKE} -C ${NETBSDSRCDIR}/minix/drivers $@
	${MAKE} -C ${NETBSDSRCDIR}/sbin $@
	${MAKE} -C ${NETBSDSRCDIR}/minix/sbin $@
	rm -rf ${GEN_FILES}

cleandir: clean
	rm -rf .gitignore

# LSC: For STRIP and HOST_SH
.include <bsd.sys.mk>
