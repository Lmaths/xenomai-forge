/**
 * Copyright (C) 2012 Philippe Gerum <rpm@xenomai.org>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, Inc., 675 Mass Ave, Cambridge MA 02139,
 * USA; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#ifndef _XENO_ASM_GENERIC_IPIPE_THREAD_INFO_H
#define _XENO_ASM_GENERIC_IPIPE_THREAD_INFO_H

struct xnthread;
struct mm_struct;

struct ipipe_threadinfo {
	struct xnthread *thread;
	struct mm_struct *mm;
};

static inline void __ipipe_init_threadinfo(struct ipipe_threadinfo *p)
{
	p->thread = NULL;
	p->mm = NULL;
}

#endif /* !_XENO_ASM_GENERIC_IPIPE_THREAD_INFO_H */
