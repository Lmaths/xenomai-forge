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

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
 */
#include <sys/types.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <stdarg.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "nocore/atomic.h"
#include "boilerplate/lock.h"
#include "boilerplate/time.h"
#include "boilerplate/scope.h"
#include "boilerplate/ancillaries.h"

pthread_mutex_t __printlock;

struct timespec __init_date;

void __printout(const char *name, const char *header,
		const char *fmt, va_list ap)
{
	unsigned long long ms, us, ns;
	struct timespec now, delta;
	FILE *fp = stderr;

	__RT(clock_gettime(CLOCK_MONOTONIC, &now));
	timespec_sub(&delta, &now, &__init_date);
	ns = delta.tv_sec * 1000000000ULL;
	ns += delta.tv_nsec;
	ms = ns / 1000000ULL;
	us = (ns % 1000000ULL) / 1000ULL;

	push_cleanup_lock(&__printlock);
	write_lock(&__printlock);

	fprintf(fp, "%4d\"%.3d.%.3d| ",
		(int)ms / 1000, (int)ms % 1000, (int)us);

	if (header)
		fputs(header, fp);

	fprintf(fp, "[%s] ", name ?: "main");
	vfprintf(fp, fmt, ap);
	fputc('\n', fp);
	fflush(fp);

	write_unlock(&__printlock);
	pop_cleanup_lock(&__printlock);
}

void __warning(const char *name, const char *fmt, va_list ap)
{
	__printout(name, "WARNING: ", fmt, ap);
}

void __panic(const char *name, const char *fmt, va_list ap)
{
	__printout(name, "BUG: ", fmt, ap);
	exit(1);
}

__attribute__ ((weak))
void error_hook(struct error_frame *ef) /* NULL in non-debug mode */
{
}

#define __esym_def(e)	[e] = #e

static const char *__esym_map[] = {
	[0] = "OK",
	__esym_def(EPERM),
	__esym_def(ENOENT),
	__esym_def(ESRCH),
	__esym_def(EINTR),
	__esym_def(EIO),
	__esym_def(ENXIO),
	__esym_def(E2BIG),
	__esym_def(ENOEXEC),
	__esym_def(EBADF),
	__esym_def(ECHILD),
	__esym_def(EAGAIN),
	__esym_def(ENOMEM),
	__esym_def(EACCES),
	__esym_def(EFAULT),
	__esym_def(ENOTBLK),
	__esym_def(EBUSY),
	__esym_def(EEXIST),
	__esym_def(EXDEV),
	__esym_def(ENODEV),
	__esym_def(ENOTDIR),
	__esym_def(EISDIR),
	__esym_def(EINVAL),
	__esym_def(ENFILE),
	__esym_def(EMFILE),
	__esym_def(ENOTTY),
	__esym_def(ETXTBSY),
	__esym_def(EFBIG),
	__esym_def(ENOSPC),
	__esym_def(ESPIPE),
	__esym_def(EROFS),
	__esym_def(EMLINK),
	__esym_def(EPIPE),
	__esym_def(EDOM),
	__esym_def(ERANGE),
	__esym_def(ENOSYS),
	__esym_def(ETIMEDOUT),
	__esym_def(ENOMSG),
	__esym_def(EADDRINUSE),
};

#define __esym_max  (sizeof(__esym_map) / sizeof(__esym_map[0]))

const char *symerror(int errnum)
{
	int v = -errnum;
	size_t ebufsz;
	char *ebuf;

	if (v < 0 || v >= (int)__esym_max || __esym_map[v] == NULL) {
		/* Catch missing codes in the error map. */
		ebuf = __get_error_buf(&ebufsz);
		snprintf(ebuf, ebufsz, "%d?", errnum);
		return ebuf;
	}

	return __esym_map[v];
}

void __run_cleanup_block(struct cleanup_block *cb)
{
	__RT(pthread_mutex_unlock(cb->lock));
	cb->handler(cb->arg);
}

__attribute__ ((weak))
void panic(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	__panic(NULL, fmt, ap);
	va_end(ap);
}

__attribute__ ((weak))
void warning(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	__warning(NULL, fmt, ap);
	va_end(ap);
}

char *generate_name(char *buf, const char *radix,
		    struct name_generator *ngen)
{
	int len = ngen->length - 1, tag;

	if (radix && *radix) {
		strncpy(buf, radix, len);
		buf[len] = '\0';
	} else {
		tag = atomic_add_fetch(ngen->serial, 1);
		snprintf(buf, len, "%s@%d", ngen->radix, tag);
	}

	return buf;
}

#ifdef CONFIG_XENO_PSHARED

/*
 * Client libraries may override these symbols for implementing heap
 * pointer validation in their own context (e.g. copperplate).
 */

__attribute__ ((weak))
int pshared_check(void *heap, void *addr)
{
	return 1;
}

__attribute__ ((weak)) void *__main_heap = NULL;

#endif /* !CONFIG_XENO_PSHARED */

#ifdef __XENO_DEBUG__

int __check_cancel_type(const char *locktype)
{
	int oldtype;

	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &oldtype);
	if (oldtype != PTHREAD_CANCEL_DEFERRED) {
		warning("%s_nocancel() section is NOT cancel-safe", locktype);
		return __bt(-EINVAL);
	}

	return 0;
}

#endif /* !__XENO_DEBUG__ */

static void __boilerplate_init(void)
{
	__RT(clock_gettime(CLOCK_MONOTONIC, &__init_date));
	__RT(pthread_mutex_init(&__printlock, NULL));
}

void boilerplate_init(void)
{
	static pthread_once_t once = PTHREAD_ONCE_INIT;
	pthread_once(&once, __boilerplate_init);
}
