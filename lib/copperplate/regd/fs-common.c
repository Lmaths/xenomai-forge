/*
 * Copyright (C) 2013 Philippe Gerum <rpm@xenomai.org>.
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

#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <malloc.h>
#include <version.h>
#include <sched.h>
#include <copperplate/heapobj.h>
#include <copperplate/threadobj.h>
#include <copperplate/clockobj.h>
#include "sysregfs.h"
#include "../internal.h"

#ifdef CONFIG_XENO_PSHARED

/*
 * If --enable-pshared was given, we can access the main shared heap
 * to retrieve session-wide information.
 */

static char *format_time(ticks_t value, char *buf, size_t bufsz)
{
	unsigned long ms, us, ns;
	char *p = buf;
	ticks_t s;

	if (value == 0) {
		strcpy(buf, "-");
		return buf;
	}

	s = value / 1000000000ULL;
	ns = value % 1000000000ULL;
	us = ns / 1000;
	ms = us / 1000;
	us %= 1000;

	if (s)
		p += snprintf(p, bufsz, "%Lus", s);

	if (ms || (s && us))
		p += snprintf(p, bufsz - (p - buf), "%lums", ms);

	if (us)
		p += snprintf(p, bufsz - (p - buf), "%luus", us);

	return buf;
}

ssize_t read_threads(struct fsobj *fsobj, char *buf,
		     size_t size, off_t offset,
		     void *priv)
{
	struct thread_data *thread_data, *p;
	char sbuf[64], pbuf[16], tbuf[64];
	struct threadobj_stat statbuf;
	struct sysgroup_memspec *obj;
	struct threadobj *thobj;
	const char *sched_class;
	ssize_t len = 0;
	int ret, count;

	ret = heapobj_bind_session(__node_info.session_label);
	if (ret)
		return ret;

	sysgroup_lock();
	count = sysgroup_count(thread);
	sysgroup_unlock();

	if (count == 0)
		goto out;

	/*
	 * We don't want to hold the sysgroup lock for too long, since
	 * it could be contended by a real-time task. So we pull all
	 * the per-thread data we need into a local array, before
	 * printing out its contents after we dropped the lock.
	 */
	thread_data = p = malloc(sizeof(*p) * count);
	if (thread_data == NULL) {
		len = -ENOMEM;
		goto out;
	}

	sysgroup_lock();

	for_each_sysgroup(obj, thread) {
		if (p - thread_data >= count)
			break;
		thobj = container_of(obj, struct threadobj, memspec);
		ret = threadobj_lock(thobj);
		if (ret)
			continue;
		strncpy(p->name, thobj->name, sizeof(p->name) - 1);
		p->name[sizeof(p->name) - 1] = '\0';
		p->pid = thobj->pid;
		p->priority = thobj->priority;
		p->policy = thobj->policy;
		threadobj_stat(thobj, &statbuf);
		threadobj_unlock(thobj);
		p->status = statbuf.status;
		p->cpu = statbuf.cpu;
		p->timeout = statbuf.timeout;
		p->schedlock = statbuf.schedlock;
		p++;
	}

	sysgroup_unlock();

	count = p - thread_data;
	if (count == 0)
		goto out_free;

	len = sprintf(buf, "%-3s  %-6s %-5s  %-8s %-8s  %-10s %s\n",
			"CPU", "PID", "CLASS", "PRI", "TIMEOUT",
			"STAT", "NAME");

	for (p = thread_data; count > 0; count--) {
		if (kill(p->pid, 0))
			continue;
		snprintf(pbuf, sizeof(pbuf), "%3d", p->priority);
		format_time(p->timeout, tbuf, sizeof(tbuf));
		format_thread_status(p, sbuf, sizeof(sbuf));
		switch (p->policy) {
		case SCHED_RT:
			sched_class = "rt";
			break;
		case SCHED_RR:
			sched_class = "rr";
			break;
#ifdef SCHED_SPORADIC
		case SCHED_SPORADIC:
			sched_class = "pss";
			break;
#endif
#ifdef SCHED_TP
		case SCHED_TP:
			sched_class = "tp";
			break;
#endif
#ifdef SCHED_QUOTA
		case SCHED_QUOTA:
			sched_class = "quota";
			break;
#endif
#ifdef SCHED_QUOTA
		case SCHED_WEAK:
			sched_class = "weak";
			break;
#endif
		default:
			sched_class = "other";
			break;
		}
		len += sprintf(buf + len,
			       "%3u  %-6d %-5s  %-8s %-8s  %-10s %s\n",
			       p->cpu, p->pid, sched_class, pbuf,
			       tbuf, sbuf, p->name);
		p++;
	}

out_free:
	free(thread_data);
out:
	heapobj_unbind_session();

	return len;
}

struct heap_data {
	char name[32];
	size_t total;
	size_t used;
};

ssize_t read_heaps(struct fsobj *fsobj, char *buf,
		   size_t size, off_t offset,
		   void *priv)
{
	struct heap_data *heap_data, *p;
	struct sysgroup_memspec *obj;
	struct shared_heap *heap;
	ssize_t len = 0;
	int ret, count;

	ret = heapobj_bind_session(__node_info.session_label);
	if (ret)
		return ret;

	sysgroup_lock();
	count = sysgroup_count(heap);
	sysgroup_unlock();

	if (count == 0)
		goto out;

	heap_data = p = malloc(sizeof(*p) * count);
	if (heap_data == NULL) {
		len = -ENOMEM;
		goto out;
	}

	sysgroup_lock();

	/*
	 * A heap we find there cannot totally vanish until we drop
	 * the group lock, so there is no point in acquiring each heap
	 * lock individually for reading the slot.
	 */
	for_each_sysgroup(obj, heap) {
		if (p - heap_data >= count)
			break;
		heap = container_of(obj, struct shared_heap, memspec);
		strncpy(p->name, heap->name, sizeof(p->name) - 1);
		p->name[sizeof(p->name) - 1] = '\0';
		p->used = heap->ubytes;
		p->total = heap->total;
		p++;
	}

	sysgroup_unlock();

	count = p - heap_data;
	if (count == 0)
		goto out_free;

	len = sprintf(buf, "%9s %9s  %s\n", "TOTAL", "USED", "NAME");

	for (p = heap_data; count > 0; count--) {
		len += sprintf(buf + len, "%9Zu %9Zu  %s\n",
			       p->total, p->used, p->name);
		p++;
	}

out_free:
	free(heap_data);
out:
	heapobj_unbind_session();

	return len;
}

#endif /* CONFIG_XENO_PSHARED */

ssize_t read_version(struct fsobj *fsobj, char *buf,
		     size_t size, off_t offset,
		     void *priv)
{
	return sprintf(buf, "%s\n", XENO_VERSION_STRING);
}
