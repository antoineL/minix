#include <machine/asm.h>

IMPORT(_tcsetpgrp)
ENTRY(tcsetpgrp)
	jmp	_C_LABEL(_tcsetpgrp)

