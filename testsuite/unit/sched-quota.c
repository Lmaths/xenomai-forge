/*
 * SCHED_QUOTA test.
 *
 * Copyright (C) Philippe Gerum <rpm@xenomai.org>
 *
 * Released under the terms of GPLv2.
 *
 * --
 *
 * This test exercizes the SCHED_QUOTA scheduling policy. Using a pool
 * of SCHED_FIFO threads, the code first calibrates, by estimating how
 * much work the system under test can perform when running
 * uninterrupted over a second.
 *
 * The same thread pool is re-started afterwards, as a SCHED_QUOTA
 * group this time, which is allotted a user-definable percentage of
 * the global quota interval
 * (CONFIG_XENO_OPT_SCHED_QUOTA_PERIOD). Using the reference
 * calibration value obtained by running the SCHED_FIFO pool, the
 * percentage of runtime consumed by the SCHED_QUOTA group over a
 * second is calculated.
 *
 * A successful test shows that the effective percentage of runtime
 * observed with the SCHED_QUOTA group closely matches the allotted
 * quota (barring rounding errors and system latency).
 */
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <semaphore.h>
#include <sched.h>
#include <errno.h>
#include <error.h>
#include <boilerplate/time.h>

#define MAX_THREADS 8
#define TEST_SECS   1
#define ONE_BILLION 1000000000UL

static unsigned long long crunch_per_sec, loops_per_sec;

static pthread_t threads[MAX_THREADS];

static unsigned long counts[MAX_THREADS];

static int nrthreads;

static pthread_cond_t barrier;

static pthread_mutex_t lock;

static int started;

static sem_t ready;

static unsigned long __attribute__(( noinline ))
__do_work(unsigned long count)
{
	return count + 1;
}

static void __attribute__(( noinline ))
do_work(unsigned long loops, unsigned long *count_r)
{
	unsigned long n;

	for (n = 0; n < loops; n++)
		*count_r = __do_work(*count_r);
}

static void *thread_body(void *arg)
{
	unsigned long *count_r = arg, loops;
	int oldstate, oldtype;

	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &oldstate);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &oldtype);
	loops = crunch_per_sec / 100; /* yield each 10 ms runtime */
	*count_r = 0;
	sem_post(&ready);

	pthread_mutex_lock(&lock);
	for (;;) {
		if (started)
			break;
		pthread_cond_wait(&barrier, &lock);
	}
	pthread_mutex_unlock(&lock);

	for (;;) {
		do_work(loops, count_r);
		if (nrthreads > 1)
			sched_yield();
	}

	return NULL;
}

static void __create_quota_thread(pthread_t *tid, const char *name,
				  int tgid, unsigned long *count_r)
{
	struct sched_param_ex param_ex;
	pthread_attr_ex_t attr_ex;
	int ret;

	pthread_attr_init_ex(&attr_ex);
	pthread_attr_setdetachstate_ex(&attr_ex, PTHREAD_CREATE_JOINABLE);
	pthread_attr_setinheritsched_ex(&attr_ex, PTHREAD_EXPLICIT_SCHED);
	pthread_attr_setschedpolicy_ex(&attr_ex, SCHED_QUOTA);
	param_ex.sched_priority = 1;
	param_ex.sched_quota_group = tgid;
	pthread_attr_setschedparam_ex(&attr_ex, &param_ex);
	pthread_attr_setstacksize_ex(&attr_ex, PTHREAD_STACK_MIN * 2);
	ret = pthread_create_ex(tid, &attr_ex, thread_body, count_r);
	if (ret)
		error(1, ret, "pthread_create_ex(SCHED_QUOTA)");

	pthread_attr_destroy_ex(&attr_ex);
	pthread_set_name_np(*tid, name);
}

#define create_quota_thread(__tid, __label, __tgid, __count)	\
	__create_quota_thread(&(__tid), __label, __tgid, &(__count))

static void __create_fifo_thread(pthread_t *tid, const char *name,
				 unsigned long *count_r)
{
	struct sched_param param;
	pthread_attr_t attr;
	int ret;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
	pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
	param.sched_priority = 1;
	pthread_attr_setschedparam(&attr, &param);
	pthread_attr_setstacksize(&attr, PTHREAD_STACK_MIN * 2);
	ret = pthread_create(tid, &attr, thread_body, count_r);
	if (ret)
		error(1, ret, "pthread_create(SCHED_FIFO)");

	pthread_attr_destroy(&attr);
	pthread_set_name_np(*tid, name);
}

#define create_fifo_thread(__tid, __label, __count)	\
	__create_fifo_thread(&(__tid), __label, &(__count))

