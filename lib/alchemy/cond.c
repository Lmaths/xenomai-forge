/*
 * Copyright (C) 2011 Philippe Gerum <rpm@xenomai.org>.
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
 *
 * @defgroup alchemy_cond Condition variable services.
 * @ingroup alchemy_cond
 * @ingroup alchemy
 *
 * Condition variable services.
 *
 * A condition variable is a synchronization object which allows tasks
 * to suspend execution until some predicate on shared data is
 * satisfied. The basic operations on conditions are: signal the
 * condition (when the predicate becomes true), and wait for the
 * condition, blocking the task execution until another task signals
 * the condition.  A condition variable must always be associated with
 * a mutex, to avoid a well-known race condition where a task prepares
 * to wait on a condition variable and another task signals the
 * condition just before the first task actually waits on it.
 *
 *@{*/

#include <errno.h>
#include <string.h>
#include <copperplate/threadobj.h>
#include <copperplate/heapobj.h>
#include "internal.h"
#include "cond.h"
#include "timer.h"
#include "mutex.h"

/*
 * XXX: Alchemy condvars are paired with Alchemy mutex objects, so we
 * must rely on POSIX condvars directly.
 */

struct syncluster alchemy_cond_table;

static DEFINE_NAME_GENERATOR(cond_namegen, "cond",
			     struct alchemy_cond, name);

DEFINE_LOOKUP_PRIVATE(cond, RT_COND);

/**
 * @fn int rt_cond_create(RT_COND *cond, const char *name)
 * @brief Create a condition variable.
 *
 * Create a synchronization object which allows tasks to suspend
 * execution until some predicate on shared data is satisfied.
 *
 * @param cond The address of a condition variable descriptor which
 * can be later used to identify uniquely the created object, upon
 * success of this call.
 *
 * @param name An ASCII string standing for the symbolic name of the
 * condition variable. When non-NULL and non-empty, a copy of this
 * string is used for indexing the created condition variable into the
 * object registry.
 *
 * @return Zero is returned upon success. Otherwise:
 *
 * - -ENOMEM is returned if the system fails to get memory from the
 * main heap in order to create the condition variable.
 *
 * - -EEXIST is returned if the @a name is conflicting with an already
 * registered condition variable.
 *
 * - -EPERM is returned if this service was called from an
 * asynchronous context.
 *
 * Valid calling context:
 *
 * - Regular POSIX threads
 * - Xenomai threads
 *
 * @note Condition variables can be shared by multiple processes which
 * belong to the same Xenomai session.
 */
