# MINIX-specific servers/drivers options
.include <bsd.own.mk>

.if ${COMPILER_TYPE} == "gnu"

.if !empty(CC:T:M*gcc)
LDADD+= -nodefaultlibs -lgcc -lsys -lgcc -lminc
.elif !empty(CC:T:M*clang)
LDADD+= -nodefaultlibs -L/usr/pkg/lib -lCompilerRT-Generic -lsys -lCompilerRT-Generic -lminc
.elif !empty(CC:T:M*pcc)
LDADD+= -nodefaultlibs -lsys -lminc
.endif

.endif

.include <bsd.prog.mk>
