/*
 * Copyright (C) 2013 Philippe Gerum <rpm@xenomai.org>.
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
#include <cobalt/kernel/assert.h>
#include "internal.h"
#include "signal.h"
#include "thread.h"
#include "timer.h"
#include "clock.h"

static void *sigpending_mem;

static LIST_HEAD(sigpending_pool);

/*
 * How many signal notifications which may be pending at any given
 * time, except timers.  Cobalt signals are always thread directed,
 * and we assume that in practice, each signal number is processed by
 * a dedicated thread. We provide for up to three real-time signal
 * events to pile up, and a single notification pending for other
 * signals. Timers use a fast queuing logic maintaining a count of
 * overruns, and therefore do not consume any memory from this pool.
 */
#define __SIGPOOL_SIZE  (sizeof(struct cobalt_sigpending) *	\
			 (_NSIG + (SIGRTMAX - SIGRTMIN) * 2))

static int cobalt_signal_deliver(struct cobalt_thread *thread,
				 struct cobalt_sigpending *sigp,
				 int group)
{				/* nklocked, IRQs off */
	struct cobalt_sigwait_context *swc;
	struct xnthread_wait_context *wc;
	int sig, ret;

	sig = sigp->si.si_signo;
	XENO_BUGON(COBALT, sig < 1 || sig > _NSIG);

	/*
	 * Attempt to deliver the signal immediately to the initial
	 * target that waits for it.
	 */
	if (xnsynch_pended_p(&thread->sigwait)) {
		wc = xnthread_get_wait_context(&thread->threadbase);
		swc = container_of(wc, struct cobalt_sigwait_context, wc);
		if (sigismember(swc->set, sig))
			goto deliver;
	}

	/*
	 * If that does not work out and we are sending to a thread
	 * group, try to deliver to any thread from the same process
	 * waiting for that signal.
	 */
	if (!group || list_empty(&thread->process->sigwaiters))
		return 0;

	list_for_each_entry(thread, &thread->process->sigwaiters, signext) {
		wc = xnthread_get_wait_context(&thread->threadbase);
		swc = container_of(wc, struct cobalt_sigwait_context, wc);
		if (sigismember(swc->set, sig))
			goto deliver;
	}

	return 0;
deliver:
	cobalt_copy_siginfo(sigp->si.si_code, swc->si, &sigp->si);
	cobalt_call_extension(signal_deliver, &thread->extref,
			      ret, swc->si, sigp);
	xnthread_complete_wait(&swc->wc);
	xnsynch_wakeup_one_sleeper(&thread->sigwait);
	list_del(&thread->signext);

	/*
	 * This is an immediate delivery bypassing any queuing, so we
	 * have to release the sigpending data right away before
	 * leaving.
	 */
	if ((void *)sigp >= sigpending_mem &&
	    (void *)sigp < sigpending_mem + __SIGPOOL_SIZE)
		list_add_tail(&sigp->next, &sigpending_pool);

	return 1;
}

int cobalt_signal_send(struct cobalt_thread *thread,
		       struct cobalt_sigpending *sigp,
		       int group)
{				/* nklocked, IRQs off */
	struct list_head *sigq;
	int sig, ret;

	/* Can we deliver this signal immediately? */
	ret = cobalt_signal_deliver(thread, sigp, group);
	if (ret)
		return 0;	/* Yep, done. */

	/*
	 * Nope, attempt to queue it. We start by calling any Cobalt
	 * extension for queuing the signal first.
	 */
	if (cobalt_call_extension(signal_queue, &thread->extref, ret, sigp)) {
		if (ret < 0)
			return ret; /* Error. */
		if (ret > 0)
			return 0; /* Queuing done. */
	}

	sig = sigp->si.si_signo;
	sigq = thread->sigqueues + sig - 1;
	if (!list_empty(sigq)) {
		/* Queue non-rt signals only once. */
		if (sig < SIGRTMIN)
			return 0;
		/* Queue rt signal source only once (SI_TIMER). */
		if (!list_empty(&sigp->next))
			return 0;
	}

	sigaddset(&thread->sigpending, sig);
	list_add_tail(&sigp->next, sigq);

	return 0;
}
EXPORT_SYMBOL_GPL(cobalt_signal_send);