int rt_cond_create(RT_COND *cond, const char *name)
{
	struct alchemy_cond *ccb;
	pthread_condattr_t cattr;
	struct service svc;
	int ret = 0;

	if (threadobj_irq_p())
		return -EPERM;

	CANCEL_DEFER(svc);

	ccb = xnmalloc(sizeof(*ccb));
	if (ccb == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	generate_name(ccb->name, name, &cond_namegen);
	__RT(pthread_condattr_init(&cattr));
	__RT(pthread_condattr_setpshared(&cattr, mutex_scope_attribute));
	__RT(pthread_condattr_setclock(&cattr, CLOCK_COPPERPLATE));
	__RT(pthread_cond_init(&ccb->cond, &cattr));
	__RT(pthread_condattr_destroy(&cattr));
	ccb->magic = cond_magic;

	if (syncluster_addobj(&alchemy_cond_table, ccb->name, &ccb->cobj)) {
		__RT(pthread_cond_destroy(&ccb->cond));
		xnfree(ccb);
		ret = -EEXIST;
	} else
		cond->handle = mainheap_ref(ccb, uintptr_t);
out:
	CANCEL_RESTORE(svc);

	return ret;
}

/**
 * @fn int rt_cond_delete(RT_COND *cond)
 * @brief Delete a condition variable.
 *
 * This routine deletes a condition variable object previously created
 * by a call to rt_cond_create().
 *
 * @param cond The descriptor address of the deleted condition variable.
 *
 * @return Zero is returned upon success. Otherwise:
 *
 * - -EINVAL is returned if @a alarm is not a valid condition variable
 * descriptor.
 *
 * - -EPERM is returned if this service was called from an
 * asynchronous context.
 *
 * - -EBUSY is returned upon an attempt to destroy the object
 * referenced by @a cond while it is referenced (for example, while
 * being used in a rt_cond_wait(), rt_cond_wait_timed() or
 * rt_cond_wait_until() by another task).
 *
 * Valid calling context:
 *
 * - Regular POSIX threads
 * - Xenomai threads
 */
int rt_cond_delete(RT_COND *cond)
{
	struct alchemy_cond *ccb;
	struct service svc;
	int ret = 0;

	if (threadobj_irq_p())
		return -EPERM;

	CANCEL_DEFER(svc);

	ccb = find_alchemy_cond(cond, &ret);
	if (ccb == NULL)
		goto out;

	ret = -__RT(pthread_cond_destroy(&ccb->cond));
	if (ret)
		goto out;

	ccb->magic = ~cond_magic;
	syncluster_delobj(&alchemy_cond_table, &ccb->cobj);
	xnfree(ccb);
out:
	CANCEL_RESTORE(svc);

	return ret;
}

/**
 * @fn int rt_cond_signal(RT_COND *cond)
 * @brief Signal a condition variable.
 *
 * If the condition variable @a cond is pended, this routine
 * immediately unblocks the first waiting task (by queuing priority
 * order).
 *
 * @param The descriptor address of the condition variable to signal.
 *
 * @return Zero is returned upon success. Otherwise:
 *
 * - -EINVAL is returned if @a cond is not a valid condition variable
 * descriptor.
 *
 * Valid calling context: any.
 */
int rt_cond_signal(RT_COND *cond)
{
	struct alchemy_cond *ccb;
	int ret = 0;

	ccb = find_alchemy_cond(cond, &ret);
	if (ccb == NULL)
		return ret;

	return -__RT(pthread_cond_signal(&ccb->cond));

	return ret;
}

/**
 * @fn int rt_cond_broadcast(RT_COND *cond)
 * @brief Broadcast a condition variable.
 *
 * All tasks currently waiting on the condition variable are
 * immediately unblocked.
 *
 * @param The descriptor address of the condition variable to
 * broadcast.
 *
 * @return Zero is returned upon success. Otherwise:
 *
 * - -EINVAL is returned if @a cond is not a valid condition variable
 * descriptor.
 *
 * Valid calling context: any.
 */
int rt_cond_broadcast(RT_COND *cond)
{
	struct alchemy_cond *ccb;
	int ret = 0;

	ccb = find_alchemy_cond(cond, &ret);
	if (ccb == NULL)
		return ret;

	return -__RT(pthread_cond_broadcast(&ccb->cond));
}

/**
 * @fn int rt_cond_wait(RT_COND *cond, RT_MUTEX *mutex, RTIME timeout)
 * @brief Wait on a condition variable (with relative scalar timeout).
 *
 * This routine is a variant of rt_cond_wait_timed() accepting a
 * relative timeout specification expressed as a scalar value.
 *
 * @param cond The descriptor address of the condition variable to
 * wait on.
 *
 * @param timeout A delay expressed in clock ticks.
 */

/**
 * @fn int rt_cond_wait_until(RT_COND *cond, RT_MUTEX *mutex, RTIME abs_timeout)
 * @brief Wait on a condition variable (with absolute scalar timeout).
 *
 * This routine is a variant of rt_cond_wait_timed() accepting an
 * abs_timeout specification expressed as a scalar value.
 *
 * @param cond The descriptor address of the condition variable to
 * wait on.
 *
 * @param abs_timeout An absolute date expressed in clock ticks.
 */

/**
 * @fn int rt_cond_wait_timed(RT_COND *cond, RT_MUTEX *mutex, const struct timespec *abs_timeout)
 * @brief Wait on a condition variable.
 *
 * This service atomically releases the mutex and blocks the calling
 * task, until the condition variable @a cond is signaled or a timeout
 * occurs, whichever comes first. The mutex is re-acquired before
 * returning from this service.
 *
 * @param cond The descriptor address of the condition variable to
 * wait on.
 *
 * @param abs_timeout An absolute date expressed in clock ticks,
 * specifying a time limit to wait for the condition variable to be
 * signaled  (see note). Passing NULL causes the caller to
 * block indefinitely.
 *
 * @return Zero is returned upon success. Otherwise:
 *
 * - -ETIMEDOUT is returned if @a abs_timeout is reached before the
 * condition variable is signaled.
 *
 * - -EWOULDBLOCK is returned if @a abs_timeout is { .tv_sec = 0,
 * .tv_nsec = 0 } .
 *
 * - -EINTR is returned if rt_task_unblock() was called for the
 * current task.
 *
 * - -EINVAL is returned if @a cond is not a valid condition variable
 * descriptor.
 *
 * - -EIDRM is returned if @a cond is deleted while the caller was
 * waiting on the condition variable. In such event, @a cond is no
 * more valid upon return of this service.
 *
 * - -EPERM is returned if this service should block, but was not
 * called from a Xenomai thread.
 *
 * Valid calling contexts:
 *
 * - Xenomai threads
 *
 * @note @a abs_timeout is interpreted as a multiple of the Alchemy
 * clock resolution (see --alchemy-clock-resolution option, defaults
 * to 1 nanosecond).
 */
int rt_cond_wait_timed(RT_COND *cond, RT_MUTEX *mutex,
		       const struct timespec *abs_timeout)
{
	struct alchemy_mutex *mcb;
	struct alchemy_cond *ccb;
	int ret = 0;

	if (alchemy_poll_mode(abs_timeout))
		return -EWOULDBLOCK;

	ccb = find_alchemy_cond(cond, &ret);
	if (ccb == NULL)
		return ret;

	mcb = find_alchemy_mutex(mutex, &ret);
	if (mcb == NULL)
		return ret;

	if (abs_timeout)
		ret = -__RT(pthread_cond_timedwait(&ccb->cond,
						   &mcb->lock, abs_timeout));
	else
		ret = -__RT(pthread_cond_wait(&ccb->cond, &mcb->lock));

	return ret;
}

/**
 * @fn int rt_cond_inquire(RT_COND *cond, RT_COND_INFO *info)
 * @brief Query condition variable status.
 *
 * This routine returns the status information about the specified
 * condition variable.
 *
 * @param The descriptor address of the condition variable to get the
 * status of.
 *
 * @return Zero is returned and status information is written to the
 * structure pointed at by @a info upon success. Otherwise:
 *
 * - -EINVAL is returned if @a cond is not a valid condition variable
 * descriptor.
 *
 * Valid calling context: any.
 */
int rt_cond_inquire(RT_COND *cond, RT_COND_INFO *info)
{
	struct alchemy_cond *ccb;
	int ret = 0;

	ccb = find_alchemy_cond(cond, &ret);
	if (ccb == NULL)
		return ret;

	strcpy(info->name, ccb->name);

	return ret;
}

/**
 * @fn int rt_cond_bind(RT_COND *cond, const char *name, RTIME timeout)
 * @brief Bind to a condition variable.
 *
 * This routine creates a new descriptor to refer to an existing
 * condition variable identified by its symbolic name. If the object
 * not exist on entry, the caller may block until a condition variable
 * of the given name is created.
 *
 * @param cond The address of a condition variable descriptor filled
 * in by the operation. Contents of this memory is undefined upon
 * failure.
 *
 * @param name A valid NULL-terminated name which identifies the
 * condition variable to bind to. This string should match the object
 * name argument passed to rt_cond_create().
 *
 * @param timeout The number of clock ticks to wait for the
 * registration to occur (see note). Passing TM_INFINITE causes the
 * caller to block indefinitely until the object is
 * registered. Passing TM_NONBLOCK causes the service to return
 * immediately without waiting if the object is not registered on
 * entry.
 *
 * @return Zero is returned upon success. Otherwise:
 *
 * - -EINTR is returned if rt_task_unblock() was called for the
 * current task before the retrieval has completed.
 *
 * - -EWOULDBLOCK is returned if @a timeout is equal to TM_NONBLOCK
 * and the searched object is not registered on entry.
 *
 * - -ETIMEDOUT is returned if the object cannot be retrieved within
 * the specified amount of time.
 *
 * - -EPERM is returned if this service should block, but was not
 * called from a Xenomai thread.
 *
 * Valid calling contexts:
 *
 * - Xenomai threads
 * - Any other context if @a timeout equals TM_NONBLOCK.
 *
 * @note The @a timeout value is interpreted as a multiple of the
 * Alchemy clock resolution (see --alchemy-clock-resolution option,
 * defaults to 1 nanosecond).
 */
int rt_cond_bind(RT_COND *cond,
		 const char *name, RTIME timeout)
{
	return alchemy_bind_object(name,
				   &alchemy_cond_table,
				   timeout,
				   offsetof(struct alchemy_cond, cobj),
				   &cond->handle);
}

/**
 * @fn int rt_cond_unbind(RT_COND *cond)
 * @brief Unbind from a condition variable.
 *
 * @param cond The descriptor address of the condition variable to
 * unbind from.
 *
 * This routine releases a previous binding to a condition
 * variable. After this call has returned, the descriptor is no more
 * valid for referencing this object.
 */
int rt_cond_unbind(RT_COND *cond)
{
	cond->handle = 0;
	return 0;
}

/*@}*/
