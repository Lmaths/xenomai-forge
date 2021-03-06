/*
 * Copyright (C) 2005 Philippe Gerum <rpm@xenomai.org>.
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

#include <stdlib.h>		/* For malloc & free. */
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>		/* For O_CREAT. */
#include <pthread.h>		/* For pthread_setcanceltype. */
#include <semaphore.h>
#include <asm/xenomai/syscall.h>
#include <cobalt/uapi/sem.h>
#include "internal.h"

static inline struct sem_dat *sem_get_datp(struct __shadow_sem *shadow)
{
	unsigned pshared = shadow->datp_offset < 0;
	
	if (pshared) 
		return (struct sem_dat *)
			(cobalt_sem_heap[1] - shadow->datp_offset);
	else
		return (struct sem_dat *)
			(cobalt_sem_heap[0] + shadow->datp_offset);
}

COBALT_IMPL(int, sem_init, (sem_t *sem, int pshared, unsigned value))
{
	struct __shadow_sem *_sem = &((union cobalt_sem_union *)sem)->shadow_sem;
	int err;

	err = -XENOMAI_SKINCALL3(__cobalt_muxid,
				 sc_cobalt_sem_init, _sem, pshared, value);
	if (err < 0) {
		errno = err;
		return -1;
	}
	
	__cobalt_prefault(sem_get_datp(_sem));
	return 0;
}

COBALT_IMPL(int, sem_destroy, (sem_t *sem))
{
	struct __shadow_sem *_sem = &((union cobalt_sem_union *)sem)->shadow_sem;
	int err;

	if (_sem->magic != COBALT_SEM_MAGIC
	    && _sem->magic != COBALT_NAMED_SEM_MAGIC) {
		errno = EINVAL;
		return -1;
	}

	err = XENOMAI_SKINCALL1(__cobalt_muxid, sc_cobalt_sem_destroy, _sem);
	if (err >= 0)
		return err;

	errno = -err;
	return -1;
}

COBALT_IMPL(int, sem_post, (sem_t *sem))
{
	struct __shadow_sem *_sem = &((union cobalt_sem_union *)sem)->shadow_sem;
	struct sem_dat *datp;
	long value;
	int err;

	if (_sem->magic != COBALT_SEM_MAGIC
	    && _sem->magic != COBALT_NAMED_SEM_MAGIC) {
		errno = EINVAL;
		return -1;
	}

	datp = sem_get_datp(_sem);

	value = atomic_long_read(&datp->value);

	if (value >= 0) {
		long old, new;
		
		if (datp->flags & SEM_PULSE)
			return 0;

		do {
			old = value;
			new = value + 1;

			value = atomic_long_cmpxchg(&datp->value, old, new);
			if (value < 0)
				goto do_syscall;
		} while (value != old);

		return 0;
	}	

  do_syscall:
	err = -XENOMAI_SKINCALL1(__cobalt_muxid, sc_cobalt_sem_post, _sem);
	if (!err)
		return 0;

	errno = err;
	return -1;
}

COBALT_IMPL(int, sem_trywait, (sem_t *sem))
{
	struct __shadow_sem *_sem = &((union cobalt_sem_union *)sem)->shadow_sem;
	struct sem_dat *datp;
	long value;

	if (_sem->magic != COBALT_SEM_MAGIC
	    && _sem->magic != COBALT_NAMED_SEM_MAGIC) {
		errno = EINVAL;
		return -1;
	}

	datp = sem_get_datp(_sem);

	value = atomic_long_read(&datp->value);

	if (value > 0) {
		long old, new;

		do {
			old = value;
			new = value - 1;
			
			value = atomic_long_cmpxchg(&datp->value, old, new);
			if (value <= 0)
				goto eagain;
		} while (value != old);
		
		return 0;
	}

  eagain:
	errno = EAGAIN;
	return -1;
}

COBALT_IMPL(int, sem_wait, (sem_t *sem))
{
	struct __shadow_sem *_sem = &((union cobalt_sem_union *)sem)->shadow_sem;
	int err, oldtype;

	err = __RT(sem_trywait(sem));
	
	if (err != -1 || errno != EAGAIN)
		return err;
	
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &oldtype);

	err = -XENOMAI_SKINCALL1(__cobalt_muxid, sc_cobalt_sem_wait, _sem);

	pthread_setcanceltype(oldtype, NULL);

	if (err == 0)
		return 0;
	
	errno = err;
	return -1;
}

