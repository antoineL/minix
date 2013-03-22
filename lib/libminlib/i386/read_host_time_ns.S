#include <machine/asm.h>

/**===========================================================================* */
/* PUBLIC void read_host_time_ns(unsigned long *high, unsigned long *low); */
/* access real time in ns from host in vmware. */
ENTRY(read_host_time_ns)
        pushl   %edx
        pushl   %eax
        pushl   %ecx
        movl    $0x10001, %ecx
	rdtsc
        pushl   %ebp
        movl    20(%esp), %ebp
        movl    %edx, (%ebp)
        movl    24(%esp), %ebp
        movl    %eax, (%ebp)
        popl    %ebp
        popl    %ecx
        popl    %eax
        popl    %edx
        ret
