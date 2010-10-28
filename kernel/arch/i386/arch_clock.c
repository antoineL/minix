
/* i386-specific clock functions. */

#include <machine/ports.h>
#include <minix/portio.h>

#include "kernel/kernel.h"

#include "kernel/clock.h"
#include "kernel/proc.h"
#include "kernel/interrupt.h"
#include <minix/u64.h>
#include "glo.h"
#include "profile.h"


#ifdef CONFIG_APIC
#include "apic.h"
#endif
#include "spinlock.h"

#define CLOCK_ACK_BIT   0x80    /* PS/2 clock interrupt acknowledge bit */

/* Clock parameters. */
#define COUNTER_FREQ (2*TIMER_FREQ) /* counter frequency using square wave */
#define LATCH_COUNT     0x00    /* cc00xxxx, c = channel, x = any */
#define SQUARE_WAVE     0x36    /* ccaammmb, a = access, m = mode, b = BCD */
                                /*   11x11, 11 = LSB then MSB, x11 = sq wave */
#define TIMER_FREQ  1193182    /* clock frequency for timer in PC and AT */
#define TIMER_COUNT(freq) (TIMER_FREQ/(freq)) /* initial value for counter*/

PRIVATE irq_hook_t pic_timer_hook;		/* interrupt handler hook */

PRIVATE unsigned probe_ticks;
PRIVATE u64_t tsc0, tsc1;
#define PROBE_TICKS	(system_hz / 10)

PRIVATE unsigned tsc_per_ms[CONFIG_MAX_CPUS];

/*===========================================================================*
 *				init_8235A_timer			     *
 *===========================================================================*/
PUBLIC int init_8253A_timer(const unsigned freq)
{
	/* Initialize channel 0 of the 8253A timer to, e.g., 60 Hz,
	 * and register the CLOCK task's interrupt handler to be run
	 * on every clock tick.
	 */
	outb(TIMER_MODE, SQUARE_WAVE);  /* run continuously */
	outb(TIMER0, (TIMER_COUNT(freq) & 0xff)); /* timer low byte */
	outb(TIMER0, TIMER_COUNT(freq) >> 8); /* timer high byte */

	return OK;
}

/*===========================================================================*
 *				stop_8235A_timer			     *
 *===========================================================================*/
PUBLIC void stop_8253A_timer(void)
{
	/* Reset the clock to the BIOS rate. (For rebooting.) */
	outb(TIMER_MODE, 0x36);
	outb(TIMER0, 0);
	outb(TIMER0, 0);
}

PRIVATE int calib_cpu_handler(irq_hook_t * UNUSED(hook))
{
	u64_t tsc;

	probe_ticks++;
	read_tsc_64(&tsc);


	if (probe_ticks == 1) {
		tsc0 = tsc;
	}
	else if (probe_ticks == PROBE_TICKS) {
		tsc1 = tsc;
	}

	/* just in case we are in an SMP single cpu fallback mode */
	BKL_UNLOCK();
	return 1;
}

PRIVATE void estimate_cpu_freq(void)
{
	u64_t tsc_delta;
	u64_t cpu_freq;

	irq_hook_t calib_cpu;

	/* set the probe, we use the legacy timer, IRQ 0 */
	put_irq_handler(&calib_cpu, CLOCK_IRQ, calib_cpu_handler);

	/* just in case we are in an SMP single cpu fallback mode */
	BKL_UNLOCK();
	/* set the PIC timer to get some time */
	intr_enable();

	/* loop for some time to get a sample */
	while(probe_ticks < PROBE_TICKS) {
		intr_enable();
	}

	intr_disable();
	/* just in case we are in an SMP single cpu fallback mode */
	BKL_LOCK();

	/* remove the probe */
	rm_irq_handler(&calib_cpu);

	tsc_delta = sub64(tsc1, tsc0);

	cpu_freq = mul64(div64u64(tsc_delta, PROBE_TICKS - 1), make64(system_hz, 0));
	cpu_set_freq(cpuid, cpu_freq);
	cpu_info[cpuid].freq = div64u(cpu_freq, 1000000);
	BOOT_VERBOSE(cpu_print_freq(cpuid));
}

