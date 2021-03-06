/**
 * @note Copyright (C) 2006-2011 Philippe Gerum <rpm@xenomai.org>.
 *
 * Xenomai is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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
 *
 * @ingroup nucleus
 * @defgroup clock Clock services.
 *
 * @{
 */
#include <linux/percpu.h>
#include <linux/errno.h>
#include <cobalt/kernel/sched.h>
#include <cobalt/kernel/timer.h>
#include <cobalt/kernel/clock.h>
#include <cobalt/kernel/arith.h>
#include <cobalt/kernel/vdso.h>
#include <asm/xenomai/calibration.h>

unsigned long nktimerlat;

atomic_t nkclklk;

static unsigned long long clockfreq;

#ifdef XNARCH_HAVE_LLMULSHFT

static unsigned int tsc_scale, tsc_shift;

#ifdef XNARCH_HAVE_NODIV_LLIMD

static struct xnarch_u32frac tsc_frac;
static struct xnarch_u32frac bln_frac;

long long xnclock_core_ns_to_ticks(long long ns)
{
	return xnarch_nodiv_llimd(ns, tsc_frac.frac, tsc_frac.integ);
}

unsigned long long xnclock_divrem_billion(unsigned long long value,
					  unsigned long *rem)
{
	unsigned long long q;
	unsigned r;

	q = xnarch_nodiv_ullimd(value, bln_frac.frac, bln_frac.integ);
	r = value - q * 1000000000;
	if (r >= 1000000000) {
		++q;
		r -= 1000000000;
	}
	*rem = r;
	return q;
}

#else /* !XNARCH_HAVE_NODIV_LLIMD */

long long xnclock_core_ns_to_ticks(long long ns)
{
	return xnarch_llimd(ns, 1 << tsc_shift, tsc_scale);
}

#endif /* !XNARCH_HAVE_NODIV_LLIMD */

xnsticks_t xnclock_core_ticks_to_ns(xnsticks_t ticks)
{
	return xnarch_llmulshft(ticks, tsc_scale, tsc_shift);
}

xnsticks_t xnclock_core_ticks_to_ns_rounded(xnsticks_t ticks)
{
	unsigned int shift = tsc_shift - 1;
	return (xnarch_llmulshft(ticks, tsc_scale, shift) + 1) / 2;
}

#else  /* !XNARCH_HAVE_LLMULSHFT */

xnsticks_t xnclock_core_ticks_to_ns(xnsticks_t ticks)
{
	return xnarch_llimd(ticks, 1000000000, clockfreq);
}

xnsticks_t xnclock_core_ticks_to_ns_rounded(xnsticks_t ticks)
{
	return (xnarch_llimd(ticks, 1000000000, clockfreq/2) + 1) / 2;
}

xnsticks_t xnclock_core_ns_to_ticks(xnsticks_t ns)
{
	return xnarch_llimd(ns, clockfreq, 1000000000);
}

#endif /* !XNARCH_HAVE_LLMULSHFT */

#ifndef XNARCH_HAVE_NODIV_LLIMD
unsigned long long xnclock_divrem_billion(unsigned long long value,
					  unsigned long *rem)
{
	return xnarch_ulldiv(value, 1000000000, rem);

}
#endif /* !XNARCH_HAVE_NODIV_LLIMD */

EXPORT_SYMBOL_GPL(xnclock_core_ticks_to_ns);
EXPORT_SYMBOL_GPL(xnclock_core_ticks_to_ns_rounded);
EXPORT_SYMBOL_GPL(xnclock_core_ns_to_ticks);
EXPORT_SYMBOL_GPL(xnclock_divrem_billion);

