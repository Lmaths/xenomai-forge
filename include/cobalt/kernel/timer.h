/**
 * @file
 * @note Copyright (C) 2001,2002,2003 Philippe Gerum <rpm@xenomai.org>.
 *
 * Xenomai is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * Xenomai is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Xenomai; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * \ingroup timer
 */

#ifndef _COBALT_KERNEL_TIMER_H
#define _COBALT_KERNEL_TIMER_H

#include <cobalt/kernel/clock.h>
#include <cobalt/kernel/stat.h>
#include <cobalt/kernel/list.h>

#define XN_INFINITE   ((xnticks_t)0)
#define XN_NONBLOCK   ((xnticks_t)-1)

/* Timer modes */
typedef enum xntmode {
	XN_RELATIVE,
	XN_ABSOLUTE,
	XN_REALTIME
} xntmode_t;

/* Timer status */
#define XNTIMER_DEQUEUED  0x00000001
#define XNTIMER_KILLED    0x00000002
#define XNTIMER_PERIODIC  0x00000004
#define XNTIMER_REALTIME  0x00000008
#define XNTIMER_FIRED     0x00000010
#define XNTIMER_NOBLCK	  0x00000020

/* These flags are available to the real-time interfaces */
#define XNTIMER_SPARE0  0x01000000
#define XNTIMER_SPARE1  0x02000000
#define XNTIMER_SPARE2  0x04000000
#define XNTIMER_SPARE3  0x08000000
#define XNTIMER_SPARE4  0x10000000
#define XNTIMER_SPARE5  0x20000000
#define XNTIMER_SPARE6  0x40000000
#define XNTIMER_SPARE7  0x80000000

/* Timer priorities */
#define XNTIMER_LOPRIO  (-999999999)
#define XNTIMER_STDPRIO 0
#define XNTIMER_HIPRIO  999999999

struct xntlholder {
	struct list_head link;
	xnticks_t key;
	int prio;
};

#define xntlholder_date(h)	((h)->key)
#define xntlholder_prio(h)	((h)->prio)
#define xntlist_init(q)		INIT_LIST_HEAD(q)
#define xntlist_empty(q)	list_empty(q)
#define xntlist_head(q)							\
	({								\
		struct xntlholder *h = list_empty(q) ? NULL :		\
			list_first_entry(q, struct xntlholder, link);	\
		h;							\
	})

#define xntlist_next(q, h)						\
	({								\
		struct xntlholder *_h = list_is_last(&h->link, q) ? NULL : \
			list_entry(h->link.next, struct xntlholder, link); \
		_h;							\
	})

static inline void xntlist_insert(struct list_head *q, struct xntlholder *holder)
{
	struct xntlholder *p;

	if (list_empty(q)) {
		list_add(&holder->link, q);
		return;
	}

	/*
	 * Insert the new timer at the proper place in the single
	 * queue. O(N) here, but this is the price for the increased
	 * flexibility...
	 */
	list_for_each_entry_reverse(p, q, link) {
		if ((xnsticks_t) (holder->key - p->key) > 0 ||
		    (holder->key == p->key && holder->prio <= p->prio))
		  break;
	}

	list_add(&holder->link, &p->link);
}

#define xntlist_remove(q, h)			\
	do {					\
		(void)(q);			\
		list_del(&(h)->link);		\
	} while (0)

#if defined(CONFIG_XENO_OPT_TIMER_HEAP)

#include <cobalt/kernel/bheap.h>

typedef bheaph_t xntimerh_t;

#define xntimerh_date(h)          bheaph_key(h)
#define xntimerh_prio(h)          bheaph_prio(h)
#define xntimerh_init(h)          bheaph_init(h)

typedef DECLARE_BHEAP_CONTAINER(xntimerq_t, CONFIG_XENO_OPT_TIMER_HEAP_CAPACITY);

#define xntimerq_init(q)          bheap_init((q), CONFIG_XENO_OPT_TIMER_HEAP_CAPACITY)
#define xntimerq_destroy(q)       bheap_destroy(q)
#define xntimerq_empty(q)         bheap_empty(q)
#define xntimerq_head(q)          bheap_gethead(q)
#define xntimerq_insert(q, h)     bheap_insert((q),(h))
#define xntimerq_remove(q, h)     bheap_delete((q),(h))

typedef struct {} xntimerq_it_t;

#define xntimerq_it_begin(q, i)   ((void) (i), bheap_gethead(q))
#define xntimerq_it_next(q, i, h) ((void) (i), bheap_next((q),(h)))

#else /* CONFIG_XENO_OPT_TIMER_LIST */

typedef struct xntlholder xntimerh_t;

#define xntimerh_date(h)       xntlholder_date(h)
#define xntimerh_prio(h)       xntlholder_prio(h)
#define xntimerh_init(h)       do { } while (0)

