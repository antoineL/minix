.PATH: ${NETBSDSRCDIR}/lib/libcompat_minix/include

INCSDIR= /usr/include

INCS+= net/gen/netdb.h net/gen/resolv.h net/gen/nameser.h
INCS+= sys/jmp_buf.h ${MACHINE}/compat_jmp_buf.h