COBALT_IMPL(int, sem_timedwait, (sem_t *sem, const struct timespec *ts))
{
	struct __shadow_sem *_sem = &((union cobalt_sem_union *)sem)->shadow_sem;
	int err, oldtype;

	err = __RT(sem_trywait(sem));
	
	if (err != -1 || errno != EAGAIN)
		return err;	

	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &oldtype);

	err = -XENOMAI_SKINCALL2(__cobalt_muxid,
				 sc_cobalt_sem_timedwait, _sem, ts);

	pthread_setcanceltype(oldtype, NULL);

	if (!err)
		return 0;

	errno = err;
	return -1;
}

COBALT_IMPL(int, sem_getvalue, (sem_t *sem, int *sval))
{
	struct __shadow_sem *_sem = &((union cobalt_sem_union *)sem)->shadow_sem;
	struct sem_dat *datp;
	long value;

	if (_sem->magic != COBALT_SEM_MAGIC
	    && _sem->magic != COBALT_NAMED_SEM_MAGIC) {
		errno = EINVAL;
		return -1;
	}

	datp = sem_get_datp(_sem);

	value = atomic_long_read(&datp->value);
	if (value < 0 && (datp->flags & SEM_REPORT) == 0)
		value = 0;

	*sval = value;
	return 0;
}

COBALT_IMPL(sem_t *, sem_open, (const char *name, int oflags, ...))
{
	union cobalt_sem_union *sem, *rsem;
	unsigned value = 0;
	mode_t mode = 0;
	va_list ap;
	int err;

	if ((oflags & O_CREAT)) {
		va_start(ap, oflags);
		mode = va_arg(ap, int);
		value = va_arg(ap, unsigned);
		va_end(ap);
	}

	rsem = sem = (union cobalt_sem_union *)malloc(sizeof(*sem));

	if (!rsem) {
		err = ENOSPC;
		goto error;
	}

	err = -XENOMAI_SKINCALL5(__cobalt_muxid,
				 sc_cobalt_sem_open,
				 &rsem, name, oflags, mode, value);

	if (!err) {
		if (rsem != sem)
			free(sem);
		return &rsem->native_sem;
	}

	free(sem);
      error:
	errno = err;
	return SEM_FAILED;
}

COBALT_IMPL(int, sem_close, (sem_t *sem))
{
	struct __shadow_sem *_sem = &((union cobalt_sem_union *)sem)->shadow_sem;
	int err;

	if (_sem->magic != COBALT_NAMED_SEM_MAGIC) {
		errno = EINVAL;
		return -1;
	}

	err = XENOMAI_SKINCALL1(__cobalt_muxid, sc_cobalt_sem_close, _sem);
	if (err < 0) {
		errno = -err;
		return -1;
	}
	if (err)
		free(sem);

	return 0;
}

COBALT_IMPL(int, sem_unlink, (const char *name))
{
	int err;

	err = -XENOMAI_SKINCALL1(__cobalt_muxid, sc_cobalt_sem_unlink, name);

	if (!err)
		return 0;

	errno = err;
	return -1;
}

int sem_init_np(sem_t *sem, int flags, unsigned int value)
{
	struct __shadow_sem *_sem = &((union cobalt_sem_union *)sem)->shadow_sem;
	int err;

	err = -XENOMAI_SKINCALL3(__cobalt_muxid,
				 sc_cobalt_sem_init_np, _sem, flags, value);
	if (!err)
		return 0;

	errno = err;
	return -1;
}

int sem_broadcast_np(sem_t *sem)
{
	struct __shadow_sem *_sem = &((union cobalt_sem_union *)sem)->shadow_sem;
	struct sem_dat *datp;
	long value;
	int err;

	if (_sem->magic != COBALT_SEM_MAGIC
	    && _sem->magic != COBALT_NAMED_SEM_MAGIC) {
		errno = EINVAL;
		return -1;
	}

	datp = sem_get_datp(_sem);

	value = atomic_long_read(&datp->value);
	if (value >= 0)
		return 0;

	err = -XENOMAI_SKINCALL1(__cobalt_muxid,
				 sc_cobalt_sem_broadcast_np, _sem);
	if (!err)
		return 0;

	errno = err;
	return -1;
}