typedef struct list_head xntimerq_t;

#define xntimerq_init(q)        xntlist_init(q)
#define xntimerq_destroy(q)     do { } while (0)
#define xntimerq_empty(q)       xntlist_empty(q)
#define xntimerq_head(q)        xntlist_head(q)
#define xntimerq_insert(q,h)    xntlist_insert((q),(h))
#define xntimerq_remove(q, h)   xntlist_remove((q),(h))

typedef struct { } xntimerq_it_t;

#define xntimerq_it_begin(q,i)  ((void) (i), xntlist_head(q))
#define xntimerq_it_next(q,i,h) ((void) (i), xntlist_next((q),(h)))

#endif /* CONFIG_XENO_OPT_TIMER_LIST */

struct xnsched;

struct xntimerdata {
	xntimerq_t q;
};

static inline struct xntimerdata *
xnclock_percpu_timerdata(struct xnclock *clock, int cpu)
{
	return per_cpu_ptr(clock->timerdata, cpu);
}

static inline struct xntimerdata *
xnclock_this_timerdata(struct xnclock *clock)
{
	return __this_cpu_ptr(clock->timerdata);
}

struct xntimer {
#ifdef CONFIG_XENO_OPT_EXTCLOCK
	struct xnclock *clock;
#endif
	/** Link in timers list. */
	xntimerh_t aplink;
	struct list_head adjlink;
	/** Timer status. */
	unsigned long status;
	/** Periodic interval (raw ticks, 0 == one shot). */
	xnticks_t interval;
	/** Date of next periodic release point (raw ticks). */
	xnticks_t pexpect;
	/** Sched structure to which the timer is attached. */
	struct xnsched *sched;
	/** Timeout handler. */
	void (*handler)(struct xntimer *timer);
#ifdef CONFIG_XENO_OPT_STATS
#ifdef CONFIG_XENO_OPT_EXTCLOCK
	struct xnclock *tracker;
#endif
	/** Timer name to be displayed. */
	char name[XNOBJECT_NAME_LEN];
	/** Handler name to be displayed. */
	const char *handler_name;
	/** Timer holder in timebase. */
	struct list_head next_stat;
	/** Number of timer schedules. */
	xnstat_counter_t scheduled;
	/** Number of timer events. */
	xnstat_counter_t fired;
#endif /* CONFIG_XENO_OPT_STATS */
};

#ifdef CONFIG_XENO_OPT_EXTCLOCK

static inline struct xnclock *xntimer_clock(struct xntimer *timer)
{
	return timer->clock;
}

#else /* !CONFIG_XENO_OPT_EXTCLOCK */

static inline struct xnclock *xntimer_clock(struct xntimer *timer)
{
	return &nkclock;
}

#endif /* !CONFIG_XENO_OPT_EXTCLOCK */

#ifdef CONFIG_SMP
static inline struct xnsched *xntimer_sched(struct xntimer *timer)
{
	return timer->sched;
}
#else /* !CONFIG_SMP */
#define xntimer_sched(t)	xnsched_current()
#endif /* !CONFIG_SMP */

#define xntimer_percpu_queue(__timer)					\
	({								\
		struct xntimerdata *tmd;				\
		int cpu = xnsched_cpu((__timer)->sched);		\
		tmd = xnclock_percpu_timerdata(xntimer_clock(__timer), cpu); \
		&tmd->q;						\
	})

static inline xntimerq_t *xntimer_this_queue(struct xntimer *timer)
{
	struct xntimerdata *tmd;

	tmd = xnclock_this_timerdata(xntimer_clock(timer));

	return &tmd->q;
}

static inline xnticks_t xntimer_interval(struct xntimer *timer)
{
	return timer->interval;
}

static inline xnticks_t xntimer_pexpect(struct xntimer *timer)
{
	return timer->pexpect;
}

static inline xnticks_t xntimer_pexpect_forward(struct xntimer *timer,
						xnticks_t delta)
{
	return timer->pexpect += delta;
}

static inline void xntimer_set_priority(struct xntimer *timer,
					int prio)
{
	xntimerh_prio(&timer->aplink) = prio;
}

static inline int xntimer_active_p(struct xntimer *timer)
{
	return timer->sched != NULL;
}

static inline int xntimer_running_p(struct xntimer *timer)
{
	return (timer->status & XNTIMER_DEQUEUED) == 0;
}

static inline int xntimer_fired_p(struct xntimer *timer)
{
	return (timer->status & XNTIMER_FIRED) != 0;
}

static inline int xntimer_reload_p(struct xntimer *timer)
{
	return (timer->status &
		(XNTIMER_PERIODIC|XNTIMER_DEQUEUED|XNTIMER_KILLED)) ==
		(XNTIMER_PERIODIC|XNTIMER_DEQUEUED);
}

