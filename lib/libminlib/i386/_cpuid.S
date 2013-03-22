/* _cpuid() - interface to cpuid instruction */


/* void _cpuid(u32_t *eax, u32_t *ebx, u32_t *ecx, u32_t *edx); */
/* 0 for OK, nonzero for unsupported */

#include <machine/asm.h>

ENTRY(_cpuid)
/* save work registers */
	push	%ebx

/* set eax parameter to cpuid and execute cpuid */
	movl	8(%esp), %eax
	cpuid

/* store results in pointer arguments */
	movl	%eax, 8(%esp)
	movl	%ebx, 12(%esp)
	movl	%ecx, 16(%esp)
	movl	%edx, 20(%esp)

/* restore registers */
	pop	%ebx

	ret
