/*
 * Copyright (C) 2001,2002,2003,2004 Philippe Gerum <rpm@xenomai.org>.
 * Copyright (C) 2013 Gilles Chanteperdrix <gch@xenomai.org>.
 *
 * ARM port
 *   Copyright (C) 2005 Stelian Pop
 *
 * Copyright (C) 2007 Sebastian Smolorz <sesmo@gmx.net>
 *	Support for TSC emulation in user space for decrementing counters
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
 */
#include <asm/xenomai/tsc.h>
#include <asm/xenomai/features.h>

unsigned long long __xn_rdtsc(void)
{
#if CONFIG_XENO_ARM_TSC_TYPE == __XN_TSC_TYPE_KUSER
	return __xn_tscinfo.kuser_tsc_get(__xn_tscinfo.kinfo.counter);

#elif CONFIG_XENO_ARM_TSC_TYPE == __XN_TSC_TYPE_FREERUNNING
	volatile unsigned long long *const tscp = __xn_tscinfo.kinfo.tsc;
	volatile unsigned int *const counterp = __xn_tscinfo.kinfo.counter;
	const unsigned int mask = __xn_tscinfo.kinfo.mask;
	register unsigned long long result;
	unsigned int counter;

	__asm__ ("ldmia %1, %M0\n": "=r"(result): "r"(tscp), "m"(*tscp));
	__asm__ __volatile__ ("" : /* */ : /* */ : "memory");
	counter = *counterp;

	if ((counter & mask) < ((unsigned) result & mask))
		result += mask + 1ULL;
	return (result & ~((unsigned long long) mask)) | (counter & mask);

#elif CONFIG_XENO_ARM_TSC_TYPE == __XN_TSC_TYPE_FREERUNNING_COUNTDOWN
	volatile unsigned long long *const tscp = __xn_tscinfo.kinfo.tsc;
	volatile unsigned int *const counterp = __xn_tscinfo.kinfo.counter;
	const unsigned int mask = __xn_tscinfo.kinfo.mask;
	register unsigned long long result;
	unsigned int counter;

	__asm__ ("ldmia %1, %M0\n": "=r"(result): "r"(tscp), "m"(*tscp));
	__asm__ __volatile__ ("" : /* */ : /* */ : "memory");
	counter = mask - *counterp;

	if ((counter & mask) > ((unsigned) result & mask))
		result += mask + 1ULL;
	return (result & ~((unsigned long long) mask)) | (counter & mask);

#elif CONFIG_XENO_ARM_TSC_TYPE == __XN_TSC_TYPE_FREERUNNING_FAST_WRAP
	volatile unsigned long long *const tscp = __xn_tscinfo.kinfo.tsc;
	volatile unsigned int *const counterp = __xn_tscinfo.kinfo.counter;
	const unsigned int mask = __xn_tscinfo.kinfo.mask;
	register unsigned long long after, before;
	unsigned int counter;

	__asm__ ("ldmia %1, %M0\n": "=r"(after): "r"(tscp), "m"(*tscp));
	do {
		before = after;
		counter = *counterp;
		__asm__ __volatile__ ("" : /* */ : /* */ : "memory");
		__asm__ ("ldmia %1, %M0\n" : "=r"(after): "r"(tscp), "m"(*tscp));
	} while (((unsigned) after) != ((unsigned) before));
	if ((counter & mask) < ((unsigned) before & mask))
		before += mask + 1;
	return (before & ~((unsigned long long) mask)) | (counter & mask);

#elif CONFIG_XENO_ARM_TSC_TYPE == __XN_TSC_TYPE_DECREMENTER
	volatile unsigned long long *const tscp = __xn_tscinfo.kinfo.tsc;
	volatile unsigned int *const counterp = __xn_tscinfo.kinfo.counter;
	volatile unsigned int *const last_cntp = __xn_tscinfo.kinfo.last_cnt;
	const unsigned int mask = __xn_tscinfo.kinfo.mask;
	register unsigned long long after, before;
	unsigned int counter, last_cnt;

	__asm__ ("ldmia %1, %M0\n": "=r"(after): "r"(tscp), "m"(*tscp));
	do {
		before = after;
		counter = *counterp;
		last_cnt = *last_cntp;
		/* compiler barrier. */
		__asm__ __volatile__ ("" : /* */ : /* */ : "memory");
		__asm__ ("ldmia %1, %M0\n": "=r"(after): "r"(tscp), "m"(*tscp));
	} while (after != before);

	counter &= mask;
	last_cnt &= mask;
	if (counter > last_cnt)
		before += mask + 1ULL;
	return (before + last_cnt - counter);

#endif /* CONFIG_XENO_ARM_TSC_TYPE == __XN_TSC_TYPE_DECREMENTER */
}