void xnclock_core_local_shot(struct xnsched *sched)
{
	struct xntimerdata *tmd;
	struct xntimer *timer;
	xnsticks_t delay;
	xntimerq_it_t it;
	xntimerh_t *h;

	/*
	 * Do not reprogram locally when inside the tick handler -
	 * will be done on exit anyway. Also exit if there is no
	 * pending timer.
	 */
	if (sched->status & XNINTCK)
		return;

	tmd = xnclock_this_timerdata(&nkclock);
	h = xntimerq_it_begin(&tmd->q, &it);
	if (h == NULL)
		return;

	/*
	 * Here we try to defer the host tick heading the timer queue,
	 * so that it does not preempt a real-time activity uselessly,
	 * in two cases:
	 *
	 * 1) a rescheduling is pending for the current CPU. We may
	 * assume that a real-time thread is about to resume, so we
	 * want to move the host tick out of the way until the host
	 * kernel resumes, unless there is no other outstanding
	 * timers.
	 *
	 * 2) the current thread is running in primary mode, in which
	 * case we may also defer the host tick until the host kernel
	 * resumes.
	 *
	 * The host tick deferral is cleared whenever Xenomai is about
	 * to yield control to the host kernel (see __xnsched_run()),
	 * or a timer with an earlier timeout date is scheduled,
	 * whichever comes first.
	 */
	sched->lflags &= ~XNHDEFER;
	timer = container_of(h, struct xntimer, aplink);
	if (unlikely(timer == &sched->htimer)) {
		if (xnsched_resched_p(sched) ||
		    !xnthread_test_state(sched->curr, XNROOT)) {
			h = xntimerq_it_next(&tmd->q, &it, h);
			if (h) {
				sched->lflags |= XNHDEFER;
				timer = container_of(h, struct xntimer, aplink);
			}
		}
	}

	delay = xntimerh_date(&timer->aplink) -
		(xnclock_core_read_raw() + nkclock.gravity);

	if (delay < 0)
		delay = 0;
	else if (delay > ULONG_MAX)
		delay = ULONG_MAX;

	xntrace_tick((unsigned)delay);

	ipipe_timer_set(delay);
}

#ifdef CONFIG_SMP
void xnclock_core_remote_shot(struct xnsched *sched)
{
	cpumask_t mask = cpumask_of_cpu(xnsched_cpu(sched));
	ipipe_send_ipi(IPIPE_HRTIMER_IPI, mask);
}
#endif

static void adjust_timer(struct xntimer *timer, xntimerq_t *q,
			 xnsticks_t delta)
{
	struct xnclock *clock = xntimer_clock(timer);
	xnticks_t period, mod;
	xnsticks_t diff;

	xntimerh_date(&timer->aplink) -= delta;

	if ((timer->status & XNTIMER_PERIODIC) == 0)
		goto enqueue;

	period = xntimer_interval(timer);
	timer->pexpect -= delta;
	diff = xnclock_read_raw(clock) - xntimerh_date(&timer->aplink);

	if ((xnsticks_t) (diff - period) >= 0) {
		/*
		 * Timer should tick several times before now, instead
		 * of calling timer->handler several times, we change
		 * the timer date without changing its pexpect, so
		 * that timer will tick only once and the lost ticks
		 * will be counted as overruns.
		 */
		mod = xnarch_mod64(diff, period);
		xntimerh_date(&timer->aplink) += diff - mod;
	} else if (delta < 0
		   && (timer->status & XNTIMER_FIRED)
		   && (xnsticks_t) (diff + period) <= 0) {
		/*
		 * Timer is periodic and NOT waiting for its first
		 * shot, so we make it tick sooner than its original
		 * date in order to avoid the case where by adjusting
		 * time to a sooner date, real-time periodic timers do
		 * not tick until the original date has passed.
		 */
		mod = xnarch_mod64(-diff, period);
		xntimerh_date(&timer->aplink) += diff + mod;
		timer->pexpect += diff + mod;
	}

enqueue:
	xntimer_enqueue(timer, q);
}

static void adjust_clock_timers(struct xnclock *clock, xnsticks_t delta)
{
	struct xntimer *timer, *tmp;
	struct list_head adjq;
	struct xnsched *sched;
	xntimerq_it_t it;
	unsigned int cpu;
	xntimerh_t *h;
	xntimerq_t *q;

	INIT_LIST_HEAD(&adjq);
	delta = xnclock_ns_to_ticks(clock, delta);

	for_each_online_cpu(cpu) {
		sched = xnsched_struct(cpu);
		q = &xnclock_percpu_timerdata(clock, cpu)->q;

		for (h = xntimerq_it_begin(q, &it); h;
		     h = xntimerq_it_next(q, &it, h)) {
			timer = container_of(h, struct xntimer, aplink);
			if (timer->status & XNTIMER_REALTIME)
				list_add_tail(&timer->adjlink, &adjq);
		}

		if (list_empty(&adjq))
			continue;

		list_for_each_entry_safe(timer, tmp, &adjq, adjlink) {
			list_del(&timer->adjlink);
			xntimer_dequeue(timer, q);
			adjust_timer(timer, q, delta);
		}

		if (sched != xnsched_current())
			xnclock_remote_shot(clock, sched);
		else
			xnclock_program_shot(clock, sched);
	}
}

