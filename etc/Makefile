# Makefile for system configuration files
#
# Doing a make install builds ...?

NOOBJ=	# defined; to avoid cd ${.OBJDIR}

FILESDIR=	/etc
FILESMODE=	755

FILES=	fstab group hostname.file inet.conf motd.install mtab passwd profile \
	protocols rc services termcap ttytab utmp rc.cd binary_sizes \
	binary_sizes.big binary_sizes.xxl syslog.conf rc.daemons.dist \
	rs.inet rs.single make.conf pkgsrc.conf

FILES+=	system.conf 
FILESMODE_system.conf=		644

FILES+=	shadow
FILESMODE_shadow=		600

FILES+=	usr/daily usr/dhcptags.conf usr/rc 
FILESDIR_usr/daily=		/usr/etc
FILESDIR_usr/dhcptags.conf=	/usr/etc
FILESDIR_usr/rc=		/usr/etc

FILES+=	usr/Makefile
FILESDIR_usr/Makefile=		/usr
FILESMODE_usr/Makefile=		644

# Should have the tree before actually copying files
reallinstall: mtree

mtree0:
	sh mtree0.sh mtree/minix.tree

mtree: mtree/minix.tree
	@+cat ${.ALLSRC} | while read line ; do echo $line | awk ' \
	   NF==4 { print "${INSTALL_DIR} -m "$1" -o "$2" -g "$3" ${DESTDIR}"$4" || exit 1" } \
	   NF==3 { print "rm -f ${DESTDIR}"$1" ; ln -s "$3" ${DESTDIR}"$1" || exit 1"  } \
	' | sh -e || exit 1 ; done

.PHONY: mtree

# make install should do the same as make includes
#realinstall: includes 
#includes: mtree

.if 0
ETC=/etc/
USR=/usr/
USRETC=/usr/etc/
FILES1=fstab group hostname.file inet.conf motd.install mtab passwd profile \
	protocols rc services termcap ttytab utmp rc.cd binary_sizes \
	binary_sizes.big binary_sizes.xxl syslog.conf rc.daemons.dist \
	rs.inet rs.single make.conf pkgsrc.conf system.conf 
FILES2=shadow
FILES3=daily dhcptags.conf rc 
USRFILES=Makefile

install:
	@echo "Installing /etc and /usr/etc.."
	mkdir -p $(ETC)
	@for f in $(FILES1); do \
		if [ -f $(ETC)/$$f ]; \
		then :; \
		else cp $$f $(ETC)/$$f; \
			chmod 755 $(ETC)/$$f; \
		fi; \
	done
	@for f in $(FILES2); do \
		if [ -f $(ETC)/$$f ]; \
		then :; \
		else cp $$f $(ETC)/$$f; \
			chmod 600 $(ETC)/$$f; \
		fi; \
	done
	@for f in $(USRFILES); do \
		cp usr/$$f $(USR)/$$f; \
		chmod 644 $(USR)/$$f; \
	done
	@echo "Making hierarchy.."
	sh mtree.sh mtree/minix.tree
	@for f in $(FILES3); do \
		if [ -f $(USRETC)/$$f ]; \
		then :; \
		else cp usr/$$f $(USRETC); \
			chmod 755 $(USRETC)/$$f; \
		fi; \
	done
	@echo "Making devices.."
	p=`pwd` && cd /dev && sh $$p/../commands/MAKEDEV/MAKEDEV.sh null
	p=`pwd` && cd /dev && sh $$p/../commands/MAKEDEV/MAKEDEV.sh std  2>/dev/null
	install -o root -g operator -m 755 crontab /usr/lib
	@echo "Making user homedirs.."
	for u in /usr/ast ~root; do cp ast/.[aepv]* $$u ; done
	@echo "Installing fonts.."
	install -m 644 -o root -g operator fonts/*.fnt /usr/lib/fonts/

installforce:: $(ETC)/rc $(ETC)/rs.inet $(ETC)/rs.single $(ETC)/system.conf $(USRETC)/rc

$(ETC)/rc: rc
	install -m 755 -o root -g operator $> $@

$(ETC)/rs.inet: rs.inet
	install -m 755 -o root -g operator $> $@

$(ETC)/rs.single: rs.single
	install -m 755 -o root -g operator $> $@

$(ETC)/system.conf: system.conf
	install -m 644 -o root -g operator $> $@

$(USRETC)/rc: usr/rc
	install -m 755 -o root -g operator $> $@
.endif

.include <bsd.prog.mk>