PUBLIC int init_local_timer(unsigned freq)
{
#ifdef CONFIG_APIC
	/* if we know the address, lapic is enabled and we should use it */
	if (lapic_addr) {
		unsigned cpu = cpuid;
		tsc_per_ms[cpu] = div64u(cpu_get_freq(cpu), 1000);
		lapic_set_timer_one_shot(1000000/system_hz);
	} else
	{
		BOOT_VERBOSE(printf("Initiating legacy i8253 timer\n"));
#else
	{
#endif
		init_8253A_timer(freq);
		estimate_cpu_freq();
		/* always only 1 cpu in the system */
		tsc_per_ms[0] = div64u(cpu_get_freq(0), 1000);
	}

	return 0;
}

PUBLIC void stop_local_timer(void)
{
#ifdef CONFIG_APIC
	if (lapic_addr) {
		lapic_stop_timer();
		apic_eoi();
	} else
#endif
	{
		stop_8253A_timer();
	}
}

PUBLIC void restart_local_timer(void)
{
#ifdef CONFIG_APIC
	if (lapic_addr) {
		lapic_restart_timer();
	}
#endif
}

PUBLIC int register_local_timer_handler(const irq_handler_t handler)
{
#ifdef CONFIG_APIC
	if (lapic_addr) {
		/* Using APIC, it is configured in apic_idt_init() */
		BOOT_VERBOSE(printf("Using LAPIC timer as tick source\n"));
	} else
#endif
	{
		/* Using PIC, Initialize the CLOCK's interrupt hook. */
		pic_timer_hook.proc_nr_e = NONE;
		pic_timer_hook.irq = CLOCK_IRQ;

		put_irq_handler(&pic_timer_hook, CLOCK_IRQ, handler);
	}

	return 0;
}

PUBLIC void cycles_accounting_init(void)
{
	unsigned cpu = cpuid;

	read_tsc_64(get_cpu_var_ptr(cpu, tsc_ctr_switch));

	make_zero64(get_cpu_var(cpu, cpu_last_tsc));
	make_zero64(get_cpu_var(cpu, cpu_last_idle));
}

PUBLIC void context_stop(struct proc * p)
{
	unsigned cpu = cpuid;
	u64_t tsc, tsc_delta;
	u64_t * __tsc_ctr_switch = get_cpulocal_var_ptr(tsc_ctr_switch);

#ifdef CONFIG_SMP
	/*
	 * This function is called only if we switch from kernel to user or idle
	 * or back. Therefore this is a perfect location to place the big kernel
	 * lock which will hopefully disappear soon.
	 *
	 * If we stop accounting for KERNEL we must unlock the BKL. If account
	 * for IDLE we must not hold the lock
	 */
	if (p == proc_addr(KERNEL)) {
		u64_t tmp;

		read_tsc_64(&tsc);
		tmp = sub64(tsc, *__tsc_ctr_switch);
		kernel_ticks[cpu] = add64(kernel_ticks[cpu], tmp);
		p->p_cycles = add64(p->p_cycles, tmp);
		BKL_UNLOCK();
	} else {
		u64_t bkl_tsc;
		atomic_t succ;
		
		read_tsc_64(&bkl_tsc);
		/* this only gives a good estimate */
		succ = big_kernel_lock.val;
		
		BKL_LOCK();
		
		read_tsc_64(&tsc);

		bkl_ticks[cpu] = add64(bkl_ticks[cpu], sub64(tsc, bkl_tsc));
		bkl_tries[cpu]++;
		bkl_succ[cpu] += !(!(succ == 0));

		p->p_cycles = add64(p->p_cycles, sub64(tsc, *__tsc_ctr_switch));
	}
#else
	read_tsc_64(&tsc);
	p->p_cycles = add64(p->p_cycles, sub64(tsc, *__tsc_ctr_switch));
#endif
	
	tsc_delta = sub64(tsc, *__tsc_ctr_switch);

	/*
	 * deduct the just consumed cpu cycles from the cpu time left for this
	 * process during its current quantum. Skip IDLE and other pseudo kernel
	 * tasks
	 */
	if (p->p_endpoint >= 0) {
#if DEBUG_RACE
		make_zero64(p->p_cpu_time_left);
#else
		/* if (tsc_delta < p->p_cpu_time_left) in 64bit */
		if (tsc_delta.hi < p->p_cpu_time_left.hi ||
				(tsc_delta.hi == p->p_cpu_time_left.hi &&
				 tsc_delta.lo < p->p_cpu_time_left.lo))
			p->p_cpu_time_left = sub64(p->p_cpu_time_left, tsc_delta);
		else {
			make_zero64(p->p_cpu_time_left);
		}
#endif
	}

	*__tsc_ctr_switch = tsc;
}

PUBLIC void context_stop_idle(void)
{
	int is_idle;
	unsigned cpu = cpuid;
	
	is_idle = get_cpu_var(cpu, cpu_is_idle);
	get_cpu_var(cpu, cpu_is_idle) = 0;

	context_stop(get_cpulocal_var_ptr(idle_proc));

	if (is_idle)
		restart_local_timer();

	if (sprofiling)
		get_cpulocal_var(idle_interrupted) = 1;
}

PUBLIC u64_t ms_2_cpu_time(unsigned ms)
{
	return mul64u(tsc_per_ms[cpuid], ms);
}

PUBLIC unsigned cpu_time_2_ms(u64_t cpu_time)
{
	return div64u(cpu_time, tsc_per_ms[cpuid]);
}

PUBLIC short cpu_load(void)
{
	u64_t current_tsc, *current_idle;
	u64_t tsc_delta, idle_delta, busy;
	struct proc *idle;
	short load;
	unsigned cpu = cpuid;

	u64_t *last_tsc, *last_idle;

	last_tsc = get_cpu_var_ptr(cpu, cpu_last_tsc);
	last_idle = get_cpu_var_ptr(cpu, cpu_last_idle);

	idle = get_cpu_var_ptr(cpu, idle_proc);;
	read_tsc_64(&current_tsc);
	current_idle = &idle->p_cycles; /* ptr to idle proc */

	/* calculate load since last cpu_load invocation */
	if (!is_zero64(*last_tsc)) {
		tsc_delta = sub64(current_tsc, *last_tsc);
		idle_delta = sub64(*current_idle, *last_idle);

		busy = sub64(tsc_delta, idle_delta);
		busy = mul64(busy, make64(100, 0));
		load = div64(busy, tsc_delta).lo;

		if (load > 100)
			load = 100;
	} else
		load = 0;
	
	*last_tsc = current_tsc;
	*last_idle = *current_idle;
	return load;
}