/**
 * @fn void xnclock_adjust(struct xnclock *clock, xnsticks_t delta)
 * @brief Adjust a clock time.
 *
 * This service changes the epoch for the given clock by applying the
 * specified tick delta on its wallclock offset.
 *
 * @param clock The clock to adjust.
 *
 * @param delta The adjustment value expressed in nanoseconds.
 *
 * @note This routine must be entered nklock locked, interrupts off.
 *
 * @remark Tags: none.
 *
 * @note Xenomai tracks the system time in @a nkclock, as a
 * monotonously increasing count of ticks since the epoch. The epoch
 * is initially the same as the underlying machine time.
 */
void xnclock_adjust(struct xnclock *clock, xnsticks_t delta)
{
	xnticks_t now;

	nkclock.wallclock_offset += delta;
	nkvdso->wallclock_offset = nkclock.wallclock_offset;
	now = xnclock_read_monotonic(clock) + nkclock.wallclock_offset;
	adjust_clock_timers(clock, delta);

	trace_mark(xn_nucleus, clock_adjust, "clock %s, delta %Lu",
		   clock->name, delta);
}
EXPORT_SYMBOL_GPL(xnclock_adjust);

xnticks_t xnclock_get_host_time(void)
{
	struct timeval tv;
	do_gettimeofday(&tv);
	return tv.tv_sec * 1000000000ULL + tv.tv_usec * 1000;
}
EXPORT_SYMBOL_GPL(xnclock_get_host_time);

xnticks_t xnclock_core_read_monotonic(void)
{
	return xnclock_core_ticks_to_ns(xnclock_core_read_raw());
}
EXPORT_SYMBOL_GPL(xnclock_core_read_monotonic);

#ifdef CONFIG_XENO_OPT_STATS

static struct xnvfile_directory clock_vfroot;

static struct xnvfile_snapshot_ops vfile_clock_ops;

struct vfile_clock_priv {
	struct xntimer *curr;
};

struct vfile_clock_data {
	int cpu;
	unsigned int scheduled;
	unsigned int fired;
	xnticks_t timeout;
	xnticks_t interval;
	unsigned long status;
	char handler[XNOBJECT_NAME_LEN];
	char name[XNOBJECT_NAME_LEN];
};

static int clock_vfile_rewind(struct xnvfile_snapshot_iterator *it)
{
	struct vfile_clock_priv *priv = xnvfile_iterator_priv(it);
	struct xnclock *clock = xnvfile_priv(it->vfile);

	if (list_empty(&clock->statq))
		return -ESRCH;

	priv->curr = list_first_entry(&clock->statq, struct xntimer, next_stat);

	return clock->nrtimers;
}

static int clock_vfile_next(struct xnvfile_snapshot_iterator *it, void *data)
{
	struct vfile_clock_priv *priv = xnvfile_iterator_priv(it);
	struct xnclock *clock = xnvfile_priv(it->vfile);
	struct vfile_clock_data *p = data;
	struct xntimer *timer;

	if (priv->curr == NULL)
		return 0;

	timer = priv->curr;
	if (list_is_last(&timer->next_stat, &clock->statq))
		priv->curr = NULL;
	else
		priv->curr = list_entry(timer->next_stat.next,
					struct xntimer, next_stat);

	if (clock == &nkclock && xnstat_counter_get(&timer->scheduled) == 0)
		return VFILE_SEQ_SKIP;

	p->cpu = xnsched_cpu(xntimer_sched(timer));
	p->scheduled = xnstat_counter_get(&timer->scheduled);
	p->fired = xnstat_counter_get(&timer->fired);
	p->timeout = xntimer_get_timeout(timer);
	p->interval = xntimer_get_interval(timer);
	p->status = timer->status;
	strncpy(p->handler, timer->handler_name, 16)[16] = '\0';
	xnobject_copy_name(p->name, timer->name);

	return 1;
}

static int clock_vfile_show(struct xnvfile_snapshot_iterator *it, void *data)
{
	struct vfile_clock_data *p = data;
	char timeout_buf[]  = "-         ";
	char interval_buf[] = "-         ";
	char hit_buf[32];

	if (p == NULL)
		xnvfile_printf(it,
			       "%-3s  %-20s  %-10s  %-10s  %-16s  %s\n",
			       "CPU", "SCHED/SHOT", "TIMEOUT",
			       "INTERVAL", "HANDLER", "NAME");
	else {
		if ((p->status & XNTIMER_DEQUEUED) == 0)
			xntimer_format_time(p->timeout, timeout_buf,
					    sizeof(timeout_buf));
		if (p->status & XNTIMER_PERIODIC)
			xntimer_format_time(p->interval, interval_buf,
					    sizeof(interval_buf));
		snprintf(hit_buf, sizeof(hit_buf), "%u/%u",
			 p->scheduled, p->fired);
		xnvfile_printf(it,
			       "%-3u  %-20s  %-10s  %-10s  %-16s  %s\n",
			       p->cpu, hit_buf, timeout_buf,
			       interval_buf, p->handler, p->name);
	}

	return 0;
}

