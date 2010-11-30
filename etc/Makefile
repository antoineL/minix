# Makefile for system configuration files
#
# Doing 'make install' with MKUPDATE=no (default) installs a pristine tree
# Doing 'make MKUPDATE=yes install' just updates some key files

NOOBJ=	# defined; to avoid cd ${.OBJDIR}
# Compatibility
.if target(installforce)
MKUPDATE?= yes
.endif

.include <bsd.own.mk>

.if ${MKUPDATE} == "no"
# Do not reset the tree if just updating (or do we?)
SUBDIR+=	mtree
.endif
SUBDIR+=	ast fonts

FILESDIR=	/etc
FILESMODE=	755

.if ${MKUPDATE} != "no"
# Just updating the important configuration files to the newest content:
FILES=	rc rs.inet rs.single
.else
# Install from scratch
FILES=	fstab group hostname.file inet.conf motd.install mtab passwd profile \
	protocols rc services termcap ttytab utmp rc.cd binary_sizes \
	binary_sizes.big binary_sizes.xxl syslog.conf rc.daemons.dist \
	rs.inet rs.single make.conf pkgsrc.conf
.endif

# Always does the following
FILES+=	system.conf 
FILESMODE_system.conf=		644

.if ${MKUPDATE} == "no"
FILES+=	shadow
FILESMODE_shadow=		600
.endif

.if ${MKUPDATE} != "no"
# Just updating the important configuration file:
FILES+=	usr/rc 
.else
# Install from scratch
FILES+=	usr/daily usr/dhcptags.conf usr/rc 
.endif
FILESDIR_usr/daily=		/usr/etc
FILESDIR_usr/dhcptags.conf=	/usr/etc
FILESDIR_usr/rc=		/usr/etc

# Always does the following too
FILES+=	usr/Makefile
FILESDIR_usr/Makefile=		/usr
FILESMODE_usr/Makefile=		644

.if ${MKUPDATE} == "no"
FILES+=	crontab
FILESDIR_crontab=		/usr/lib
.endif

.if ${MKUPDATE} == "no"
# Do not reset the devices if just updating
realinstall: devices
devices:
	@echo "Making devices.."
	cd ${DESTDIR}/dev \
	&& ${SUDO_SH:U${HOST_SH}} ${.CURDIR}/../commands/MAKEDEV/MAKEDEV.sh null \
	&& ( ${SUDO_SH:U${HOST_SH}} ${.CURDIR}/../commands/MAKEDEV/MAKEDEV.sh std 2>/dev/null )

.PHONY: devices
.endif

# Compatibility
installforce:: realinstall

.include <bsd.subdir.mk>
.include <bsd.prog.mk>
