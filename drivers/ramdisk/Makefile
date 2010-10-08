# Makefile for ramdisk image

PROGRAMS=ahci at_wini bios_wini cdprobe dev2name floppy loadramdisk mount \
	newroot pci sh service sysenv mfs ext2

# acpi is not compiled with ack
.if ${COMPILER_TYPE} == "gnu"
PROGRAMS += acpi
.endif

EXTRA=system.conf passwd rs.single

CPPFLAGS+= -I${MINIXSRCDIR}/servers -I${MINIXSRCDIR}
MAKEDEV=/usr/bin/MAKEDEV

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
	$(HOST_CC) -o $@ ${.ALLSRC}

image:	proto.gen ${.OBJDIR}/mtab ${.OBJDIR}/rc $(PROGRAMS) $(EXTRA)
	mkfs.mfs image proto.gen || { rm -f image; false; }

ahci: ../ahci/ahci
	${_MKTARGET_LINK}
	${INSTALL_LINK} -s $? $@
	# can also be written as (either)
	# ${INSTALL_LINK} -s $> $@
	# ${INSTALL_LINK} -s ${.ALLSRC} ${.TARGET}

../ahci/ahci:
	$(MAKE) -C ../ahci
	# can also be written as (either)
	# $(MAKE) -C ${.TARGET:H}
	# $(MAKE) -C ${@D}

at_wini: ../at_wini/at_wini
	${_MKTARGET_LINK}
	${INSTALL_LINK} -s $? $@

../at_wini/at_wini:
	$(MAKE) -C ../at_wini

bios_wini: ../bios_wini/bios_wini
	${_MKTARGET_LINK}
	${INSTALL_LINK} -s $? $@

../bios_wini/bios_wini:
	$(MAKE) -C ../bios_wini

floppy: ../floppy/floppy
	${_MKTARGET_LINK}
	${INSTALL_LINK} -s $? $@

../floppy/floppy:
	$(MAKE) -C ../floppy

acpi: ../acpi/acpi
	${_MKTARGET_LINK}
	${INSTALL_LINK} -s $? $@

../acpi/acpi:
	$(MAKE) -C ../acpi

pci: ../pci/pci
	${_MKTARGET_LINK}
	${INSTALL_LINK} -s $? $@

../pci/pci:
	$(MAKE) -C ../pci

cdprobe:  ../../commands/cdprobe/cdprobe
	${_MKTARGET_LINK}
	${INSTALL_LINK} -s $? $@

../../commands/cdprobe:
	$(MAKE) -C ../../commands/cdprobe

dev2name:  ../../commands/dev2name/dev2name
	${_MKTARGET_LINK}
	${INSTALL_LINK} -s $? $@

../../commands/dev2name/dev2name:
	$(MAKE) -C ../../commands/dev2name

loadramdisk:  ../../commands/loadramdisk/loadramdisk
	${_MKTARGET_LINK}
	${INSTALL_LINK} -s $? $@

../../commands/loadramdisk/loadramdisk:
	$(MAKE) -C ../../commands/loadramdisk

mount:  ../../commands/mount/mount
	${_MKTARGET_LINK}
	${INSTALL_LINK} -s $? $@

../../commands/mount/mount:
	$(MAKE) -C ../../commands/mount

newroot:  ../../commands/newroot/newroot.sh
	${_MKTARGET_LINK}
	${INSTALL_LINK} -s $?.sh $@

../../commands/newroot/newroot:
	$(MAKE) -C ../../commands/newroot

sysenv:  ../../commands/sysenv/sysenv
	${_MKTARGET_LINK}
	${INSTALL_LINK} -s $? $@

../../commands/sysenv:
	$(MAKE) -C ../../commands/sysenv

sh:	../../commands/ash/sh
	${_MKTARGET_LINK}
	${INSTALL_LINK} -s ../../commands/ash/$@ $@

../../commands/ash/sh:
	$(MAKE) -C ../../commands/ash sh

service: ../../commands/service/service
	${_MKTARGET_LINK}
	${INSTALL_LINK} -s ../../commands/service/$@ $@

../../commands/service:
	$(MAKE) -C ../../commands/service

mfs: ../../servers/mfs/mfs
	${_MKTARGET_LINK}
	${INSTALL_LINK} -s ../../servers/mfs/$@ $@

../../servers/mfs/mfs:
	$(MAKE) -C ../../servers/mfs

ext2: ../../servers/ext2/ext2
	${_MKTARGET_LINK}
	${INSTALL_LINK} -s ../../servers/ext2/$@ $@

../../servers/ext2/ext2:
	$(MAKE) -C ../../servers/ext2

system.conf: ../../etc/system.conf
	cp $? $@
	# install -s ../../etc/$@ $@

passwd: ../../etc/passwd
	cp $? $@
	# install -s ../../etc/$@ $@

rs.single: ../../etc/rs.single
	cp $? $@
	# install -s ../../etc/$@ $@

${.OBJDIR}/rc: rc
	cp $? $@

${.OBJDIR}/mtab: mtab
	cp $? $@

proto.gen: $(PROGRAMS) $(MAKEDEV) proto.sh proto
	${_MKTARGET_FORMAT}
	sh -e ${.CURDIR}/proto.sh ${.CURDIR}/proto >proto.gen

.include <bsd.prog.mk>