static struct xnvfile_snapshot_ops vfile_clock_ops = {
	.rewind = clock_vfile_rewind,
	.next = clock_vfile_next,
	.show = clock_vfile_show,
};

static void init_clock_proc(struct xnclock *clock)
{
	memset(&clock->vfile, 0, sizeof(clock->vfile));
	clock->vfile.privsz = sizeof(struct vfile_clock_priv);
	clock->vfile.datasz = sizeof(struct vfile_clock_data);
	clock->vfile.tag = &clock->revtag;
	clock->vfile.ops = &vfile_clock_ops;

	xnvfile_init_snapshot(clock->name, &clock->vfile, &clock_vfroot);
	xnvfile_priv(&clock->vfile) = clock;
}

static void cleanup_clock_proc(struct xnclock *clock)
{
	xnvfile_destroy_snapshot(&clock->vfile);
}

void xnclock_init_proc(void)
{
	xnvfile_init_dir("clock", &clock_vfroot, &nkvfroot);
}

void xnclock_cleanup_proc(void)
{
	xnvfile_destroy_dir(&clock_vfroot);
}

#else  /* !CONFIG_XENO_OPT_STATS */

static inline void init_clock_proc(struct xnclock *clock) { }

static inline void cleanup_clock_proc(struct xnclock *clock) { }

#endif	/* !CONFIG_XENO_OPT_STATS */

/**
 * @fn void xnclock_register(struct xnclock *clock)
 * @brief Register a Xenomai clock.
 *
 * This service installs a new clock which may be used to drive
 * Xenomai timers.
 *
 * @param clock The new clock to register.
 *
 * @remark Tags: secondary-only.
 */
int xnclock_register(struct xnclock *clock)
{
	struct xntimerdata *tmd;
	int cpu;

	secondary_mode_only();

	trace_mark(xn_nucleus, clock_register, "clock %s", clock->name);

	/* Allocate the percpu timer queue slot. */
	clock->timerdata = alloc_percpu(struct xntimerdata);
	if (clock->timerdata == NULL)
		return -ENOMEM;

	for_each_online_cpu(cpu) {
		tmd = xnclock_percpu_timerdata(clock, cpu);
		xntimerq_init(&tmd->q);
	}

#ifdef CONFIG_XENO_OPT_STATS
	INIT_LIST_HEAD(&clock->statq);
#endif /* CONFIG_XENO_OPT_STATS */

	init_clock_proc(clock);

	return 0;
}
EXPORT_SYMBOL_GPL(xnclock_register);

/**
 * @fn void xnclock_deregister(struct xnclock *clock)
 * @brief Deregister a Xenomai clock.
 *
 * This service uninstalls a Xenomai clock previously registered with
 * xnclock_register().
 *
 * This service may be called once all timers driven by @a clock have
 * been stopped.
 *
 * @param clock The clock to deregister.
 *
 * @remark Tags: secondary-only.
 */
void xnclock_deregister(struct xnclock *clock)
{
	struct xntimerdata *tmd;
	int cpu;

	secondary_mode_only();

	trace_mark(xn_nucleus, clock_deregister, "clock %s", clock->name);

	cleanup_clock_proc(clock);

	for_each_online_cpu(cpu) {
		tmd = xnclock_percpu_timerdata(clock, cpu);
		XENO_BUGON(NUCLEUS, !xntimerq_empty(&tmd->q));
		xntimerq_destroy(&tmd->q);
	}

	free_percpu(clock->timerdata);
}
EXPORT_SYMBOL_GPL(xnclock_deregister);

/**
 * @fn void xnclock_tick(struct xnclock *clock)
 * @brief Process a clock tick.
 *
 * This routine processes an incoming @a clock event, firing elapsed
 * timers as appropriate.
 *
 * @param clock The clock for which a new event was received.
 *
 * @remark Tags: primary-only, isr-only, atomic-entry.
 *
 * @note The current CPU must be part of the real-time affinity set,
 * otherwise weird things may happen.
 */
