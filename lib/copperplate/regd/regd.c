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
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
 */

#include <sys/types.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/timerfd.h>
#include <sys/un.h>
#include <signal.h>
#include <stdio.h>
#include <limits.h>
#include <memory.h>
#include <malloc.h>
#include <getopt.h>
#include <string.h>
#include <error.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <boilerplate/list.h>
#include <boilerplate/hash.h>
#include "../internal.h"
#include "sysregfs.h"

#define DEFAULT_ROOT "/mnt/xenomai/anon"

/* Allow use of oldish umount2(). */
#ifndef MNT_DETACH
#define MNT_DETACH 0
#endif

#define note(fmt, args...)					\
	do {							\
  		if (!daemonize)					\
			printf("regd: " fmt "\n", ##args);	\
	} while (0)

static const char *rootdir = DEFAULT_ROOT;

static int sockfd;

static char *sysroot;

static int daemonize;

static int linger;

struct client {
	char *mountpt;
	int sockfd;
	struct pvholder next;
};

static DEFINE_PRIVATE_LIST(client_list);

static void usage(void)
{
	fprintf(stderr, "usage: regd [--root=<dir>]   set registry root directory\n");
	fprintf(stderr, "            [--daemonize]    run in the background\n");
	fprintf(stderr, "            [--linger]       disable timed exit on idleness\n");
}

static const struct option options[] = {
	{
#define help_opt	0
		.name = "help",
		.has_arg = 0,
		.flag = NULL,
	},
	{
#define daemonize_opt	1
		.name = "daemonize",
		.has_arg = 0,
		.flag = &daemonize,
		.val = 1,
	},
	{
#define root_opt	2
		.name = "root",
		.has_arg = 1,
		.flag = NULL,
	},
	{
#define linger_opt	3
		.name = "linger",
		.has_arg = 0,
		.flag = NULL,
		.val = 1,
	},
	{
		.name = NULL,
	},
};

static int create_directory_recursive(const char *dir) /* absolute path */
{
	char *s, *p;
	int ret;

	ret = chdir("/");
	if (ret)	/* That should better work... */
		error(1, errno, "chdir(\"/\")");

	s = strdup(dir);
	if (s == NULL)
		return -ENOMEM;

	p = strtok(s + 1, "/");
	while (p) {
		if (*p == '\0')
			goto next;
		ret = access(p, R_OK|W_OK|X_OK);
		if (ret) {
			ret = mkdir(p, 0755);
			if (ret && errno != EEXIST)
				return -errno;
		}
		ret = chdir(p);
		if (ret)
			return -errno;
	next:
		p = strtok(NULL, "/");
	}

	free(s);
	chdir(rootdir);		/* Back to rootdir */

	return 0;
}

static void create_rootdir(void)
{
	int ret;

	if (*rootdir != '/')
		error(1, EINVAL, "absolute root directory path required");
		
	ret = create_directory_recursive(rootdir);
	if (ret)
		error(1, -ret, "create_directory_recursive(\"%s\")", rootdir);
}

/*
 * Attempt to bind a local domain socket to some address in the
 * abstract namespace, for allowing clients to register. This way, we
 * won't have to suffer socket node left overs, which are a pain to
 * deal with racelessly.
 *
 * The address is a hash of the root directory we have been told to
 * maintain.
 */
static void bind_socket(void)
{
	struct sockaddr_un sun;
	unsigned int hash;
	socklen_t addrlen;
	int ret;

	sockfd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if (sockfd < 0)
		error(1, errno, "bind_socket/socket");

	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	hash = __hash_key(rootdir, strlen(rootdir), 0);
	snprintf(sun.sun_path, sizeof(sun.sun_path), "X%X-xenomai", hash);
	addrlen = offsetof(struct sockaddr_un, sun_path) + strlen(sun.sun_path);
	sun.sun_path[0] = '\0';
	ret = bind(sockfd, (struct sockaddr *)&sun, addrlen);
	if (ret) {
		if (errno == EADDRINUSE)
			exit(0);
		error(1, errno, "bind_socket/bind");
	}

	ret = listen(sockfd, SOMAXCONN);
	if (ret)
		error(1, errno, "bind_socket/listen");
}

static int register_client(int s)
{
	struct ucred ucred;
	struct client *c;
	socklen_t optlen;
	char *mountpt;
	int ret;

	optlen = sizeof(ucred);
	ret = getsockopt(s, SOL_SOCKET, SO_PEERCRED, &ucred, &optlen);
	if (ret)
		return -errno;

	c = malloc(sizeof(*c));
	if (c == NULL)
		return -ENOMEM;

	/*
	 * The registry mount point for a client will be
	 * <rootdir>/pid.
	 */
	ret = asprintf(&mountpt, "%s/%d", rootdir, ucred.pid);
	if (ret < 0) {
		ret = -ENOMEM;
		goto fail_nopath;
	}

	ret = create_directory_recursive(mountpt);
	if (ret) {
		note("failed creating mount point %s", mountpt);
		goto fail;
	}

	note("created mount point %s", mountpt);

	/* Send the mount point back to the client. */
	ret = send(s, mountpt, strlen(mountpt) + 1, 0);
	if (ret < 0)
		goto fail;

	c->mountpt = mountpt;
	c->sockfd = s;
	pvlist_append(&c->next, &client_list);

	return 0;
fail:
	free(mountpt);
fail_nopath:
	free(c);

	return ret;
}

static void unregister_client(int s)
{
	struct client *c;

	pvlist_for_each_entry(c, &client_list, next) {
		if (c->sockfd == s) {
			pvlist_remove(&c->next);
			note("deleting mount point %s", c->mountpt);
			umount2(c->mountpt, MNT_DETACH);
			rmdir(c->mountpt);
			free(c->mountpt);
			free(c);
			return;
		}
	}
}

static void delete_system_fs(void)
{
	umount2(sysroot, MNT_DETACH);
	rmdir(sysroot);
}

static void handle_requests(void)
{
	struct itimerspec its;
	fd_set refset, set;
	int ret, s, tmfd;
	uint64_t exp;
	char c;

	FD_ZERO(&refset);
	FD_SET(sockfd, &refset);

	if (!linger) {
		tmfd = timerfd_create(CLOCK_MONOTONIC, 0);
		if (tmfd < 0)
			error(1, errno, "handle_requests/timerfd_create");
		/* Silently exit after 30s being idle. */
		its.it_value.tv_sec = 30;
		its.it_value.tv_nsec = 0;
		its.it_interval.tv_sec = 30;
		its.it_interval.tv_nsec = 0;
		timerfd_settime(tmfd, 0, &its, NULL);
		FD_SET(tmfd, &refset);
	}

	for (;;) {
		set = refset;
		ret = select(FD_SETSIZE, &set, NULL, NULL, NULL);
		if (ret < 0)
			error(1, errno, "handle_requests/select");
		if (FD_ISSET(sockfd, &set)) {
			s = accept(sockfd, NULL, 0);
			if (s < 0)
				error(1, errno, "handle_requests/accept");
			ret = register_client(s);
			if (ret) {
				close(s);
				continue;
			}
			FD_SET(s, &refset);
			if (!linger)
				timerfd_settime(tmfd, 0, &its, NULL);
		}
		if (!linger && FD_ISSET(tmfd, &set)) {
			ret = read(tmfd, &exp, sizeof(exp));
			(void)ret;
			if (pvlist_empty(&client_list)) {
				delete_system_fs();
				exit(0);
			}
		}
		for (s = sockfd + 1; s < FD_SETSIZE; s++) {
			if (!FD_ISSET(s, &set) || linger || s == tmfd)
				continue;
			ret = recv(s, &c, sizeof(c), 0);
			if (ret <= 0) {
				unregister_client(s);
				close(s);
				FD_CLR(s, &refset);
			}
		}
	}
}

static void cleanup_handler(int sig)
{
	delete_system_fs();
	_exit(1);
}

static void create_system_fs(const char *arg0, const char *rootdir)
{
	struct sysreg_fsfile *f;
	struct sysreg_fsdir *d;
	struct sigaction sa;
	const char *session;
	char *mountpt;
	int ret;

	session = strrchr(rootdir, '/');
	if (session++ == NULL)
		error(1, EINVAL, "root directory %s", rootdir);

	ret = asprintf(&mountpt, "%s/system", rootdir);
	if (ret < 0)
		error(1, ENOMEM, "malloc");

	ret = create_directory_recursive(mountpt);
	if (ret)
		error(1, -ret, "create_directory_recursive(\"%s\")", mountpt);

	CPU_ZERO(&__node_info.cpu_affinity);
	__node_info.session_label = session;
	__node_info.registry_root = rootdir;
	sysroot = mountpt;
	copperplate_bootstrap_minimal(arg0, mountpt);
	atexit(delete_system_fs);

	note("mounted system fs at %s", mountpt);

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = cleanup_handler;
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGHUP, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);

	for (d = sysreg_dirs; d->path != NULL; d++)
		registry_add_dir(d->path);

	for (f = sysreg_files; f->path != NULL; f++) {
		registry_init_file(&f->fsobj, &f->ops, 0);
		ret = registry_add_file(&f->fsobj, f->mode, f->path);
		if (ret)
			error(1, -ret, "failed to register %s", f->path);
	}
}

int main(int argc, char *const *argv)
{
	int lindex, opt, ret;
	struct sigaction sa;

	for (;;) {
		lindex = -1;
		opt = getopt_long_only(argc, argv, "", options, &lindex);
		if (opt == EOF)
			break;
		switch (lindex) {
		case help_opt:
			usage();
			return 0;
		case daemonize_opt:
		case linger_opt:
			break;
		case root_opt:
			rootdir = optarg;
			break;
		default:
			usage();
			exit(1);
		}
	}

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_IGN;
	sigaction(SIGCHLD, &sa, NULL);
	sigaction(SIGPIPE, &sa, NULL);

	if (daemonize) {
		ret = daemon(1, 1);
		if (ret)
			error(1, errno, "cannot daemonize");
	}

	create_rootdir();
	bind_socket();
	create_system_fs(argv[0], rootdir);
	handle_requests();

	return 0;
}
