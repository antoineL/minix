# MINIX-specific servers/drivers options
.include <bsd.own.mk>

# Currently RS is not able to launch a dynamically-linked binary
LDSTATIC=-static

.include <minix.bootprog.mk>