void xnclock_tick(struct xnclock *clock)
{
	xntimerq_t *timerq = &xnclock_this_timerdata(clock)->q;
	struct xnsched *sched = xnsched_current();
	xnticks_t now, interval;
	struct xntimer *timer;
	xnsticks_t delta;
	xntimerh_t *h;

	/*
	 * Optimisation: any local timer reprogramming triggered by
	 * invoked timer handlers can wait until we leave the tick
	 * handler. Use this status flag as hint to xntimer_start().
	 */
	sched->status |= XNINTCK;

	now = xnclock_read_raw(clock);
	while ((h = xntimerq_head(timerq)) != NULL) {
		timer = container_of(h, struct xntimer, aplink);
		/*
		 * If the delay to the next shot is greater than the
		 * clock gravity value, we may stop scanning the timer
		 * queue, since timeout dates are ordered by
		 * increasing values.
		 *
		 * (*) The gravity gives the amount of time expressed
		 * in clock ticks, by which we should anticipate the
		 * next shot. For instance, this value is equal to the
		 * typical latency observed on an idle system for
		 * Xenomai's core clock (nkclock).
		 */
		delta = (xnsticks_t)(xntimerh_date(&timer->aplink) - now);
		if (delta > (xnsticks_t)clock->gravity)
			break;

		trace_mark(xn_nucleus, timer_expire, "timer %p", timer);

		xntimer_dequeue(timer, timerq);
		xntimer_account_fired(timer);

		/*
		 * By postponing the propagation of the low-priority
		 * host tick to the interrupt epilogue (see
		 * xnintr_irq_handler()), we save some I-cache, which
		 * translates into precious microsecs on low-end hw.
		 */
		if (unlikely(timer == &sched->htimer)) {
			sched->lflags |= XNHTICK;
			sched->lflags &= ~XNHDEFER;
			if (timer->status & XNTIMER_PERIODIC)
				goto advance;
			continue;
		}

		/* Check for a locked clock state (i.e. ptracing). */
		if (unlikely(atomic_read(&nkclklk) > 0)) {
			if (timer->status & XNTIMER_NOBLCK)
				goto fire;
			if (timer->status & XNTIMER_PERIODIC)
				goto advance;
			/*
			 * We have no period for this blocked timer,
			 * so have it tick again at a reasonably close
			 * date in the future, waiting for the clock
			 * to be unlocked at some point. Since clocks
			 * are blocked when single-stepping into an
			 * application using a debugger, it is fine to
			 * wait for 250 ms for the user to continue
			 * program execution.
			 */
			interval = xnclock_ns_to_ticks(clock, 250000000ULL);
			goto requeue;
		}
	fire:
		timer->handler(timer);
		now = xnclock_read_raw(clock);
		timer->status |= XNTIMER_FIRED;
		/*
		 * If the elapsed timer has no reload value, or was
		 * re-queued or killed by the timeout handler: do not
		 * re-queue it for the next shot.
		 */
		if (!xntimer_reload_p(timer))
			continue;
	advance:
		interval = timer->interval;
	requeue:
		do
			xntimerh_date(&timer->aplink) += interval;
		while (xntimerh_date(&timer->aplink) < now + clock->gravity);
#ifdef CONFIG_SMP
		/*
		 * Make sure to pick the right percpu queue, in case
		 * the timer was migrated over its timeout
		 * handler. Since this timer was dequeued,
		 * xntimer_migrate() did not kick the remote CPU, so
		 * we have to do this now if required.
		 */
		if (unlikely(timer->sched != sched)) {
			timerq = xntimer_percpu_queue(timer);
			xntimer_enqueue(timer, timerq);
			if (xntimer_heading_p(timer))
				xnclock_remote_shot(clock, timer->sched);
			continue;
		}
#endif
		xntimer_enqueue(timer, timerq);
	}

	sched->status &= ~XNINTCK;

	xnclock_program_shot(clock, sched);
}
EXPORT_SYMBOL_GPL(xnclock_tick);

struct xnclock nkclock = {
	.name = "coreclk",
	.resolution = 1,	/* nanosecond. */
	.id = -1,
};
EXPORT_SYMBOL_GPL(nkclock);

void xnclock_cleanup(void)
{
	xnclock_deregister(&nkclock);
}

int __init xnclock_init(unsigned long long freq)
{
	xnticks_t schedlat;

	clockfreq = freq;
#ifdef XNARCH_HAVE_LLMULSHFT
	xnarch_init_llmulshft(1000000000, freq, &tsc_scale, &tsc_shift);
#ifdef XNARCH_HAVE_NODIV_LLIMD
	xnarch_init_u32frac(&tsc_frac, 1 << tsc_shift, tsc_scale);
	xnarch_init_u32frac(&bln_frac, 1, 1000000000);
#endif
#endif
	nktimerlat = xnarch_timer_calibrate();
	schedlat = xnarch_get_sched_latency();
	nkclock.gravity = xnclock_ns_to_ticks(&nkclock, schedlat) + nktimerlat;
	xnclock_register(&nkclock);

	return 0;
}

/*@}*/
