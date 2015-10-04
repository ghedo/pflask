/*
 * The process in the flask.
 *
 * Copyright (c) 2013, Alessandro Ghedini
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <syslog.h>

#include <sys/syscall.h>

#include <linux/sched.h>

#include <getopt.h>

#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "capabilities.h"
#include "cmdline.h"

#include "pty.h"
#include "user.h"
#include "dev.h"
#include "machine.h"
#include "mount.h"
#include "cgroup.h"
#include "netif.h"
#include "sync.h"
#include "printf.h"
#include "util.h"

static size_t validate_optlist(const char *name, const char *opts);

static void do_daemonize(void);
static void do_chroot(const char *dest);
static pid_t do_clone(int *flags);
static void memory_cleanup(void);

struct cap_action *caps = NULL;

int main(int argc, char *argv[]) {
	int rc, sync[2];

	pid_t pid = -1;

	siginfo_t status;

	struct mount *mounts = NULL;
	struct netif *netifs = NULL;
	struct cgroup *cgroups = NULL;
	struct user *users = NULL;

	char *cap;
#ifdef HAVE_LIBCAP_NG
	char *name;
	bool clear_caps = false;
	size_t total_caps = 0;
	struct cap_action parsed_cap;
#endif

	char *master;
	_close_ int master_fd = -1;

	char ephemeral_dir[] = "/tmp/pflask-ephemeral-XXXXXX";

	int clone_flags = CLONE_NEWNS  |
                          CLONE_NEWIPC |
                          CLONE_NEWPID |
                          CLONE_NEWUTS;

	struct gengetopt_args_info args;

	if (cmdline_parser(argc, argv, &args) != 0)
		return 1;

	for (unsigned int i = 0; i < args.mount_given; i++) {
		validate_optlist("--mount", args.mount_arg[i]);
		mount_add_from_spec(&mounts, args.mount_arg[i]);
	}

	for (unsigned int i = 0; i < args.netif_given; i++) {
		clone_flags |= CLONE_NEWNET;

		if (args.netif_arg != NULL) {
			validate_optlist("--netif", args.netif_arg[i]);
			netif_add_from_spec(&netifs, args.netif_arg[i]);
		}
	}

	if (args.user_given && !args.user_map_given) {
		uid_t uid;
		gid_t gid;

		clone_flags |= CLONE_NEWUSER;

		if (user_get_uid_gid(args.user_arg, &uid, &gid)) {
			user_add_map(&users, 'u', uid, uid, 1);
			user_add_map(&users, 'g', gid, gid, 1);
		}
	}

	for (unsigned int i = 0; i < args.user_map_given; i++) {
		size_t count;
		uid_t id, host_id;

		char *start = args.user_map_arg[i], *end = NULL;

		validate_optlist("--user-map", args.user_map_arg[i]);

		clone_flags |= CLONE_NEWUSER;

		id = strtoul(start, &end, 10);
		if (*end != ':')
			fail_printf("Invalid value '%s' for --user-map",
			            args.user_map_arg[i]);

		start = end + 1;

		host_id = strtoul(start, &end, 10);
		if (*end != ':')
			fail_printf("Invalid value '%s' for --user-map",
			            args.user_map_arg[i]);

		start = end + 1;

		count = strtoul(start, &end, 10);
		if (*end != '\0')
			fail_printf("Invalid value '%s' for --user-map",
			            args.user_map_arg[i]);

		user_add_map(&users, 'u', id, host_id, count);
		user_add_map(&users, 'g', id, host_id, count);
	}

	for (unsigned int i = 0; i < args.caps_given; i++) {
		cap = args.caps_arg[i];

		fail_if(strlen(cap) == 0, "Empty capability name specified");

		if (i == 0) {
			if (!strcasecmp(cap, "+all") || !strcasecmp(cap, "all")) {
				// nop

				continue;
			} else if (!strcasecmp(cap, "-all")) {
#ifndef HAVE_LIBCAP_NG
				fail_printf("No capabilities support built-in");
#else
				clear_caps = true;

				continue;
#endif
			}
		}

#ifndef HAVE_LIBCAP_NG
		fail_printf("No capabilities support built-in");
#else
		if (cap[0] == '+') {
			parsed_cap.action = CAPNG_ADD;

			name = &cap[1];
		} else if (cap[0] == '-') {
			parsed_cap.action = CAPNG_DROP;

			name = &cap[1];
		} else {
			// implicit '+'
			parsed_cap.action = CAPNG_ADD;

			name = cap;
		}

		if (i != 0) {
			// check for alias after prefix removal
			if (!strcasecmp(name, "all")) {
				fail_printf("Alias '%s' is valid only as first capability", cap);
			}
		}

		rc = capng_name_to_capability(name);
		fail_if(rc == -1, "Invalid capability name: '%s'", name);

		parsed_cap.capability = rc;

		// reallocate and copy
		caps = realloc(caps, ++total_caps);
		caps[total_caps-1] = parsed_cap;
#endif
	}

	// release allocated buffers at exit
	atexit(memory_cleanup);

	for (unsigned int i = 0; i < args.cgroup_given; i++)
		cgroup_add(&cgroups, args.cgroup_arg[i]);

	if (args.no_userns_flag)
		clone_flags &= ~(CLONE_NEWUSER);

	if (args.no_mountns_flag)
		clone_flags &= ~(CLONE_NEWNS);

	if (args.no_netns_flag)
		clone_flags &= ~(CLONE_NEWNET);

	if (args.no_ipcns_flag)
		clone_flags &= ~(CLONE_NEWIPC);

	if (args.no_utsns_flag)
		clone_flags &= ~(CLONE_NEWUTS);

	if (args.no_pidns_flag)
		clone_flags &= ~(CLONE_NEWPID);

	if (args.attach_given) {
		master_fd = recv_pty(args.attach_arg);
		fail_if(master_fd < 0, "Invalid PID '%u'", args.attach_arg);

		process_pty(master_fd);
		return 0;
	}

	open_master_pty(&master_fd, &master);

	if (args.detach_flag)
		do_daemonize();

	sync_init(sync);

	if (args.ephemeral_flag) {
		if (!mkdtemp(ephemeral_dir))
			sysf_printf("mkdtemp()");
	}

	pid = do_clone(&clone_flags);

	if (!pid) {
		closep(&master_fd);

		rc = prctl(PR_SET_PDEATHSIG, SIGKILL);
		sys_fail_if(rc < 0, "prctl(PR_SET_PDEATHSIG)");

		rc = setsid();
		sys_fail_if(rc < 0, "setsid()");

		sync_barrier_parent(sync, SYNC_START);

		sync_close(sync);

		open_slave_pty(master);

		setup_user(args.user_arg);

		if (args.hostname_given) {
			rc = sethostname(args.hostname_arg,
			                 strlen(args.hostname_arg));
			sys_fail_if(rc < 0, "Error setting hostname");
		}

		setup_mount(mounts, args.chroot_arg, args.ephemeral_flag ?
		                                       ephemeral_dir : NULL);

		if (args.chroot_given) {
			setup_nodes(args.chroot_arg);

			setup_ptmx(args.chroot_arg);

			setup_symlinks(args.chroot_arg);

			setup_console(args.chroot_arg, master);

			do_chroot(args.chroot_arg);
		}

		if (clone_flags & CLONE_NEWNET)
			config_netif();

		umask(0022);

#if HAVE_LIBCAP_NG
		setup_capabilities(clear_caps, total_caps, caps);
#endif

		if (args.chdir_given) {
			rc = chdir(args.chdir_arg);
			sys_fail_if(rc < 0, "Error changing cwd");
		}

		if (args.chroot_given) {
			char *term = getenv("TERM");

			if (!args.keepenv_flag)
				clearenv();

			setenv("PATH", "/usr/sbin:/usr/bin:/sbin:/bin", 1);
			setenv("USER", args.user_arg, 1);
			setenv("LOGNAME", args.user_arg, 1);
			setenv("TERM", term, 1);
		}

		for (unsigned int i = 0; i < args.setenv_given; i++) {
			rc = putenv(strdup(args.setenv_arg[i]));
			sys_fail_if(rc != 0, "Error setting environment");
		}

		setenv("container", "pflask", 1);

		if (argc > optind)
			rc = execvpe(argv[optind], argv + optind, environ);
		else
			rc = execle("/bin/bash", "-bash", NULL, environ);

		sys_fail_if(rc < 0, "Error executing command");
	}

	sync_wait_child(sync, SYNC_START);

	if (args.chroot_given && (clone_flags & CLONE_NEWUSER))
		setup_console_owner(master, users);

	setup_cgroup(cgroups, pid);

	setup_netif(netifs, pid);

#ifdef HAVE_DBUS
	register_machine(pid, args.chroot_given ? args.chroot_arg : "");
#endif

	if (clone_flags & CLONE_NEWUSER)
		setup_user_map(users, pid);

	sync_wake_child(sync, SYNC_DONE);

	sync_close(sync);

	if (args.detach_flag)
		serve_pty(master_fd);
	else
		process_pty(master_fd);

	kill(pid, SIGKILL);

	rc = waitid(P_PID, pid, &status, WEXITED);
	sys_fail_if(rc < 0, "Error waiting for child");

	switch (status.si_code) {
	case CLD_EXITED:
		if (status.si_status != 0)
			err_printf("Child failed with code '%d'",
			           status.si_status);
		else
			ok_printf("Child exited");
		break;

	case CLD_KILLED:
		err_printf("Child was terminated");
		break;

	default:
		err_printf("Child failed");
		break;
	}

	sync_close(sync);

	clean_cgroup(cgroups);

	if (args.ephemeral_flag) {
		rc = rmdir(ephemeral_dir);
		sys_fail_if(rc != 0, "Error deleting ephemeral directory: %s",
		                     ephemeral_dir);
	}

	return status.si_status;
}

static void memory_cleanup() {
	if (caps != NULL) {
		free(caps);
	}
}

static size_t validate_optlist(const char *name, const char *opts) {
	size_t i, c;
	_free_ char **vars = NULL;

	_free_ char *tmp = NULL;

	if (!opts || !*opts)
		return 0;

	tmp = strdup(opts);
	fail_if(!tmp, "OOM");

	c = split_str(tmp, &vars, ":");
	fail_if(!c, "Invalid value '%s' for %s", opts, name);

	for (i = 0; i < c; i++) {
		if (vars[i] == '\0')
			fail_printf("Invalid value '%s' for %s", opts, name);
	}

	return c;
}

static void do_daemonize(void) {
	int rc;

	openlog("pflask", LOG_NDELAY | LOG_PID, LOG_DAEMON);
	use_syslog = 1;

	rc = daemon(0, 0);
	sys_fail_if(rc < 0, "Error daemonizing");
}

static void do_chroot(const char *dest) {
	int rc;

	rc = chdir(dest);
	sys_fail_if(rc < 0, "chdir()");

	rc = chroot(".");
	sys_fail_if(rc < 0, "Error chrooting");

	rc = chdir("/");
	sys_fail_if(rc < 0, "chdir(/)");
}

static pid_t do_clone(int *flags) {
	pid_t pid;

	*flags |= SIGCHLD;

	pid = syscall(__NR_clone, *flags, NULL);
	if (pid < 0) {
		if (errno == EINVAL) {
			*flags &= ~(CLONE_NEWUSER);
			pid = syscall(__NR_clone, *flags, NULL);
		}
	}

	sys_fail_if(pid < 0, "Error cloning process");

	return pid;
}