void __xntimer_init(struct xntimer *timer,
		    struct xnclock *clock,
		    void (*handler)(struct xntimer *timer),
		    struct xnthread *thread);

#ifdef CONFIG_XENO_OPT_STATS

#define xntimer_init(timer, clock, handler, thread)		\
	do {							\
		__xntimer_init(timer, clock, handler, thread);	\
		(timer)->handler_name = #handler;		\
	} while (0)

static inline void xntimer_reset_stats(struct xntimer *timer)
{
	xnstat_counter_set(&timer->scheduled, 0);
	xnstat_counter_set(&timer->fired, 0);
}

static inline void xntimer_account_scheduled(struct xntimer *timer)
{
	xnstat_counter_inc(&timer->scheduled);
}

static inline void xntimer_account_fired(struct xntimer *timer)
{
	xnstat_counter_inc(&timer->fired);
}

static inline void xntimer_set_name(struct xntimer *timer, const char *name)
{
	strncpy(timer->name, name, sizeof(timer->name));
}

#else /* !CONFIG_XENO_OPT_STATS */

#define xntimer_init	__xntimer_init

static inline void xntimer_reset_stats(struct xntimer *timer) { }

static inline void xntimer_account_scheduled(struct xntimer *timer) { }

static inline void xntimer_account_fired(struct xntimer *timer) { }

static inline void xntimer_set_name(struct xntimer *timer, const char *name) { }

#endif /* !CONFIG_XENO_OPT_STATS */

#if defined(CONFIG_XENO_OPT_EXTCLOCK) && defined(CONFIG_XENO_OPT_STATS)
void xntimer_switch_tracking(struct xntimer *timer,
			     struct xnclock *newclock);
#else
static inline
void xntimer_switch_tracking(struct xntimer *timer,
			     struct xnclock *newclock) { }
#endif

#define xntimer_init_noblock(timer, clock, handler, thread)	\
	do {							\
		xntimer_init(timer, clock, handler, thread);	\
		(timer)->status |= XNTIMER_NOBLCK;		\
	} while (0)

void xntimer_destroy(struct xntimer *timer);

/*!
 * \addtogroup timer
 *@{ */

int xntimer_start(struct xntimer *timer,
		  xnticks_t value,
		  xnticks_t interval,
		  xntmode_t mode);

void __xntimer_stop(struct xntimer *timer);

xnticks_t xntimer_get_date(struct xntimer *timer);

xnticks_t xntimer_get_timeout(struct xntimer *timer);

xnticks_t xntimer_get_interval(struct xntimer *timer);

int xntimer_heading_p(struct xntimer *timer);

static inline void xntimer_stop(struct xntimer *timer)
{
	if ((timer->status & XNTIMER_DEQUEUED) == 0)
		__xntimer_stop(timer);
}

static inline xnticks_t xntimer_get_timeout_stopped(struct xntimer *timer)
{
	return xntimer_get_timeout(timer);
}

static inline xnticks_t xntimer_get_expiry(struct xntimer *timer)
{
	return xntimerh_date(&timer->aplink);
}

static inline void xntimer_enqueue(struct xntimer *timer,
				   xntimerq_t *q)
{
	xntimerq_insert(q, &timer->aplink);
	timer->status &= ~XNTIMER_DEQUEUED;
	xntimer_account_scheduled(timer);
}

static inline void xntimer_dequeue(struct xntimer *timer,
				   xntimerq_t *q)
{
	xntimerq_remove(q, &timer->aplink);
	timer->status |= XNTIMER_DEQUEUED;
}

/*@}*/

void xntimer_init_proc(void);

void xntimer_cleanup_proc(void);

unsigned long long xntimer_get_overruns(struct xntimer *timer, xnticks_t now);

#ifdef CONFIG_SMP

void __xntimer_migrate(struct xntimer *timer, struct xnsched *sched);

static inline
void xntimer_migrate(struct xntimer *timer, struct xnsched *sched)
{				/* nklocked, IRQs off */
	if (timer->sched != sched)
		__xntimer_migrate(timer, sched);
}

int xntimer_setup_ipi(void);

void xntimer_release_ipi(void);

#else /* ! CONFIG_SMP */

static inline void xntimer_migrate(struct xntimer *timer,
				   struct xnsched *sched)
{ }

static inline int xntimer_setup_ipi(void)
{
	return 0;
}

static inline void xntimer_release_ipi(void) { }

#endif /* CONFIG_SMP */

static inline void xntimer_set_sched(struct xntimer *timer,
				     struct xnsched *sched)
{
	xntimer_migrate(timer, sched);
}

char *xntimer_format_time(xnticks_t ns,
			  char *buf, size_t bufsz);

int xntimer_grab_hardware(int cpu);

void xntimer_release_hardware(int cpu);

#endif /* !_COBALT_KERNEL_TIMER_H */