int cobalt_signal_send_pid(pid_t pid, struct cobalt_sigpending *sigp)
{				/* nklocked, IRQs off */
	struct cobalt_thread *thread;

	thread = cobalt_thread_find(pid);
	if (thread)
		return cobalt_signal_send(thread, sigp, 0);

	return -ESRCH;
}
EXPORT_SYMBOL_GPL(cobalt_signal_send_pid);

struct cobalt_sigpending *cobalt_signal_alloc(void)
{				/* nklocked, IRQs off */
	struct cobalt_sigpending *sigp;

	if (list_empty(&sigpending_pool)) {
		if (printk_ratelimit())
			printk(XENO_WARN "signal bucket pool underflows\n");
		return NULL;
	}

	sigp = list_get_entry(&sigpending_pool, struct cobalt_sigpending, next);
	INIT_LIST_HEAD(&sigp->next);

	return sigp;
}
EXPORT_SYMBOL_GPL(cobalt_signal_alloc);

void cobalt_signal_flush(struct cobalt_thread *thread)
{
	struct cobalt_sigpending *sigp, *tmp;
	struct list_head *sigq;
	int n;

	/*
	 * TCB is not accessible from userland anymore, no locking
	 * required.
	 */
	if (sigisemptyset(&thread->sigpending))
		return;

	for (n = 0; n < _NSIG; n++) {
		sigq = thread->sigqueues + n;
		if (list_empty(sigq))
			continue;
		/*
		 * sigpending blocks must be unlinked so that we
		 * detect this fact when deleting their respective
		 * owners.
		 */
		list_for_each_entry_safe(tmp, sigp, sigq, next)
			list_del_init(&sigp->next);
	}

	sigemptyset(&thread->sigpending);
}

static int signal_wait(sigset_t *set, xnticks_t timeout,
		       struct siginfo __user *u_si)
{
	struct cobalt_sigpending *sigp = NULL;
	struct cobalt_sigwait_context swc;
	int ret, sig, n, code, overrun;
	struct cobalt_thread *curr;
	unsigned long *p, *t, m;
	struct siginfo si, *sip;
	struct list_head *sigq;
	spl_t s;

	curr = cobalt_current_thread();
	XENO_BUGON(COBALT, curr == NULL);

	if (u_si && !access_wok(u_si, sizeof(*u_si)))
		return -EFAULT;

	xnlock_get_irqsave(&nklock, s);

check:
	if (sigisemptyset(&curr->sigpending))
		/* Most common/fast path. */
		goto wait;

	p = curr->sigpending.sig; /* pending */
	t = set->sig;		  /* tested */

	for (n = 0, sig = 0; n < _NSIG_WORDS; ++n) {
		m = *p++ & *t++;
		if (m == 0)
			continue;
		sig = ffz(~m) +  n *_NSIG_BPW + 1;
		break;
	}