static double run_quota(int quota)
{
	size_t len = sched_quota_confsz();
	unsigned long long count;
	union sched_config cf;
	struct timespec req;
	int ret, tgid, n;
	double percent;
	char label[8];

	cf.quota.op = sched_quota_add;
	cf.quota.add.tgid_r = &tgid;
	ret = sched_setconfig_np(0, SCHED_QUOTA, &cf, len);
	if (ret)
		error(1, ret, "sched_setconfig_np(add-quota-group)");

	cf.quota.op = sched_quota_set;
	cf.quota.set.quota = quota;
	cf.quota.set.quota_peak = quota;
	cf.quota.set.tgid = tgid;
	ret = sched_setconfig_np(0, SCHED_QUOTA, &cf, len);
	if (ret)
		error(1, ret, "sched_setconfig_np(set-quota, tgid=%d)", tgid);

	printf("new thread group #%d on CPU0\n", tgid);

	for (n = 0; n < nrthreads; n++) {
		sprintf(label, "t%d", n);
		create_quota_thread(threads[n], label, tgid, counts[n]);
		sem_wait(&ready);
	}

	pthread_mutex_lock(&lock);
	started = 1;
	pthread_cond_broadcast(&barrier);
	pthread_mutex_unlock(&lock);

	req.tv_sec = TEST_SECS;
	req.tv_nsec = 0;
	clock_nanosleep(CLOCK_MONOTONIC, 0, &req, NULL);

	for (n = 0, count = 0; n < nrthreads; n++) {
		count += counts[n];
		pthread_kill(threads[n], SIGDEMT);
	}

	percent = ((double)count / TEST_SECS) * 100.0 / loops_per_sec;

	for (n = 0; n < nrthreads; n++) {
		__real_printf("done quota_thread[%d], count=%lu\n", n, counts[n]);
		pthread_cancel(threads[n]);
		pthread_join(threads[n], NULL);
	}

	cf.quota.op = sched_quota_remove;
	cf.quota.remove.tgid = tgid;
	ret = sched_setconfig_np(0, SCHED_QUOTA, &cf, len);
	if (ret)
		error(1, ret, "sched_setconfig_np(remove-quota-group)");

	return percent;
}

static unsigned long long calibrate(void)
{
	struct timespec start, end, delta;
	const int crunch_loops = 10000;
	unsigned long long ns, lps;
	unsigned long count;
	struct timespec req;
	char label[8];
	int n;

	count = 0;
	clock_gettime(CLOCK_MONOTONIC, &start);
	do_work(crunch_loops, &count);
	clock_gettime(CLOCK_MONOTONIC, &end);
	
	timespec_sub(&delta, &end, &start);
	ns = delta.tv_sec * ONE_BILLION + delta.tv_nsec;
	crunch_per_sec = (unsigned long long)((double)ONE_BILLION / (double)ns * crunch_loops);

	for (n = 0; n < nrthreads; n++) {
		sprintf(label, "t%d", n);
		create_fifo_thread(threads[n], label, counts[n]);
		sem_wait(&ready);
	}

	pthread_mutex_lock(&lock);
	started = 1;
	pthread_cond_broadcast(&barrier);
	pthread_mutex_unlock(&lock);

	req.tv_sec = 1;
	req.tv_nsec = 0;
	clock_nanosleep(CLOCK_MONOTONIC, 0, &req, NULL);

	for (n = 0, lps = 0; n < nrthreads; n++) {
		lps += counts[n];
		pthread_kill(threads[n], SIGDEMT);
	}

	for (n = 0; n < nrthreads; n++) {
		pthread_cancel(threads[n]);
		pthread_join(threads[n], NULL);
	}

	started = 0;

	return lps;
}

int main(int argc, char **argv)
{
	pthread_t me = pthread_self();
	struct sched_param param;
	sigset_t mask, oldmask;
	int ret, quota = 0;
	double effective;

	mlockall(MCL_CURRENT | MCL_FUTURE);

	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGTERM);
	sigaddset(&mask, SIGHUP);
	pthread_sigmask(SIG_BLOCK, &mask, &oldmask);
	pthread_mutex_init(&lock, NULL);
	pthread_cond_init(&barrier, NULL);
	sem_init(&ready, 0, 0);

	param.sched_priority = 50;
	ret = pthread_setschedparam(me, SCHED_FIFO, &param);
	if (ret)
		error(1, ret, "pthread_setschedparam");

	if (argc > 1)
		quota = atoi(argv[1]);
	if (quota <= 0)
		quota = 10;

	if (argc > 2)
		nrthreads = atoi(argv[2]);
	if (nrthreads <= 0)
		nrthreads = 3;
	if (nrthreads > MAX_THREADS)
		error(1, EINVAL, "max %d threads", MAX_THREADS);

	calibrate();	/* Warming up, ignore result. */
	loops_per_sec = calibrate();

	printf("calibrating: %Lu loops/sec\n", loops_per_sec);

	effective = run_quota(quota);
	__real_printf("%d thread%s: cap=%d%%, effective=%.1f%%\n",
		      nrthreads, nrthreads > 1 ? "s": "", quota, effective);

	return 0;
}
