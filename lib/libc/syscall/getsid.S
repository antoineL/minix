#include <machine/asm.h>

IMPORT(_getsid)
ENTRY(getsid)
	jmp	_C_LABEL(_getsid)