	if (sig) {
		sigq = curr->sigqueues + sig - 1;
		if (list_empty(sigq)) {
			sigdelset(&curr->sigpending, sig);
			goto check;
		}
		sigp = list_get_entry(sigq, struct cobalt_sigpending, next);
		INIT_LIST_HEAD(&sigp->next); /* Mark sigp as unlinked. */
		if (list_empty(sigq))
			sigdelset(&curr->sigpending, sig);
		sip = &sigp->si;
		ret = 0;
		goto done;
	}

wait:
	if (timeout == XN_NONBLOCK) {
		ret = -EAGAIN;
		goto fail;
	}
	swc.set = set;
	swc.si = &si;
	xnthread_prepare_wait(&swc.wc);
	list_add_tail(&curr->signext, &curr->process->sigwaiters);
	ret = xnsynch_sleep_on(&curr->sigwait, timeout, XN_RELATIVE);
	if (ret) {
		list_del(&curr->signext);
		ret = ret & XNBREAK ? -EINTR : -EAGAIN;
		goto fail;
	}
	sig = si.si_signo;
	sip = &si;
done:
	 /*
	  * si_overrun raises a nasty issue since we have to
	  * collect+clear it atomically before we drop the lock,
	  * although we don't know in advance if any extension would
	  * use it along with the additional si_codes it may provide,
	  * but we must drop the lock before running the
	  * signal_copyinfo handler.
	  *
	  * Observing that si_overrun is likely the only "unstable"
	  * data from the signal information which might change under
	  * our feet while we copy the bits to userland, we collect it
	  * here from the atomic section for all unknown si_codes,
	  * then pass its value to the signal_copyinfo handler.
	  */
	switch (sip->si_code) {
	case SI_TIMER:
		overrun = cobalt_timer_deliver(sip->si_tid);
		break;
	case SI_USER:
	case SI_MESGQ:
		overrun = 0;
		break;
	default:
		overrun = sip->si_overrun;
		if (overrun)
			sip->si_overrun = 0;
	}

	xnlock_put_irqrestore(&nklock, s);

	if (u_si == NULL)
		goto out;	/* Return signo only. */

	/* Translate kernel codes for userland. */
	code = sip->si_code;
	if (code & __SI_MASK)
		code |= __SI_MASK;

	ret = __xn_put_user(sip->si_signo, &u_si->si_signo);
	ret |= __xn_put_user(sip->si_errno, &u_si->si_errno);
	ret |= __xn_put_user(code, &u_si->si_code);

	/*
	 * Copy the generic/standard siginfo bits to userland.
	 */
	switch (sip->si_code) {
	case SI_TIMER:
		ret |= __xn_put_user(sip->si_tid, &u_si->si_tid);
		ret |= __xn_put_user(sip->si_ptr, &u_si->si_ptr);
		ret |= __xn_put_user(overrun, &u_si->si_overrun);
		break;
	case SI_MESGQ:
		ret |= __xn_put_user(sip->si_ptr, &u_si->si_ptr);
		/* falldown wanted. */
	case SI_USER:
		ret |= __xn_put_user(sip->si_pid, &u_si->si_pid);
		ret |= __xn_put_user(sip->si_uid, &u_si->si_uid);
	}

	/* Allow an extended target to receive more data. */
	if (ret == 0)
		cobalt_call_extension(signal_copyinfo, &curr->extref,
				      ret, u_si, sip, overrun);
out:
	/*
	 * If we pulled the signal information from a sigpending
	 * block, release it to the free pool if applicable.
	 */
	if (sigp &&
	    (void *)sigp >= sigpending_mem &&
	    (void *)sigp < sigpending_mem + __SIGPOOL_SIZE) {
		xnlock_get_irqsave(&nklock, s);
		list_add_tail(&sigp->next, &sigpending_pool);
		xnlock_put_irqrestore(&nklock, s);
		/* no more ref. to sigp beyond this point. */
	}

	return ret ?: sig;
fail:
	xnlock_put_irqrestore(&nklock, s);

	return ret;
}

int cobalt_sigwait(const sigset_t __user *u_set, int __user *u_sig)
{
	sigset_t set;
	int sig;

	if (__xn_safe_copy_from_user(&set, u_set, sizeof(set)))
		return -EFAULT;

	sig = signal_wait(&set, XN_INFINITE, NULL);
	if (sig < 0)
		return sig;

	if (__xn_safe_copy_to_user(u_sig, &sig, sizeof(*u_sig)))
		return -EFAULT;

	return 0;
}

