#include <machine/asm.h>

IMPORT(_setpgid)
ENTRY(setpgid)
	jmp	_C_LABEL(_setpgid)

