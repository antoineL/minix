# Doing a make includes builds /usr/include

NOOBJ=	# defined; to avoid cd ${.OBJDIR}

INCS=	alloca.h ansi.h a.out.h ar.h assert.h configfile.h ctype.h \
	curses.h dirent.h env.h err.h errno.h fcntl.h fenv.h \
	float.h fnmatch.h fts.h getopt.h glob.h grp.h ifaddrs.h \
	inttypes.h libgen.h lib.h libutil.h limits.h locale.h \
	mathconst.h math.h midiparser.h netdb.h pwd.h regex.h \
	regexp.h setjmp.h sgtty.h signal.h stdarg.h stddef.h \
	stdint.h stdbool.h stdio.h stdlib.h string.h strings.h sysexits.h \
	syslog.h tar.h termcap.h termios.h time.h timers.h tools.h \
	ttyent.h ucontext.h unistd.h utime.h utmp.h wchar.h wctype.h \
	hgfs.h tzfile.h util.h fetch.h mpool.h ndbm.h db.h poll.h resolv.h \
	memory.h paths.h libexec.h

INCSDIR=        /usr/include

SUBDIR=		arpa net netinet minix net/gen sys arch

.include <bsd.kinc.mk>
#.include <bsd.subdir.mk>