int cobalt_sigtimedwait(const sigset_t __user *u_set, struct siginfo __user *u_si,
			const struct timespec __user *u_timeout)
{
	struct timespec timeout;
	xnticks_t ticks;
	sigset_t set;

	if (__xn_safe_copy_from_user(&set, u_set, sizeof(set)))
		return -EFAULT;

	if (__xn_safe_copy_from_user(&timeout, u_timeout, sizeof(timeout)))
		return -EFAULT;

	if ((unsigned long)timeout.tv_nsec >= ONE_BILLION)
		return -EINVAL;

	ticks = ts2ns(&timeout);
	if (ticks++ == 0)
		ticks = XN_NONBLOCK;

	return signal_wait(&set, ticks, u_si);
}

int cobalt_sigwaitinfo(const sigset_t __user *u_set, struct siginfo __user *u_si)
{
	sigset_t set;

	if (__xn_safe_copy_from_user(&set, u_set, sizeof(set)))
		return -EFAULT;

	return signal_wait(&set, XN_INFINITE, u_si);
}

int cobalt_sigpending(sigset_t __user *u_set)
{
	struct cobalt_thread *curr;

	curr = cobalt_current_thread();
	XENO_BUGON(COBALT, curr == NULL);
	
	if (__xn_safe_copy_to_user(u_set, &curr->sigpending, sizeof(*u_set)))
		return -EFAULT;

	return 0;
}

int __cobalt_kill(struct cobalt_thread *thread, int sig, int group) /* nklocked, IRQs off */
{
	struct cobalt_sigpending *sigp;
	int ret = 0;

	/*
	 * We have undocumented pseudo-signals to suspend/resume/unblock
	 * threads, force them out of primary mode or even demote them
	 * to the weak scheduling class/priority. Process them early,
	 * before anyone can notice...
	 */
	switch(sig) {
	case 0:
		/* Check for existence only. */
		break;
	case SIGSUSP:
		/*
		 * All callers shall be tagged as conforming calls, so
		 * self-directed suspension can only happen from
		 * primary mode. Yummie.
		 */
		xnthread_suspend(&thread->threadbase, XNSUSP,
				 XN_INFINITE, XN_RELATIVE, NULL);
		if (&thread->threadbase == xnsched_current_thread() &&
		    xnthread_test_info(&thread->threadbase, XNBREAK))
			ret = EINTR;
		break;
	case SIGRESM:
		xnthread_resume(&thread->threadbase, XNSUSP);
		goto resched;
	case SIGRELS:
		xnthread_unblock(&thread->threadbase);
		goto resched;
	case SIGKICK:
		xnshadow_kick(&thread->threadbase);
		goto resched;
	case SIGDEMT:
		xnshadow_demote(&thread->threadbase);
		goto resched;
	case 1 ... _NSIG:
		sigp = cobalt_signal_alloc();
		if (sigp) {
			sigp->si.si_signo = sig;
			sigp->si.si_errno = 0;
			sigp->si.si_code = SI_USER;
			sigp->si.si_pid = current->pid;
			sigp->si.si_uid = current_uid();
			cobalt_signal_send(thread, sigp, group);
		}
	resched:
		xnsched_run();
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

int cobalt_kill(pid_t pid, int sig)
{
	struct cobalt_thread *thread;
	int ret;
	spl_t s;

	xnlock_get_irqsave(&nklock, s);

	thread = cobalt_thread_find(pid);
	if (thread == NULL)
		ret = -ESRCH;
	else
		ret = __cobalt_kill(thread, sig, 1);

	xnlock_put_irqrestore(&nklock, s);

	return ret;
}

int cobalt_signal_pkg_init(void)
{
	struct cobalt_sigpending *sigp;

	sigpending_mem = alloc_pages_exact(__SIGPOOL_SIZE, GFP_KERNEL);
	if (sigpending_mem == NULL)
		return -ENOMEM;

	for (sigp = sigpending_mem;
	     (void *)sigp < sigpending_mem + __SIGPOOL_SIZE; sigp++)
		list_add_tail(&sigp->next, &sigpending_pool);

	return 0;
}

void cobalt_signal_pkg_cleanup(void)
{
	free_pages_exact(sigpending_mem, __SIGPOOL_SIZE);
}
