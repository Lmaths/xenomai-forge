/*
 * Copyright (C) 2010 Philippe Gerum <rpm@xenomai.org>.
 *
 * Xenomai is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Xenomai is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Xenomai; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */
#include <linux/types.h>
#include <linux/ipipe.h>
#include <cobalt/kernel/thread.h>
#include <asm/cacheflush.h>
#include <asm/ptrace.h>
#include <asm/xenomai/syscall.h>

void xnarch_setup_mayday_page(void *page)
{
	/*
	 * We want this code to appear at the top of the MAYDAY page:
	 *
	 * ifdef ARM_EABI
	 *
	 * e59f000c     ldr     r0, [pc, #12]
	 * e59f700c     ldr     r7, [pc, #12]
	 * ef000000 	svc	0x00000000
	 * e3a00000 	mov	r0, #0
	 * e5800000 	str	r0, [r0]	; <bug>
	 * 0a00022b     .word   0x0a00022b	; __xn_sys_mayday << 24
	 * 000f0042     .word   0x000f0042
	 *
	 * elif ARM_OABI
	 *
	 * e59f0008     ldr     r0, [pc, #8]
	 * ef9f0042 	swi	0x009f0042
	 * e3a00000 	mov	r0, #0
	 * e5800000 	str	r0, [r0]	; <bug>
	 * 0a00022b     .word   0x0a00022b	; __xn_sys_mayday << 24
	 *
	 * endif
	 *
	 * 32bit instruction words will be laid out by the compiler as
	 * the target endianness requires.
	 *
	 * We don't mess with CPSR here, so no need to save/restore it
	 * in handle/fixup code.
	 */
#ifdef CONFIG_XENO_ARM_EABI
	static const struct {
		u32 ldr_r0;
		u32 ldr_r7;
		u32 swi_0;
		u32 mov_r0;
		u32 str_r0;
		u32 cst_r0;
		u32 cst_r7;
	} code = {
		.ldr_r0 = 0xe59f000c,
		.ldr_r7 = 0xe59f700c,
		.swi_0 = 0xef000000,
		.mov_r0 = 0xe3a00000,
		.str_r0 = 0xe5800000,
		.cst_r0 = 0x0a00022b,
		.cst_r7 = 0x000f0042,
	};
#else /* OABI */
	static const struct {
		u32 ldr_r0;
		u32 swi_syscall;
		u32 mov_r0;
		u32 str_r0;
		u32 cst_r0;
	} code = {
		.ldr_r0 = 0xe59f0008,
		.swi_syscall = 0xef9f0042,
		.mov_r0 = 0xe3a00000,
		.str_r0 = 0xe5800000,
		.cst_r0 = 0x0a00022b,
	};
#endif /* OABI */

	memcpy(page, &code, sizeof(code));

	flush_dcache_page(vmalloc_to_page(page));
}

void xnarch_call_mayday(struct task_struct *p)
{
	ipipe_raise_mayday(p);
}

void xnarch_handle_mayday(struct xnarchtcb *tcb, struct pt_regs *regs,
			  unsigned long tramp)
{
	tcb->mayday.pc = regs->ARM_pc;
	tcb->mayday.r0 = regs->ARM_r0;
#ifdef CONFIG_XENO_ARM_EABI
	tcb->mayday.r7 = regs->ARM_r7;
#endif
#ifdef CONFIG_ARM_THUMB
	/* The code on the mayday page must be run in ARM mode */
	tcb->mayday.psr = regs->ARM_cpsr;
	regs->ARM_cpsr &= ~PSR_T_BIT;
#endif
	regs->ARM_pc = tramp;
}

void xnarch_fixup_mayday(struct xnarchtcb *tcb, struct pt_regs *regs)
{
	regs->ARM_pc = tcb->mayday.pc;
	regs->ARM_r0 = tcb->mayday.r0;
#ifdef CONFIG_XENO_ARM_EABI
	regs->ARM_r7 = tcb->mayday.r7;
#endif
#ifdef CONFIG_ARM_THUMB
	regs->ARM_cpsr = tcb->mayday.psr;
#endif
}
