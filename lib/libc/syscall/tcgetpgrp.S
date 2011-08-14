#include <machine/asm.h>

IMPORT(_tcgetpgrp)
ENTRY(tcgetpgrp)
	jmp	_C_LABEL(_tcgetpgrp)

