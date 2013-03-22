#include <machine/asm.h>

/**===========================================================================* */
/* PUBLIC void read_tsc(unsigned long *high, unsigned long *low); */
/* Read the cycle counter of the CPU. Pentium and up.  */
ENTRY(read_tsc)
	rdtsc	/* places the TSC in EDX:EAX */
	movl	4(%esp), %ecx
	movl	%edx, (%ecx)
	movl	8(%esp), %ecx
	movl	%eax, (%ecx)
	ret

