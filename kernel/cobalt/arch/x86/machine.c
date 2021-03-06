/**
 *   Copyright (C) 2007-2012 Philippe Gerum.
 *
 *   Xenomai is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License as
 *   published by the Free Software Foundation, Inc., 675 Mass Ave,
 *   Cambridge MA 02139, USA; either version 2 of the License, or (at
 *   your option) any later version.
 *
 *   Xenomai is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 *   02111-1307, USA.
 */

#include <cobalt/kernel/arith.h>
#include <asm/xenomai/machine.h>
#include <asm/xenomai/smi.h>

#ifdef __i386__

/*
 * Lifted from linux/arch/i386/lib/usercopy.c.
 *
 * Copyright 1997 Andi Kleen <ak@muc.de>
 * Copyright 1997 Linus Torvalds
 */

#define __do_strncpy_from_user(dst, src, count, res)			   \
do {									   \
	int __d0, __d1, __d2;						   \
	__asm__ __volatile__(						   \
		"	testl %1,%1\n"					   \
		"	jz 2f\n"					   \
		"0:	lodsb\n"					   \
		"	stosb\n"					   \
		"	testb %%al,%%al\n"				   \
		"	jz 1f\n"					   \
		"	decl %1\n"					   \
		"	jnz 0b\n"					   \
		"1:	subl %1,%0\n"					   \
		"2:\n"							   \
		".section .fixup,\"ax\"\n"				   \
		"3:	movl %5,%0\n"					   \
		"	jmp 2b\n"					   \
		".previous\n"						   \
		".section __ex_table,\"a\"\n"				   \
		"	.align 4\n"					   \
		"	.long 0b,3b\n"					   \
		".previous"						   \
		: "=&d"(res), "=&c"(count), "=&a" (__d0), "=&S" (__d1),	   \
		  "=&D" (__d2)						   \
		: "i"(-EFAULT), "0"(count), "1"(count), "3"(src), "4"(dst) \
		: "memory");						   \
} while (0)

#else /* __x86_64__ */

/*
 * Lifted from linux/arch/x86_64/lib/usercopy.c.
 *
 * Copyright 1997 Andi Kleen <ak@muc.de>
 * Copyright 1997 Linus Torvalds
 * Copyright 2002 Andi Kleen <ak@suse.de>
 */

#define __do_strncpy_from_user(dst, src, count, res)			   \
do {									   \
	long __d0, __d1, __d2;						   \
	__asm__ __volatile__(						   \
		"	testq %1,%1\n"					   \
		"	jz 2f\n"					   \
		"0:	lodsb\n"					   \
		"	stosb\n"					   \
		"	testb %%al,%%al\n"				   \
		"	jz 1f\n"					   \
		"	decq %1\n"					   \
		"	jnz 0b\n"					   \
		"1:	subq %1,%0\n"					   \
		"2:\n"							   \
		".section .fixup,\"ax\"\n"				   \
		"3:	movq %5,%0\n"					   \
		"	jmp 2b\n"					   \
		".previous\n"						   \
		".section __ex_table,\"a\"\n"				   \
		"	.align 8\n"					   \
		"	.quad 0b,3b\n"					   \
		".previous"						   \
		: "=&r"(res), "=&c"(count), "=&a" (__d0), "=&S" (__d1),	   \
		  "=&D" (__d2)						   \
		: "i"(-EFAULT), "0"(count), "1"(count), "3"(src), "4"(dst) \
		: "memory");						   \
} while (0)

#endif

long strncpy_from_user_nocheck(char *dst, const char __user *src, long count)
{
	long res;
	__do_strncpy_from_user(dst, src, count, res);
	return res;
}
EXPORT_SYMBOL_GPL(strncpy_from_user_nocheck);

static unsigned long mach_x86_calibrate(void)
{
	unsigned long delay = (xnarch_machdata.timer_freq + HZ / 2) / HZ;
	unsigned long long t0, t1, dt;
	unsigned long flags;
	int i;

	flags = ipipe_critical_enter(NULL);

	ipipe_timer_set(delay);

	ipipe_read_tsc(t0);

	for (i = 0; i < 100; i++)
		ipipe_timer_set(delay);

	ipipe_read_tsc(t1);
	dt = t1 - t0;

	ipipe_critical_exit(flags);

	/*
	 * Reset the max trace, since it contains the calibration time
	 * now.
	 */
	ipipe_trace_max_reset();

	/*
	 * Compute average with a 5% margin to avoid negative
	 * latencies with PIT.
	 */
	return xnarch_ulldiv(dt, i + 5, NULL);
}

static int mach_x86_init(void)
{
	mach_x86_smi_init();
	mach_x86_smi_disable();

	return 0;
}

static void mach_x86_cleanup(void)
{
	mach_x86_smi_restore();
}

static const char *const fault_labels[] = {
    [0] = "Divide error",
    [1] = "Debug",
    [2] = "",   /* NMI is not pipelined. */
    [3] = "Int3",
    [4] = "Overflow",
    [5] = "Bounds",
    [6] = "Invalid opcode",
    [7] = "FPU not available",
    [8] = "Double fault",
    [9] = "FPU segment overrun",
    [10] = "Invalid TSS",
    [11] = "Segment not present",
    [12] = "Stack segment",
    [13] = "General protection",
    [14] = "Page fault",
    [15] = "Spurious interrupt",
    [16] = "FPU error",
    [17] = "Alignment check",
    [18] = "Machine check",
    [19] = "SIMD error",
    [20] = NULL,
};

struct xnarch_machdesc xnarch_machdesc = {
	.name = "x86",
	.init = mach_x86_init,
	.cleanup = mach_x86_cleanup,
	.calibrate = mach_x86_calibrate,
	.prefault = NULL,
	.fault_labels = fault_labels,
};
