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
 */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>
#include <linux/unistd.h>
#include <boilerplate/ancillaries.h>
#include <copperplate/clockobj.h>
#include <copperplate/threadobj.h>
#include <copperplate/init.h>
#include "internal.h"

static int thread_spawn_prologue(struct corethread_attributes *cta);

static int thread_spawn_epilogue(struct corethread_attributes *cta);

static void *thread_trampoline(void *arg);

pid_t copperplate_get_tid(void)
{
	/*
	 * XXX: The nucleus maintains a hash table indexed on
	 * task_pid_vnr() values for mapped shadows. This is what
	 * __NR_gettid retrieves as well in Cobalt mode.
	 */
	return syscall(__NR_gettid);
}

#ifdef CONFIG_XENO_COBALT

#include "cobalt/internal.h"

int copperplate_probe_node(unsigned int id)
{
	/*
	 * XXX: this call does NOT migrate to secondary mode therefore
	 * may be used in time-critical contexts. However, since the
	 * nucleus has to know about a probed thread to find out
	 * whether it exists, copperplate_init() must always be
	 * invoked from a real-time shadow, so that __node_id can be
	 * matched.
	 */
	return pthread_probe_np((pid_t)id) == 0;
}

int copperplate_create_thread(struct corethread_attributes *cta,
			      pthread_t *tid)
{
	struct sched_param_ex param_ex;
	pthread_attr_ex_t attr_ex;
	size_t stacksize;
	int policy, ret;

	ret = thread_spawn_prologue(cta);
	if (ret)
		return __bt(ret);

	stacksize = cta->stacksize;
	if (stacksize < PTHREAD_STACK_MIN * 4)
		stacksize = PTHREAD_STACK_MIN * 4;

	param_ex.sched_priority = cta->prio;
	policy = cta->prio ? SCHED_RT : SCHED_OTHER;
	pthread_attr_init_ex(&attr_ex);
	pthread_attr_setinheritsched_ex(&attr_ex, PTHREAD_EXPLICIT_SCHED);
	pthread_attr_setschedpolicy_ex(&attr_ex, policy);
	pthread_attr_setschedparam_ex(&attr_ex, &param_ex);
	pthread_attr_setstacksize_ex(&attr_ex, stacksize);
	pthread_attr_setdetachstate_ex(&attr_ex, cta->detachstate);
	ret = __bt(-pthread_create_ex(tid, &attr_ex, thread_trampoline, cta));
	pthread_attr_destroy_ex(&attr_ex);

	if (ret)
		return __bt(ret);

	thread_spawn_epilogue(cta);

	return 0;
}

int copperplate_renice_thread(pthread_t tid, int prio)
{
	struct sched_param_ex param_ex;
	int policy;

	param_ex.sched_priority = prio;
	policy = prio ? SCHED_RT : SCHED_OTHER;

	return __bt(-pthread_setschedparam_ex(tid, policy, &param_ex));
}

static inline void prepare_wait_corespec(void)
{
	/*
	 * Switch back to primary mode eagerly, so that both the
	 * parent and the child threads compete on the same priority
	 * scale when handshaking. In addition, this ensures the child
	 * thread enters the run() handler over the Xenomai domain,
	 * which is a basic assumption for all clients.
	 */
	__cobalt_thread_harden();
}

#else /* CONFIG_XENO_MERCURY */

int copperplate_probe_node(unsigned int id)
{
	return kill((pid_t)id, 0) == 0;
}

int copperplate_create_thread(struct corethread_attributes *cta,
			      pthread_t *tid)
{
	struct sched_param param;
	pthread_attr_t attr;
	size_t stacksize;
	int policy, ret;

	ret = thread_spawn_prologue(cta);
	if (ret)
		return __bt(ret);

	stacksize = cta->stacksize;
	if (stacksize < PTHREAD_STACK_MIN * 4)
		stacksize = PTHREAD_STACK_MIN * 4;

	param.sched_priority = cta->prio;
	policy = cta->prio ? SCHED_RT : SCHED_OTHER;
	pthread_attr_init(&attr);
	pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
	pthread_attr_setschedpolicy(&attr, policy);
	pthread_attr_setschedparam(&attr, &param);
	pthread_attr_setstacksize(&attr, stacksize);
	pthread_attr_setdetachstate(&attr, cta->detachstate);
	ret = __bt(-pthread_create(tid, &attr, thread_trampoline, cta));
	pthread_attr_destroy(&attr);

	if (ret)
		return __bt(ret);

	ret = thread_spawn_epilogue(cta);

	return __bt(ret);
}

int copperplate_renice_thread(pthread_t tid, int prio)
{
	struct sched_param param;
	int policy;

	param.sched_priority = prio;
	policy = prio ? SCHED_RT : SCHED_OTHER;

	return __bt(-__RT(pthread_setschedparam(tid, policy, &param)));
}

static inline void prepare_wait_corespec(void)
{
	/* empty */
}

#endif  /* CONFIG_XENO_MERCURY */

static int thread_spawn_prologue(struct corethread_attributes *cta)
{
	int ret;

	ret = __RT(sem_init(&cta->__reserved.warm, 0, 0));
	if (ret)
		return __bt(ret);

	cta->__reserved.status = -ENOSYS;

	return 0;
}

static void thread_spawn_wait(sem_t *sem)
{
	int ret;

	prepare_wait_corespec();

	for (;;) {
		ret = __RT(sem_wait(sem));
		if (ret && errno == EINTR)
			continue;
		if (ret == 0)
			return;
		ret = -errno;
		panic("sem_wait() failed with %s", symerror(ret));
	}
}

static void *thread_trampoline(void *arg)
{
	struct corethread_attributes *cta = arg;
	void *__arg, *(*__run)(void *arg);
	sem_t released;
	int ret;

	/*
	 * cta may be on the parent's stack, so it may be dandling
	 * soon after the parent is posted: copy what we need from
	 * this area early on.
	 */
	__run = cta->run;
	__arg = cta->arg;
	ret = cta->prologue(__arg);
	cta->__reserved.status = ret;

	if (ret) {
		backtrace_check();
		__RT(sem_post(&cta->__reserved.warm));
		return (void *)(long)ret;
	}

	__RT(sem_init(&released, 0, 0));
	cta->__reserved.released = &released;
	__RT(sem_post(&cta->__reserved.warm));

	thread_spawn_wait(&released);

	__RT(sem_destroy(&released));

	return __run(__arg);
}

static int thread_spawn_epilogue(struct corethread_attributes *cta)
{
	thread_spawn_wait(&cta->__reserved.warm);

	if (cta->__reserved.status == 0)
		__RT(sem_post(cta->__reserved.released));

	__RT(sem_destroy(&cta->__reserved.warm));

	return __bt(cta->__reserved.status);
}

void panic(const char *fmt, ...)
{
	struct threadobj *thobj = threadobj_current();
	va_list ap;

	va_start(ap, fmt);
	__panic(thobj ? threadobj_get_name(thobj) : NULL, fmt, ap);
	va_end(ap);
}

void warning(const char *fmt, ...)
{
	struct threadobj *thobj = threadobj_current();
	va_list ap;

	va_start(ap, fmt);
	__warning(thobj ? threadobj_get_name(thobj) : NULL, fmt, ap);
	va_end(ap);
}
