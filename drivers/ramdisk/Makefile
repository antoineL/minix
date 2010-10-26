# Makefile for ramdisk image

PROGRAMS=ahci at_wini bios_wini cdprobe dev2name floppy loadramdisk mount \
	newroot pci sh service sysenv mfs ext2

# acpi is not compiled with ack
.if ${COMPILER_TYPE} == "gnu"
PROGRAMS += acpi
.endif

EXTRA=system.conf passwd rs.single

CPPFLAGS+= -I${MINIXSRCDIR}/servers -I${MINIXSRCDIR}
TOOLS_MKFS?=	mkfs.mfs
MAKEDEV=/usr/bin/MAKEDEV

STRIPFLAG=	-s

install: all

all:	image.c

clean:
	rm -rf $(PROGRAMS) $(EXTRA) bintoc image image.c t proto.gen

image.c:	bintoc image
	${_MKTARGET_CREATE}
	./bintoc -o $@ image

# Note for cross compilation: this executable has to be compiled for the
# host system
bintoc:	bintoc.c
	${_MKTARGET_BUILD}
	${HOST_LINK.c} -o $@ ${.ALLSRC}

image:	proto.gen ${.OBJDIR}/mtab ${.OBJDIR}/rc $(PROGRAMS) $(EXTRA)
	$(TOOLS_MKFS) image proto.gen || { rm -f image; false; }

ahci: ../ahci/ahci
	${_MKTARGET_LINK}
	${HOST_INSTALL_FILE} ${STRIPFLAG} $? $@

../ahci/ahci:
	$(MAKE) -C ../ahci
	# can also be written as (either)
	# $(MAKE) -C ${.TARGET:H}
	# $(MAKE) -C ${@D}

at_wini: ../at_wini/at_wini
	${_MKTARGET_LINK}
	${HOST_INSTALL_FILE} ${STRIPFLAG} $? $@

../at_wini/at_wini:
	$(MAKE) -C ../at_wini

bios_wini: ../bios_wini/bios_wini
	${_MKTARGET_LINK}
	${HOST_INSTALL_FILE} ${STRIPFLAG} $? $@

../bios_wini/bios_wini:
	$(MAKE) -C ../bios_wini

floppy: ../floppy/floppy
	${_MKTARGET_LINK}
	${HOST_INSTALL_FILE} ${STRIPFLAG} $? $@

../floppy/floppy:
	$(MAKE) -C ../floppy

acpi: ../acpi/acpi
	${_MKTARGET_LINK}
	${HOST_INSTALL_FILE} ${STRIPFLAG} $? $@

../acpi/acpi:
	$(MAKE) -C ../acpi

pci: ../pci/pci
	${_MKTARGET_LINK}
	${HOST_INSTALL_FILE} ${STRIPFLAG} $? $@

../pci/pci:
	$(MAKE) -C ../pci

cdprobe:  ../../commands/cdprobe/cdprobe
	${_MKTARGET_LINK}
	${HOST_INSTALL_FILE} ${STRIPFLAG} $? $@

../../commands/cdprobe:
	$(MAKE) -C ../../commands/cdprobe

dev2name:  ../../commands/dev2name/dev2name
	${_MKTARGET_LINK}
	${HOST_INSTALL_FILE} ${STRIPFLAG} $? $@

../../commands/dev2name/dev2name:
	$(MAKE) -C ../../commands/dev2name

loadramdisk:  ../../commands/loadramdisk/loadramdisk
	${_MKTARGET_LINK}
	${HOST_INSTALL_FILE} ${STRIPFLAG} $? $@

../../commands/loadramdisk/loadramdisk:
	$(MAKE) -C ../../commands/loadramdisk

mount:  ../../commands/mount/mount
	${_MKTARGET_LINK}
	${HOST_INSTALL_FILE} ${STRIPFLAG} $? $@

../../commands/mount/mount:
	$(MAKE) -C ../../commands/mount

newroot:  ../../commands/newroot/newroot.sh
	${_MKTARGET_LINK}
	${HOST_INSTALL_FILE} $? $@

../../commands/newroot/newroot:
	$(MAKE) -C ../../commands/newroot

sysenv:  ../../commands/sysenv/sysenv
	${_MKTARGET_LINK}
	${HOST_INSTALL_FILE} ${STRIPFLAG} $? $@

../../commands/sysenv:
	$(MAKE) -C ../../commands/sysenv

sh:	../../commands/ash/sh
	${_MKTARGET_LINK}
	${HOST_INSTALL_FILE} ${STRIPFLAG} $? $@
	#${INSTALL_LINK} -s ../../commands/ash/$@ $@

../../commands/ash/sh:
	$(MAKE) -C ../../commands/ash sh

service: ../../commands/service/service
	${_MKTARGET_LINK}
	${HOST_INSTALL_FILE} ${STRIPFLAG} $? $@
	#${INSTALL_LINK} -s ../../commands/service/$@ $@

../../commands/service:
	$(MAKE) -C ../../commands/service

mfs: ../../servers/mfs/mfs
	${_MKTARGET_LINK}
	${HOST_INSTALL_FILE} ${STRIPFLAG} $? $@
	#${INSTALL_LINK} -s ../../servers/mfs/$@ $@

../../servers/mfs/mfs:
	$(MAKE) -C ../../servers/mfs

ext2: ../../servers/ext2/ext2
	${_MKTARGET_LINK}
	${HOST_INSTALL_FILE} ${STRIPFLAG} $? $@
	#${INSTALL_LINK} -s ../../servers/ext2/$@ $@

../../servers/ext2/ext2:
	$(MAKE) -C ../../servers/ext2

system.conf: ../../etc/system.conf
	${_MKTARGET_LINK}
	${HOST_INSTALL_FILE} $? $@
	# ${HOST_INSTALL_SYMLINK} $? $@
	# cp $? $@
	# install -s ../../etc/$@ $@

passwd: ../../etc/passwd
	${_MKTARGET_LINK}
	${HOST_INSTALL_FILE} $? $@
	# ${HOST_INSTALL_SYMLINK} $? $@
	# cp $? $@
	# install -s ../../etc/$@ $@

rs.single: ../../etc/rs.single
	${_MKTARGET_LINK}
	${HOST_INSTALL_FILE} $? $@
	# ${HOST_INSTALL_SYMLINK} $? $@
	# cp $? $@
	# install -s ../../etc/$@ $@

${.OBJDIR}/mtab: mtab
	${_MKTARGET_LINK}
	${HOST_INSTALL_FILE} $? $@
	# ${HOST_INSTALL_SYMLINK} $? $@
	# cp $? $@

${.OBJDIR}/rc: rc
	${_MKTARGET_LINK}
	${HOST_INSTALL_FILE} $? $@
	# ${HOST_INSTALL_SYMLINK} $? $@
	# cp $? $@

proto.gen: $(PROGRAMS) proto.sh proto # $(MAKEDEV)
	${_MKTARGET_FORMAT}
	${HOST_SH} -e ${.CURDIR}/proto.sh ${.CURDIR}/proto >proto.gen

.include <bsd.prog.mk>
