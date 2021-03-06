/*
 * Written by Gilles Chanteperdrix <gilles.chanteperdrix@xenomai.org>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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
#ifndef _COBALT_POSIX_INTERNAL_H
#define _COBALT_POSIX_INTERNAL_H

#include <cobalt/kernel/sched.h>
#include <cobalt/kernel/heap.h>
#include <cobalt/kernel/ppd.h>
#include <cobalt/kernel/assert.h>
#include <cobalt/kernel/list.h>
#include <cobalt/kernel/arith.h>
#include "registry.h"
#include "process.h"
#include "extension.h"

#define COBALT_MAGIC(n) (0x8686##n##n)
#define COBALT_ANY_MAGIC         COBALT_MAGIC(00)
#define COBALT_THREAD_MAGIC      COBALT_MAGIC(01)
#define COBALT_MUTEX_ATTR_MAGIC  (COBALT_MAGIC(04) & ((1 << 24) - 1))
#define COBALT_COND_ATTR_MAGIC   (COBALT_MAGIC(06) & ((1 << 24) - 1))
#define COBALT_KEY_MAGIC         COBALT_MAGIC(08)
#define COBALT_ONCE_MAGIC        COBALT_MAGIC(09)
#define COBALT_MQ_MAGIC          COBALT_MAGIC(0A)
#define COBALT_MQD_MAGIC         COBALT_MAGIC(0B)
#define COBALT_INTR_MAGIC        COBALT_MAGIC(0C)
#define COBALT_TIMER_MAGIC       COBALT_MAGIC(0E)
#define COBALT_EVENT_MAGIC       COBALT_MAGIC(0F)
#define COBALT_MONITOR_MAGIC     COBALT_MAGIC(10)

#define cobalt_obj_active(h,m,t)			\
	((h) && ((t *)(h))->magic == (m))

#define cobalt_mark_deleted(t) ((t)->magic = ~(t)->magic)

extern int cobalt_muxid;

static inline struct cobalt_process *cobalt_process_context(void)
{
	return xnshadow_private_get(cobalt_muxid);
}

static inline struct cobalt_kqueues *cobalt_kqueues(int pshared)
{
	struct cobalt_process *ppd;

	if (pshared || (ppd = xnshadow_private_get(cobalt_muxid)) == NULL)
		return &cobalt_global_kqueues;

	return &ppd->kqueues;
}

int cobalt_init(void);

void cobalt_cleanup(void);

int cobalt_syscall_init(void);

void cobalt_syscall_cleanup(void);

#endif /* !_COBALT_POSIX_INTERNAL_H */
