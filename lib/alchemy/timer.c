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

#include <copperplate/threadobj.h>
#include "timer.h"

struct clockobj alchemy_clock;

/**
 * @fn RTIME rt_timer_read(void)
 * @brief Return the current system time.
 *
 * Return the current time maintained by the Xenomai core clock.
 *
 * @return The current time expressed in clock ticks (see note).
 *
 * Valid calling context:
 *
 * - Regular POSIX threads
 * - Xenomai threads
 *
 * @note The @a time value is a multiple of the Alchemy clock
 * resolution (see --alchemy-clock-resolution option, defaults to 1
 * nanosecond).
 */
RTIME rt_timer_read(void)
{
	ticks_t ticks;

	clockobj_get_time(&alchemy_clock, &ticks, NULL);

	return ticks;
}

/**
 * @fn RTIME rt_timer_ns2ticks(SRTIME ns)
 * @brief Convert nanoseconds to Alchemy clock ticks.
 *
 * Convert a count of nanoseconds to Alchemy clock ticks.  This
 * routine operates on signed nanosecond values. This is the converse
 * call to rt_timer_ticks2ns().
 *
 * @param ns The count of nanoseconds to convert.
 *
 * @return The corresponding value expressed in clock ticks of the
 * Alchemy clock. The resolution of the Alchemy clock can be set using
 * the --alchemy-clock-resolution option when starting the application
 * process (defaults to 1 nanosecond).
 *
 * Valid calling context:
 *
 * - Regular POSIX threads
 * - Xenomai threads
 */
SRTIME rt_timer_ns2ticks(SRTIME ns)
{
	return clockobj_ns_to_ticks(&alchemy_clock, ns);
}

/**
 * @fn RTIME rt_timer_ticks2ns(SRTIME ns)
 * @brief Convert Alchemy clock ticks to nanoseconds.
 *
 * Convert a count of Alchemy clock ticks to nanoseconds.  This
 * routine operates on signed nanosecond values. This is the converse
 * call to rt_timer_ns2ticks().
 *
 * @param ns The count of nanoseconds to convert.
 *
 * @return The corresponding value expressed in nanoseconds.  The
 * resolution of the Alchemy clock can be set using the
 * --alchemy-clock-resolution option when starting the application
 * process (defaults to 1 nanosecond).
 *
 * Valid calling context:
 *
 * - Regular POSIX threads
 * - Xenomai threads
 */
SRTIME rt_timer_ticks2ns(SRTIME ticks)
{
	return clockobj_ticks_to_ns(&alchemy_clock, ticks);
}

/**
 * @fn int rt_timer_inquire(RT_TIMER_INFO *info)
 * @brief Inquire about the Xenomai core timer.
 *
 * Return status information of the Xenomai core timer.
 *
 * @param info The address of a structure the status data will be
 * written to.
 *
 * @return This service always returns 0.
 *
 * Valid calling context:
 *
 * - Regular POSIX threads
 * - Xenomai threads
 */
int rt_timer_inquire(RT_TIMER_INFO *info)
{
	info->period = clockobj_get_resolution(&alchemy_clock);
	clockobj_get_time(&alchemy_clock, &info->date, &info->tsc);

	return 0;
}

/**
 * @fn void rt_timer_spin(RTIME ns)
 * @brief Busy wait burning CPU cycles.
 *
 * Enter a busy waiting loop for a count of nanoseconds.
 *
 * Since this service is always called with interrupts enabled, the
 * caller might be preempted by other real-time activities, therefore
 * the actual delay might be longer than specified.
 *
 * @param ns The time to wait expressed in nanoseconds.
 *
 * Valid calling context:
 *
 * - Regular POSIX threads
 * - Xenomai threads
 */
void rt_timer_spin(RTIME ns)
{
	threadobj_spin(ns);
}
