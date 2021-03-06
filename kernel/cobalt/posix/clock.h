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
#ifndef _COBALT_POSIX_CLOCK_H
#define _COBALT_POSIX_CLOCK_H

#include <linux/types.h>
#include <linux/time.h>
#include <cobalt/uapi/time.h>

#define ONE_BILLION             1000000000

static inline void ns2ts(struct timespec *ts, xnticks_t nsecs)
{
	ts->tv_sec = xnclock_divrem_billion(nsecs, &ts->tv_nsec);
}

static inline xnticks_t ts2ns(const struct timespec *ts)
{
	xntime_t nsecs = ts->tv_nsec;

	if (ts->tv_sec)
		nsecs += (xntime_t)ts->tv_sec * ONE_BILLION;

	return nsecs;
}

static inline xnticks_t tv2ns(const struct timeval *tv)
{
	xntime_t nsecs = tv->tv_usec * 1000;

	if (tv->tv_sec)
		nsecs += (xntime_t)tv->tv_sec * ONE_BILLION;

	return nsecs;
}

static inline void ticks2tv(struct timeval *tv, xnticks_t ticks)
{
	unsigned long nsecs;

	tv->tv_sec = xnclock_divrem_billion(ticks, &nsecs);
	tv->tv_usec = nsecs / 1000;
}

static inline xnticks_t clock_get_ticks(clockid_t clock_id)
{
	return clock_id == CLOCK_REALTIME ?
		xnclock_read_realtime(&nkclock) :
		xnclock_read_monotonic(&nkclock);
}

static inline int clock_flag(int flag, clockid_t clock_id)
{
	switch(flag & TIMER_ABSTIME) {
	case 0:
		return XN_RELATIVE;

	case TIMER_ABSTIME:
		switch(clock_id) {
		case CLOCK_MONOTONIC:
		case CLOCK_MONOTONIC_RAW:
			return XN_ABSOLUTE;

		case CLOCK_REALTIME:
			return XN_REALTIME;
		}
	}
	return -EINVAL;
}

int cobalt_clock_getres(clockid_t clock_id,
			struct timespec __user *u_ts);

int cobalt_clock_gettime(clockid_t clock_id,
			 struct timespec __user *u_ts);

int cobalt_clock_settime(clockid_t clock_id,
			 const struct timespec __user *u_ts);

int cobalt_clock_nanosleep(clockid_t clock_id, int flags,
			   const struct timespec __user *u_rqt,
			   struct timespec __user *u_rmt);

int cobalt_clock_register(struct xnclock *clock,
			  clockid_t *clk_id);

void cobalt_clock_deregister(struct xnclock *clock);

extern DECLARE_BITMAP(cobalt_clock_extids, COBALT_MAX_EXTCLOCKS);

#endif /* !_COBALT_POSIX_CLOCK_H */
