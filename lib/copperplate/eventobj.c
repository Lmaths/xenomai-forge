/*
 * Copyright (C) 2012 Philippe Gerum <rpm@xenomai.org>.
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

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
 */

#include <assert.h>
#include <errno.h>
#include "copperplate/threadobj.h"
#include "copperplate/eventobj.h"
#include "copperplate/debug.h"

#ifdef CONFIG_XENO_COBALT

#include "cobalt/internal.h"

int eventobj_init(struct eventobj *evobj, unsigned long value, int flags,
		  fnref_type(void (*)(struct eventobj *evobj)) finalizer)
{
	int ret, event_flags = event_scope_attribute;

	if (flags & EVOBJ_PRIO)
		event_flags |= COBALT_EVENT_PRIO;

	ret = cobalt_event_init(&evobj->core.event, value, event_flags);
	if (ret)
		return __bt(ret);

	evobj->finalizer = finalizer;

	return 0;
}

int eventobj_destroy(struct eventobj *evobj)
{
	void (*finalizer)(struct eventobj *evobj);
	int ret;

	ret = cobalt_event_destroy(&evobj->core.event);
	if (ret)
		return ret;

	fnref_get(finalizer, evobj->finalizer);
	finalizer(evobj);

	return 0;
}

int eventobj_wait(struct eventobj *evobj,
		  unsigned long bits, unsigned long *bits_r,
		  int mode, const struct timespec *timeout)
{
	int ret;

	ret = cobalt_event_wait(&evobj->core.event,
				bits, bits_r, mode, timeout);
	if (ret)
		return ret;

	return 0;
}

int eventobj_post(struct eventobj *evobj, unsigned long bits)
{
	int ret;

	ret = cobalt_event_post(&evobj->core.event, bits);
	if (ret)
		return ret;

	return 0;
}

int eventobj_clear(struct eventobj *evobj, unsigned long bits,
		   unsigned long *bits_r)
{
	unsigned long oldval;

	oldval = cobalt_event_clear(&evobj->core.event, bits);
	if (bits_r)
		*bits_r = oldval;

	return 0;
}

int eventobj_inquire(struct eventobj *evobj, unsigned long *bits_r)
{
	return cobalt_event_inquire(&evobj->core.event, bits_r);
}

#else /* CONFIG_XENO_MERCURY */

static void eventobj_finalize(struct syncobj *sobj)
{
	struct eventobj *evobj = container_of(sobj, struct eventobj, core.sobj);
	void (*finalizer)(struct eventobj *evobj);

	fnref_get(finalizer, evobj->finalizer);
	finalizer(evobj);
}
fnref_register(libcopperplate, eventobj_finalize);

int eventobj_init(struct eventobj *evobj, unsigned long value, int flags,
		  fnref_type(void (*)(struct eventobj *evobj)) finalizer)
{
	int sobj_flags = 0;

	if (flags & EVOBJ_PRIO)
		sobj_flags = SYNCOBJ_PRIO;

	syncobj_init(&evobj->core.sobj, CLOCK_COPPERPLATE, sobj_flags,
		     fnref_put(libcopperplate, eventobj_finalize));

	evobj->core.flags = flags;
	evobj->core.value = value;
	evobj->finalizer = finalizer;

	return 0;
}

int eventobj_destroy(struct eventobj *evobj)
{
	struct syncstate syns;
	int ret;

	if (syncobj_lock(&evobj->core.sobj, &syns))
		return -EINVAL;

	ret = syncobj_destroy(&evobj->core.sobj, &syns);
	if (ret < 0)
		return ret;

	return 0;
}

int eventobj_wait(struct eventobj *evobj,
		  unsigned long bits, unsigned long *bits_r,
		  int mode, const struct timespec *timeout)
{
	struct eventobj_wait_struct *wait;
	unsigned long waitval, testval;
	struct syncstate syns;
	int ret = 0;

	ret = syncobj_lock(&evobj->core.sobj, &syns);
	if (ret)
		return ret;

	if (bits == 0) {
		*bits_r = evobj->core.value;
		goto done;
	}

	waitval = evobj->core.value & bits;
	testval = mode & EVOBJ_ANY ? waitval : evobj->core.value;

	if (waitval && waitval == testval) {
		*bits_r = waitval;
		goto done;
	}

	/* Have to wait. */

	if (timeout && timeout->tv_sec == 0 && timeout->tv_nsec == 0) {
		ret = -EWOULDBLOCK;
		goto done;
	}

	wait = threadobj_prepare_wait(struct eventobj_wait_struct);
	wait->value = bits;
	wait->mode = mode;

	ret = syncobj_wait_grant(&evobj->core.sobj, timeout, &syns);
	if (ret == -EIDRM) {
		threadobj_finish_wait();
		return ret;
	}

	if (ret == 0)
		*bits_r = wait->value;

	threadobj_finish_wait();
done:
	syncobj_unlock(&evobj->core.sobj, &syns);

	return ret;
}

int eventobj_post(struct eventobj *evobj, unsigned long bits)
{
	struct eventobj_wait_struct *wait;
	unsigned long waitval, testval;
	struct threadobj *thobj, *tmp;
	struct syncstate syns;
	int ret;

	ret = syncobj_lock(&evobj->core.sobj, &syns);
	if (ret)
		return ret;

	evobj->core.value |= bits;

	if (!syncobj_grant_wait_p(&evobj->core.sobj))
		goto done;

	syncobj_for_each_grant_waiter_safe(&evobj->core.sobj, thobj, tmp) {
		wait = threadobj_get_wait(thobj);
		waitval = wait->value & bits;
		testval = wait->mode & EVOBJ_ANY ? waitval : wait->value;
		if (waitval && waitval == testval) {
			wait->value = waitval;
			syncobj_grant_to(&evobj->core.sobj, thobj);
		}
	}
done:
	syncobj_unlock(&evobj->core.sobj, &syns);

	return 0;
}

int eventobj_clear(struct eventobj *evobj,
		   unsigned long bits,
		   unsigned long *bits_r)
{
	struct syncstate syns;
	unsigned long oldval;
	int ret;

	ret = syncobj_lock(&evobj->core.sobj, &syns);
	if (ret)
		return ret;

	oldval = evobj->core.value;
	evobj->core.value &= ~bits;

	syncobj_unlock(&evobj->core.sobj, &syns);

	if (bits_r)
		*bits_r = oldval;

	return 0;
}

int eventobj_inquire(struct eventobj *evobj, unsigned long *bits_r)
{
	struct syncstate syns;
	int ret, nwait;

	ret = syncobj_lock(&evobj->core.sobj, &syns);
	if (ret)
		return ret;

	*bits_r = evobj->core.value;
	nwait = syncobj_count_grant(&evobj->core.sobj);

	syncobj_unlock(&evobj->core.sobj, &syns);

	return nwait;
}

#endif /* CONFIG_XENO_MERCURY */
