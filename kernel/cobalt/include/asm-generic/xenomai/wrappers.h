/*
 * Copyright (C) 2005-2012 Philippe Gerum <rpm@xenomai.org>.
 *
 * Xenomai is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
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
#ifndef _COBALT_ASM_GENERIC_WRAPPERS_H

#ifndef __KERNEL__
#error "Pure kernel header included from user-space!"
#endif

#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
#error "Xenomai/cobalt requires Linux kernel 3.10 or above"
#endif

#ifdef CONFIG_IPIPE_LEGACY
#error "CONFIG_IPIPE_LEGACY must be switched off"
#endif

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/ipipe.h>
#include <linux/ipipe_tickdev.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/pid.h>
#include <linux/mm.h>
#include <asm/io.h>

#ifndef pgprot_noncached
#define pgprot_noncached(p) (p)
#endif /* !pgprot_noncached */
 
#endif /* _COBALT_ASM_GENERIC_WRAPPERS_H */
