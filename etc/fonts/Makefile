# Makefile for etc/fonts
#
# Doing a make install builds ...?

NOOBJ=	# defined; to avoid cd ${.OBJDIR}

FILESDIR=	/usr/lib/fonts
FILESMODE=	644

FILES=	cp1251.fnt cp437.fnt cp850.fnt cp865.fnt cp866.fnt \
	iso1.fnt koi8-r.fnt koi8-u.fnt polish.fnt

beforeinstall:
	@echo "Installing fonts.."

.include <bsd.prog.mk>
