# Generate binary keymaps.

.include <bsd.own.mk>

FILES=	dvorak.map french.map german.map italian.map japanese.map \
	latin-america.map olivetti.map polish.map russian-cp866.map \
	russian.map scandinavian.map spanish.map uk.map \
	us-std-esc.map us-std.map us-swap.map russian-cp1251.map \
	ukraine-koi8-u.map

FILESDIR= /usr/lib/keymaps

.SUFFIXES:      .src .map
.src.map:
	${_MKTARGET_CREATE}
	$(HOST_LINK.c) -DKEYSRC=\"${.IMPSRC}\" \
		-o ${.IMPSRC:T}_genmap ${.CURDIR}/genmap.c
	echo $$?
	${.OBJDIR}/${.IMPSRC:T}_genmap > $@
	echo $$?
	#@rm -f ${.IMPSRC:T}_genmap

realall: ${FILES}

.for _MAP in ${FILES}
${_MAP}: ${_MAP:C/map$/src/} genmap.c
.endfor

CLEANFILES+= ${FILES} ${FILES:C/map$/src_genmap/}

.include <bsd.prog.mk>
