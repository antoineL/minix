/*	getprocessor() - determine processor type	Author: Kees J. Bot */
/*								26 Jan 1994 */
#include <machine/asm.h>

/* int getprocessor(void); */
/*	Return 586 or 686; assume Pentium processor! */
ENTRY(getprocessor)
	push	%ebx
	movl	$1, %eax
	cpuid
	andb	$0x0F, %ah	/* Extract the family (5, 6, ...) */
	movzbl	%ah, %eax
	cmpl	$15, %eax	/* 15: extended family */
	jne	direct
	movl	$6, %eax	/* Make it 686 */
direct:
	imull	$100, %eax	/* 500, 600 */
	addl	$86, %eax	/* 586, 686 */
	pop	%ebx
	ret

