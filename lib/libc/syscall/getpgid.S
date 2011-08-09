#include <machine/asm.h>

IMPORT(_getpgid)
ENTRY(getpgid)
	jmp	_C_LABEL(_getpgid)

