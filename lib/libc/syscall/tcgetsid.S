#include <machine/asm.h>

IMPORT(_tcgetsid)
ENTRY(tcgetsid)
	jmp	_C_LABEL(_tcgetsid)

